# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

import logging
import os

log = logging.getLogger("UsdExport")
try:
    from fnpxr import Gf, Sdf 
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

myTypeMap = {
    "Float4": Sdf.ValueTypeNames.Float4,
    "Float3": Sdf.ValueTypeNames.Float3,
    "Float2": Sdf.ValueTypeNames.Float2,
    "Float1": Sdf.ValueTypeNames.Float,
    "Float": Sdf.ValueTypeNames.Float,
    "Double4": Sdf.ValueTypeNames.Double4,
    "Double3": Sdf.ValueTypeNames.Double3,
    "Double2": Sdf.ValueTypeNames.Double2,
    "Double1": Sdf.ValueTypeNames.Double,
    "Double": Sdf.ValueTypeNames.Double,
    "String": Sdf.ValueTypeNames.String,
    "String1": Sdf.ValueTypeNames.String,
    "Int": Sdf.ValueTypeNames.Int,
    "Int1": Sdf.ValueTypeNames.Int,
    "Int2": Sdf.ValueTypeNames.Int2,
    "Int3": Sdf.ValueTypeNames.Int3,
    "Int4": Sdf.ValueTypeNames.Int4,
    "Color": Sdf.ValueTypeNames.Color3f,
    "AssetIdInput": Sdf.ValueTypeNames.Asset
}
valueTypeCastMethods = {
    Sdf.ValueTypeNames.Float4: Gf.Vec4f,
    Sdf.ValueTypeNames.Float3: Gf.Vec3f,
    Sdf.ValueTypeNames.Float2: Gf.Vec2f,
    Sdf.ValueTypeNames.Float: float,
    Sdf.ValueTypeNames.Color3f: Gf.Vec3f,
    Sdf.ValueTypeNames.Double4: Gf.Vec4d,
    Sdf.ValueTypeNames.Double3: Gf.Vec3d,
    Sdf.ValueTypeNames.Double2: Gf.Vec2d,
    Sdf.ValueTypeNames.Double: float,
    Sdf.ValueTypeNames.String: str,
    Sdf.ValueTypeNames.Asset: str,
    Sdf.ValueTypeNames.Token: str,
    Sdf.ValueTypeNames.Int4: Gf.Vec4i,
    Sdf.ValueTypeNames.Int3: Gf.Vec3i,
    Sdf.ValueTypeNames.Int2: Gf.Vec2i,
    Sdf.ValueTypeNames.Int: int,
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
