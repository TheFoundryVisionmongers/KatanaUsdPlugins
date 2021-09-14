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

import UsdKatana

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
    C{Usd.Prim}.

    @type stage: C{Usd.Stage}
    @type lightSdfPath: C{Sdf.Path}
    @type materialAttrs: C{FnAttribute.GroupAttribute}
    @rtype: C{UsdLux.Light} or C{None}
    @param stage: The USD stage to write the light to.
    @param lightSdfPath: The path to write the C{UsdLux.Light} to.
    @param materialAttrs: The ``light`` attribute from Katana's
        scenegraph for the light location.
    @return: The C{UsdLux.Light} created by this function or C{None} if
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
            elif isinstance(propertySpec, Sdf.RelationshipSpec):
                # Light/shadow linking & filters should be here.
                pass

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

    lightShaderName = ""
    lightShaderAttrs = None
    for lightAttrName, lightAttr in materialAttrs.childList():
        if lightAttrName.endswith("LightShader"):
            shaderNameAttrData = lightAttr.getData()
            if not shaderNameAttrData:
                continue
            lightShaderName = shaderNameAttrData[0]

        elif lightAttrName.endswith("LightParams"):
            lightShaderAttrs = lightAttr

    if not lightShaderName or not lightShaderAttrs:
        log.warning("Failed to write light: missing required attributes.")
        return None

    if not lightShaderName.startswith("UsdLux"):
        # This is only temporary while third party light export does not work.
        log.warning("Shader '%s' is not a UsdLux shader type, skipping bake "
                    "for this light.", lightShaderName)
        return None

    primType = _ResolveLightPrim(lightShaderName)
    if not primType:
        log.warning("Unable to resolve prim type for shader '%s'.",
                    lightShaderName)
        return None

    lightPrim = stage.DefinePrim(lightSdfPath, primType)
    shapingAPI = UsdLux.ShapingAPI(lightPrim)
    shadowAPI = UsdLux.ShadowAPI(lightPrim)
    _ApplyKatanaLightAPI(lightPrim, lightShaderName)

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

    shapingAPI.Apply(lightPrim)
    shadowAPI.Apply(lightPrim)

    # Process shader parameters.
    for lightParamName, lightParamAttr in lightShaderAttrs.childList():
        knownParamMatches = [i for i in knownLightProperties.keys() \
            if compareFnAttrToUsdParam(lightParamName, i)]
        if len(knownParamMatches) > 0:
            lightPrim = _WriteParameter(lightPrim,
                                        knownParamMatches[0],
                                        knownLightProperties[knownParamMatches[0]],
                                        lightParamAttr)
        else:
            lightPrim = _WriteCustomParameter(lightPrim,
                                                lightParamName,
                                                lightParamAttr)

    return lightPrim

def _ApplyKatanaLightAPI(prim, shaderName):
    lightApi = UsdKatana.LightAPI(prim)
    lightApi.Apply(prim)
    lightApi.CreateIdAttr([shaderName])

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

def _ResolveLightPrim(shaderName):
    schemaType = None

    lightType = Tf.Type.FindByName(shaderName)
    if not lightType:
        log.debug("No type for '%s'.", shaderName)
    else:
        schemaType = Usd.SchemaRegistry().GetConcreteSchemaTypeName(lightType)
        if not schemaType:
            log.debug("No schema for '%s'.", shaderName)
        else:
            return schemaType

    return _FallbackBaseLightTokenParse(shaderName)

def _FallbackBaseLightTokenParse(shaderName):
    lightTypeToKnownLightNames = {
            "CylinderLight": ["cylinder"],
            "DiskLight": ["disk", "spot", "photometric"],
            "DistantLight": ["dist", "portal"],
            "DomeLight": ["dome", "env"],
            "RectLight": ["rect", "quad"],
            "SphereLight": ["sphere", "point"]}
    for lightType, names in lightTypeToKnownLightNames.items():
        for name in names:
            if name in shaderName.lower():
                return lightType

    return None

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
    if prim is None:
        prim = stage.GetDefaultPrim()
    luxListAPI = UsdLux.ListAPI(prim)
    luxListAPI.Apply(prim)
    listList = luxListAPI.ComputeLightList(
        UsdLux.ListAPI.ComputeModeIgnoreCache)
    luxListAPI.StoreLightList(listList)

