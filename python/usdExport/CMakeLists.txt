# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

pxr_katana_python_plugin(
    MODULE_NAME USD.USDExport.bundle
    PYTHON_FILES
        usdExport.py
        UsdExport/typeConversionMaps.py
        UsdExport/__init__.py
        UsdExport/material.py
    PLUGIN_TYPE Plugins
)

add_subdirectory(SuperTools)
