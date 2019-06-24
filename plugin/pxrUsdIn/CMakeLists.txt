set(PXR_PACKAGE pxrUsdIn)

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        usdKatana
        vtKatana
        katanaOpApi
        katanaPluginApi
        usd
        usdLux
        usdUtils
        usdGeom
        tf
        sdf
        vt
        arch
        ${Boost_PYTHON_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}

    CPPFILES
        pxrUsdIn.cpp
)
