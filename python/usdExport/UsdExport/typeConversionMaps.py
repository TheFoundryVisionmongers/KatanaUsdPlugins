# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.
"""
Module containing some helpful conversion maps to assist in type conversions.
"""

import logging

log = logging.getLogger("UsdExport")
try:
    from fnpxr import Sdf
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

RenderInfoShaderTagToSdfMap = {
    "color": Sdf.ValueTypeNames.Color3f,
    "array_color": Sdf.ValueTypeNames.Color3fArray,
    "point": Sdf.ValueTypeNames.Color3fArray,
    "string": Sdf.ValueTypeNames.String,
    "double": Sdf.ValueTypeNames.Double,
    "array_float": Sdf.ValueTypeNames.DoubleArray,
    "float": Sdf.ValueTypeNames.Float,
    "array_double": Sdf.ValueTypeNames.FloatArray,
    "int": Sdf.ValueTypeNames.Int,
    "array_int": Sdf.ValueTypeNames.IntArray,
    "matrix": Sdf.ValueTypeNames.Matrix4d,
    "normal": Sdf.ValueTypeNames.Normal3f
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
        sdfType = RenderInfoShaderTagToSdfMap.get(tagToUse)
        if sdfType is not None:
            return sdfType
    return None
