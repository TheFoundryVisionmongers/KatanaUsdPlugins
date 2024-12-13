# Copyright (c) 2021 The Foundry Visionmongers Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
# names, trademarks, service marks, or product names of the Licensor
# and its affiliates, except as required to comply with Section 4(c) of
# the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.

import logging

from fnpxr import Sdr

from Katana import FnAttribute

from UsdExport.common import (GetShaderNodeFromRegistry)

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from pxr import Usd, UsdLux, Sdf, Tf
    # These includes also require fnpxr
    from .typeConversionMaps import (FnAttributeToSdfType,
                                     ConvertParameterValueToGfType)
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', str(e))

rendererPrefixMapping = {
    "prman" : "ri",
    "dl": "nsi"
}

def WriteLight(stage, lightSdfPath, materialAttrs):
    """
    Converts the given light material C{GroupAttribute} into a
    C{Usd.Prim}. If the C{Sdf.Path} does not exist, it will be created. Any
    existing prim type will be overwritten with with either the corresponding
    UsdLux light type or 'Light' if a non-UsdLux light type is being written.

    @type stage: C{Usd.Stage}
    @type lightSdfPath: C{Sdf.Path}
    @type materialAttrs: C{FnAttribute.GroupAttribute}
    @rtype: C{Usd.Prim}
    @param stage: The USD stage to write the light to.
    @param lightSdfPath: The path to write the C{UsdLux.Light} to.
    @param materialAttrs: The ``light`` attribute from Katana's
        scenegraph for the light location.
    @return: The C{Usd.Prim} created by this function
    """
    # pylint: disable=too-many-locals

    def compareFnAttrToUsdParam(fnAttrName, paramName):
        """
        Sanitizes the USD parameter name to match Katana's attribute name
        format so we can check for matches.
        """
        sanitizedParamName = paramName.replace("inputs:", '')
        index = sanitizedParamName.find(':')
        while index != -1 and index < len(sanitizedParamName) - 1:
            sanitizedParamName = "%s%s%s"% \
                                 (sanitizedParamName[:index], \
                                  sanitizedParamName[index + 1].upper(),
                                  sanitizedParamName[index + 2:])
            index = sanitizedParamName.find(':')

        return sanitizedParamName == fnAttrName

    if not materialAttrs:
        return

    lightPrim = stage.DefinePrim(lightSdfPath)
    UsdLux.ShadowAPI.Apply(lightPrim)
    UsdLux.ShapingAPI.Apply(lightPrim)

    lights = ParseLightsFromMaterialAttrs(materialAttrs)
    for lightShaderName, lightShaderAttrs in lights.items():
        (renderer, lightShaderName) = lightShaderName

        node = None
        # Attempt to get the correct shader name. This resolves issues such as
        # "UsdLuxRectLight" being "RectLight" in the Sdr registry.
        shaderTypeName = Tf.Type.FindByName(lightShaderName)
        if shaderTypeName == Tf.Type.Unknown:
            node = GetShaderNodeFromRegistry(lightShaderName)
        else:
            lightShaderName = \
                Usd.SchemaRegistry.GetSchemaTypeName(shaderTypeName)
            node = GetShaderNodeFromRegistry(lightShaderName)

        # Only continue if the shader exists in the Sdr Registry.
        if node is None:
            continue

        primTypeName = _ResolveLightPrimType(lightShaderName)
        if primTypeName:
            lightPrim.SetTypeName(primTypeName)

        renderer = rendererPrefixMapping.get(renderer, renderer)

        for attrName, attr in lightShaderAttrs.childList():
            nodeInput = node.GetInput(attrName)

            # If we cannot find an input name directly, we must take some extra
            # steps.
            if nodeInput is None:
                for tmpInputName in node.GetInputNames():
                    tmpInput = node.GetInput(tmpInputName)
                    # Check the attribute name against the implementation name.
                    if attrName == tmpInput.GetImplementationName():
                        attrName = tmpInputName
                        nodeInput = tmpInput
                        break
                    # Check the attribute name against a sanitized version of
                    # the input name.
                    if compareFnAttrToUsdParam(attrName, tmpInput.GetName()):
                        attrName = tmpInputName
                        nodeInput = tmpInput
                        break

            if nodeInput is None:
                _WriteCustomParameter(lightPrim, attrName, attr)
                continue

            if renderer != "usd":
                primAttr = _CreatePrefixedAttribute(
                    lightPrim,
                    nodeInput,
                    renderer,
                    node.GetContext())
            else:
                primAttr = _CreatePrefixedAttribute(
                    lightPrim,
                    nodeInput)

            attrValue = attr.getData()
            if len(attrValue) > 1:
                attrValue = tuple(attrValue)
            elif len(attrValue) == 1:
                attrValue = attrValue[0]
            else:
                continue

            primAttr.Set(attrValue)

    return lightPrim

def WriteLightList(stage, prim=None):
    """
    Computes the light list on the given stage with compute mode of
    `UsdLux.ListAPI.ComputeModeIgnoreCache`. The light list will be written
    to the provided prim, or the stage's default prim if not provided.

    @type stage: C{Usd.Stage}
    @type prim: C{Usd.Prim}
    @param stage: The C{Usd.Stage} to write data to.
    @param prim: Optional prim to write the Usd List to, if not provided,
        the stage's default prim will be used.
    """
    flags = (Usd.PrimIsActive | Usd.PrimIsDefined | ~Usd.PrimIsAbstract)
    lightList = []
    def traverse(prim):
        if  prim.HasAPI(UsdLux.LightAPI) or \
            prim.IsA(UsdLux.LightFilter) or \
            prim.GetTypeName() == "Light":
            lightList.append(prim.GetPath())

        for child in prim.GetFilteredChildren(Usd.TraverseInstanceProxies(
            flags)):
            traverse(child)

    if prim is None:
        prim = stage.GetDefaultPrim()

    luxListAPI = UsdLux.ListAPI(prim)
    traverse(prim)
    luxListAPI.StoreLightList(lightList)
    luxListAPI.CreateLightListCacheBehaviorAttr("consumeAndHalt")
    luxListAPI.Apply(prim)

def ParseLightsFromMaterialAttrs(materialAttrs):
    """
    Creates a dictionary mapping shaders to light attributes for the given
    material attributes. The dictionary key is a tuple of C{str} for the
    renderer name and C{str} for the shader name, for example
    `("prman", "PxrDiskLight)` or `("usd", "UsdLuxRectLight)`

    @type materialAttrs: C{FnAttribute.GroupAttribute}
    @rtype: C{dict} of (C{tuple} of C{str}) : C{FnAttribute.GroupAttribute}
    @param materialAttrs: The material group attribute to read from.
    @return: A dictionary mapping the renderer prefixed shader name to the
        attributes for that light.
    """
    lights = {}
    if materialAttrs:
        for lightAttrName, lightAttr in materialAttrs.childList():
            if lightAttrName.endswith("LightShader"):
                renderer = lightAttrName.split("LightShader")[0]
                lightParams = \
                    materialAttrs.getChildByName(renderer + "LightParams")
                if not lightParams:
                    lightParams = FnAttribute.GroupAttribute()
                shaderName = lightAttr.getData()
                if not shaderName:
                    continue
                lights[(renderer, shaderName[0])] = lightParams

    return lights

def _CreatePrefixedAttribute(prim, nodeInput, renderer = "", context = ""):
    """
    Gets the node from the Sdr Registry for a given light shader on a light
    location and writes attributes matching inputs to that node with the
    correct type as specified in that input.

    @type prim: C{Usd.Prim}
    @type nodeInput: C{Sdr.ShaderProperty}
    @type renderer: C{str} or C{None}
    @type context: C{str} or C{None}
    @rtype: C{Usd.Attribute}
    @param prim: The prim to create the attribute on.
    @param nodeInput: The Sdr node input for this attribute.
    @param renderer: The renderer name which prefixes the shader.
    @param context: The context of the attribute, typically "light".
    @return: The created Usd Attribute.
    """
    rendererPrefix = rendererPrefixMapping.get(
        renderer, renderer)

    attributeName = ""
    if rendererPrefix:
        attributeName += rendererPrefix + ":"
    if context:
        attributeName += context + ":"
    if nodeInput:
        nodeInputName = nodeInput.GetName()
        if nodeInputName.startswith(attributeName):
            attributeName = nodeInputName
        else:
            attributeName += nodeInputName
    attributeName = f"inputs:{attributeName}"

    attr = prim.CreateAttribute(
        attributeName, nodeInput.GetTypeAsSdfType()[0])
    if nodeInput:
        attr.SetCustom(False)
    return attr

def _WriteCustomParameter(prim, paramName, fnAttr, paramType = None):
    if not isinstance(paramType, Sdf.ValueTypeName):
        paramType = FnAttributeToSdfType.get(type(fnAttr),
                                             Sdf.ValueTypeNames.Token)

    inputAttr = prim.CreateAttribute(paramName, paramType)
    gfValue = ConvertParameterValueToGfType(fnAttr.getData(), paramType)
    inputAttr.Set(gfValue)

def _ResolveLightPrimType(shaderName):
    # Try to retrive from Sdr node metadata.
    node = Sdr.Registry().GetNodeByName(shaderName)
    if node:
        typedSchema = node.GetMetadata().get("typedSchemaForAttrPruning", None)
        if typedSchema and Usd.SchemaRegistry().IsConcrete(typedSchema):
            return typedSchema

    # Try to retrieve directly from the shader name.
    if Usd.SchemaRegistry().IsConcrete(shaderName):
        return shaderName
    # Try tp retrieve from a concrete schema name.
    shaderTypeName = Tf.Type.FindByName(shaderName)
    if shaderTypeName:
        concreteSchemaTypeName = \
            Usd.SchemaRegistry().GetConcreteSchemaTypeName(shaderTypeName)
        if Usd.SchemaRegistry().IsConcrete(concreteSchemaTypeName):
            return concreteSchemaTypeName

    return None
