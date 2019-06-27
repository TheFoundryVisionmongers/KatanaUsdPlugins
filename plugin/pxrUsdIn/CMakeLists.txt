set(PXR_PACKAGE pxrUsdIn)

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        usdKatana
        katanaOpApi
        katanaPluginApi
        usd
        usdLux
        usdUtils
        ${Boost_SYSTEM_LIBRARY}

    CPPFILES
        pxrUsdIn.cpp
)
