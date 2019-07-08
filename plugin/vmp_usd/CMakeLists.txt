set(PXR_PACKAGE vmp_usd)

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")
    return()
endif()

set(vtKatana_LIBRARY "")
if (NOT ${KATANA_API_VERSION} VERSION_LESS "3.0.0")
    set(vtKatana_LIBRARY "vtKatana")
endif()

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        usdImagingGL
        katanaPluginApi
        usdKatana
        ${vtKatana_LIBRARY}

    CPPFILES
        debugCodes.cpp
        usdVMP.cpp
)

