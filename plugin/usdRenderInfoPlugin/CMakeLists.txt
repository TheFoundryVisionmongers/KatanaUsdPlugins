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
