set(PXR_PACKAGE usdInPrman)

set(vtKatana_LIBRARY "")
if (NOT ${KATANA_API_VERSION} VERSION_LESS "3.0.0")
    set(vtKatana_LIBRARY "vtKatana")
endif()

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        katanaPluginApi
        usd
        usdGeom
        usdKatana
        usdLux
        usdRi
        ${vtKatana_LIBRARY}

    PRIVATE_HEADERS
        declarePackageOps.h

    CPPFILES
        register.cpp
        prmanGeometryDecorator.cpp
        prmanLightDecorator.cpp
)
