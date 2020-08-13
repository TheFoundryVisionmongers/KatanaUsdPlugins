# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

from .Node import UsdMaterialBakeNode

def GetUsdMaterialBakeEditor():
    """Editor must be lazily loaded by this function"""
    from . import Editor
    return Editor.UsdMaterialBakeEditor

if UsdMaterialBakeNode:
    PluginRegistry = [
        ("SuperTool", 2, "UsdMaterialBake", (UsdMaterialBakeNode,
                                                GetUsdMaterialBakeEditor)),
        ]
