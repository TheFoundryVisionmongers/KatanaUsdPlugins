set(PXR_PACKAGE UsdRenderInfo)

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        usdShade
        ndr
        sdr
        sdf
        gf
        tf
        usdKatana
        katanaPluginApi
        vtKatana
        Boost::system

    PRIVATE_HEADERS
        UsdRenderInfoPlugin.h

    CPPFILES
        main.cpp
        UsdRenderInfoPlugin.cpp
)

pxr_katana_install_plugin_resources(
    MODULE_NAME USD.UsdRenderInfoPluginShaders.bundle
    FILES
        Shaders/UsdLuxCylinderLight.args
        Shaders/UsdLuxDiskLight.args
        Shaders/UsdLuxDistantLight.args
        Shaders/UsdLuxDomeLight.args
        Shaders/UsdLuxGeometryLight.args
        Shaders/UsdLuxRectLight.args
        Shaders/UsdLuxSphereLight.args
    PLUGIN_TYPE Shaders
)
