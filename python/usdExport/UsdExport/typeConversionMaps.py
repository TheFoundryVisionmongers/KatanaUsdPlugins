# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

import logging
import os

log = logging.getLogger("UsdExport")
try:
    from fnpxr import Gf, Sdf 
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

valueTypeCastMethods = {
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


def convertRenderInfoShaderTagsToSdfType(tags):
    if isinstance(tags, list):
        tags_list = tags
    else:
        tags_list = tags.split(" or ")
    if tags_list:
        tagToUse = tags_list[0]
        for tag in tags_list:
            if "array_" in tag:
                tagToUse = tag
        sdfType = RenderInfoShaderTagToSdfMap.get(tagToUse)
        if sdfType is not None:
            return sdfType
