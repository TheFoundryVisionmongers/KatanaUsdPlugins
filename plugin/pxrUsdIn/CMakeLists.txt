set(PXR_PACKAGE pxrUsdIn)

set(vtKatana_LIBRARY "")
if (NOT ${KATANA_API_VERSION} VERSION_LESS "3.0.0")
    set(vtKatana_LIBRARY "vtKatana")
endif()

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        usdKatana
        katanaPluginApi
        usdGeom
        vtKatana
        Boost::system
        Boost::date_time

    CPPFILES
        pxrUsdIn.cpp
)
