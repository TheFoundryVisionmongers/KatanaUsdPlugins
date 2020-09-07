# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

pxr_katana_python_plugin(
    MODULE_NAME USD.UsdMaterialBakeSuperTool.bundle
    PYTHON_PLUGIN_REGISTRY_FILES
        UsdMaterialBake/__init__.py
        UsdMaterialBake/Editor.py
        UsdMaterialBake/Node.py
        UsdMaterialBake/VariantsWidget.py
    PYTHON_MODULE_FILES
    PLUGIN_TYPE SuperTools
)
