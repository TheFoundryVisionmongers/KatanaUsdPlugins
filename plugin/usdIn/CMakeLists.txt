set(PXR_PACKAGE usdIn)

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
        ${vtKatana_LIBRARY}
        Boost::system
        Boost::date_time

    CPPFILES
        usdIn.cpp
)
