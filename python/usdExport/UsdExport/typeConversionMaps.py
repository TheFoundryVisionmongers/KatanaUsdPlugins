# Copyright (c) 2020 The Foundry Visionmongers Ltd.
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
"""
Module containing some helpful conversion maps to assist in type conversions.
"""

import logging

import PyFnAttribute
from Katana import FnAttribute

log = logging.getLogger("UsdExport")
try:
    from pxr import  Gf, Sdf, Vt
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

ValueTypeCastMethods = {
    Sdf.ValueTypeNames.Asset: str,
    Sdf.ValueTypeNames.Bool: bool,
    Sdf.ValueTypeNames.Double: float,
    Sdf.ValueTypeNames.Float: float,
    Sdf.ValueTypeNames.Int: int,
    Sdf.ValueTypeNames.Int64: int,
    Sdf.ValueTypeNames.String: str,
    Sdf.ValueTypeNames.Token: str,
    Sdf.ValueTypeNames.UInt: int,
    Sdf.ValueTypeNames.UInt64: int,
}

FnAttributeToSdfType = {
    FnAttribute.StringAttribute: Sdf.ValueTypeNames.String,
    FnAttribute.IntAttribute: Sdf.ValueTypeNames.Int,
    FnAttribute.FloatAttribute: Sdf.ValueTypeNames.Float,
    FnAttribute.DoubleAttribute: Sdf.ValueTypeNames.Double,
}

RenderInfoShaderTagToSdfMap = {
    "color": Sdf.ValueTypeNames.Color3f,
    "rgb": Sdf.ValueTypeNames.Color3f,
    "rgba": Sdf.ValueTypeNames.Color4f,
    "array_color": Sdf.ValueTypeNames.Color3fArray,
    "point": Sdf.ValueTypeNames.Color3fArray,
    "string": Sdf.ValueTypeNames.String,
    "double": Sdf.ValueTypeNames.Double,
    "array_float": Sdf.ValueTypeNames.FloatArray,
    "float": Sdf.ValueTypeNames.Float,
    "array_double": Sdf.ValueTypeNames.DoubleArray,
    "int": Sdf.ValueTypeNames.Int,
    "array_int": Sdf.ValueTypeNames.IntArray,
    "matrix": Sdf.ValueTypeNames.Matrix4d,
    "normal": Sdf.ValueTypeNames.Normal3f,

    # These are SdrPropertyTypes that have no corresponding SdfValueType; USD
    # docs recommends just mapping them to Token:
    #   https://graphics.pixar.com/usd/docs/api/sdr_page_front.html.
    "struct": Sdf.ValueTypeNames.Token,
    "terminal": Sdf.ValueTypeNames.Token,
    "vstruct": Sdf.ValueTypeNames.Token,
}


def ConvertRenderInfoShaderTagsToSdfType(tags):
    """
    Converts the given tags from the relevant Katana RenderInfoPlugin into
    SdfTypes.

    @type tags: C{list} or C{str}
    @rtype: C{Sdf.ValueTypeNames} or C{None}
    @param tags: a list of tags, or a string of tags separated by "or".
    @return: The closest Sdf type, or None if no match can be found.
    """
    if isinstance(tags, list):
        tagsList = tags
    else:
        tagsList = tags.split(" or ")
    if tagsList:
        tagToUse = tagsList[0]
        for tag in tagsList:
            if "array_" in tag:
                tagToUse = tag
        if " or " in tagToUse:
            tagToUse = tagToUse.split(" or ")
            sdfType = ConvertRenderInfoShaderTagsToSdfType(tagToUse)
        else:
            sdfType = RenderInfoShaderTagToSdfMap.get(tagToUse)
        if sdfType is not None:
            return sdfType

    return None


def ConvertToVtVec3fArray(array):
    """
    Converts an array into Vt.Vec3fArray.

    @type array: C{List} or C{PyFnAttribute.ConstVector}
    @rtype: C{Vt.Vec3fArray}
    @param array: A array to convert to C{Vt.Vec3fArray}.
    @return: The resulting C{Vt.Vec3fArray} or an empty C{Vt.Vec3fArray}.
    """
    arraySize = len(array)
    isValid = (arraySize % 3) == 0
    if not isValid:
        log.error(
            "Couldn't convert the array to Vt.Vec3fArray because the number of"
            "elements is not divisible by 3")
        return Vt.Vec3fArray()

    newarray = []
    for i in range(0, int(arraySize / 3)):
        j = i * 3
        newarray.append((array[j], array[j + 1], array[j + 2]))
    return Vt.Vec3fArray(newarray)

def ConvertParameterValueToGfType(value, sdfType):
    """
    Converts the Katana attribute into its equivalent Gf type based on the
    C{sdfType} provided.

    @type value: C{PyFnAttribute}
    @type sdfType: C{Sdf.ValueTypeNames}
    @rtype: C{Gf}
    @param value: The value to be cast to a Gf equivalent.
    @param sdfType: The type to cast to.
    @return: The Gf type casted value.
    """
    if sdfType.type.pythonClass:
        gfCast = sdfType.type.pythonClass
    else:
        gfCast = ValueTypeCastMethods.get(sdfType)
    if gfCast:
        if isinstance(value, PyFnAttribute.ConstVector):
            # Convert Katana's PyFnAttribute.ConstVector to a python list
            if isinstance(gfCast(), Vt.Vec3fArray):
                value = ConvertToVtVec3fArray(value)
            else:
                value = [v for v in value]
        if isinstance(value, list):
            if len(value) == 1:
                value = gfCast(value[0])
            elif hasattr(gfCast, "dimension"):
                if gfCast.dimension == 2:
                    value = gfCast(value[0], value[1])
                elif gfCast.dimension == 3:
                    value = gfCast(value[0], value[1], value[2])
                elif gfCast.dimension == 4:
                    value = gfCast(value[0], value[1], value[2], value[3])
                elif gfCast.dimension == (2, 2):
                    value = gfCast(
                        value[0], value[1],
                        value[2], value[3])
                elif gfCast.dimension == (3, 3):
                    value = gfCast(
                        value[0], value[1], value[2],
                        value[3], value[4], value[5],
                        value[6], value[7], value[8])
                elif gfCast.dimension == (4, 4):
                    value = gfCast(
                        value[0], value[1], value[2], value[3],
                        value[4], value[5], value[6], value[7],
                        value[8], value[9], value[10], value[11],
                        value[12], value[13], value[14], value[15])
            elif gfCast in [Gf.Quath, Gf.Quatf, Gf.Quatd]:
                value = gfCast(value[0], value[1], value[2], value[3])
    return value
