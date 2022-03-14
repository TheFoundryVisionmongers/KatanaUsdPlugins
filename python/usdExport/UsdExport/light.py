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

from Katana import FnAttribute

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from pxr import Usd, UsdLux, Sdf, Tf
    # These includes also require fnpxr
    from .typeConversionMaps import (FnAttributeToSdfType,
                                     ConvertParameterValueToGfType)
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

def WriteLight(stage, lightSdfPath, materialAttrs):
    """
    Converts the given light material C{GroupAttribute} into a
    C{Usd.Prim}. If the C{Sdf.Path} does not exist, it will be created. Any
    existing prim type will be overwritten with with either the corresponding
    UsdLux light type or 'Light' if a non-UsdLux light type is being written.

    @type stage: C{Usd.Stage}
    @type lightSdfPath: C{Sdf.Path}
    @type materialAttrs: C{FnAttribute.GroupAttribute}
    @rtype: C{UsdLux.Light} or C{None}
    @param stage: The USD stage to write the light to.
    @param lightSdfPath: The path to write the C{UsdLux.Light} to.
    @param materialAttrs: The ``light`` attribute from Katana's
        scenegraph for the light location.
    @return: The C{Usd.Prim} created by this function or C{None} if
        no light was created.
    """
    # pylint: disable=too-many-locals
    def collateProperties(prim):
        if not isinstance(prim, Usd.PrimDefinition):
            return {}

        properties = {}
        for propertyName in prim.GetPropertyNames():
            propertySpec = prim.GetSchemaPropertySpec(propertyName)
            if isinstance(propertySpec, Sdf.AttributeSpec):
                properties[propertyName] = propertySpec.typeName

        return properties

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

    lightPrim = stage.DefinePrim(lightSdfPath, "Light")
    if not materialAttrs:
        return

    lights = ParseLightsFromMaterialAttrs(materialAttrs)
    for lightShaderName, lightShaderAttrs in lights.items():
        (renderer, lightShaderName) = lightShaderName

        if renderer != "usd":
            continue

        primType = _ResolveLightPrimType(lightShaderName)
        lightPrim = stage.DefinePrim(lightSdfPath, primType)

        shapingAPI = UsdLux.ShapingAPI(lightPrim)
        shapingAPI.Apply(lightPrim)
        shadowAPI = UsdLux.ShadowAPI(lightPrim)
        shadowAPI.Apply(lightPrim)

        knownLightProperties = {} # Maps property name to sdf type.
        # Update the known properties with properties from the lux
        # light, shaping and shadow schemas.
        lightPrimDef = \
            Usd.SchemaRegistry().FindConcretePrimDefinition(primType)
        knownLightProperties.update(collateProperties(lightPrimDef))
        knownLightProperties.update(
            collateProperties(shapingAPI.GetSchemaClassPrimDefinition()))
        knownLightProperties.update(
            collateProperties(shadowAPI.GetSchemaClassPrimDefinition()))

        # Process shader parameters.
        for lightParamName, lightParamAttr in lightShaderAttrs.childList():
            knownParamMatches = [i for i in knownLightProperties.keys() \
                if compareFnAttrToUsdParam(lightParamName, i)]
            if len(knownParamMatches) > 0:
                matchingParam = knownParamMatches[0]
                existingParamNames = lightPrim.GetAuthoredAttributes()
                if not matchingParam in existingParamNames:
                    lightPrim = _WriteParameter(lightPrim,
                                                matchingParam,
                                                knownLightProperties[matchingParam],
                                                lightParamAttr)
            else:
                lightPrim = _WriteCustomParameter(lightPrim,
                                                  lightParamName,
                                                  lightParamAttr)

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
        if  prim.IsA(UsdLux.Light) or \
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

def _WriteParameter(prim, paramName, typeName, fnAttr):
    inputAttr = prim.CreateAttribute(paramName, typeName)
    gfValue = ConvertParameterValueToGfType(fnAttr.getData(), typeName)
    inputAttr.Set(gfValue)

    return prim

def _WriteCustomParameter(prim, paramName, fnAttr, paramType = None):
    if not isinstance(paramType, Sdf.ValueTypeName):
        paramType = FnAttributeToSdfType.get(type(fnAttr),
                                             Sdf.ValueTypeNames.Token)

    return _WriteParameter(prim, paramName, paramType, fnAttr)

def _ResolveLightPrimType(shaderName):
    lightType = Tf.Type.FindByName(shaderName)
    if not lightType:
        return "Light"

    schemaType =  Usd.SchemaRegistry().GetConcreteSchemaTypeName(lightType)
    return schemaType if schemaType else "Light"
