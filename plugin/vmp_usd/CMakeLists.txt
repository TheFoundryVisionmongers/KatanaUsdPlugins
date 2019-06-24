set(PXR_PACKAGE vmp_usd)

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")
    return()
endif()

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        arch
        tf
        work
        gf
        usd
        usdUtils
        glf
        sdf
        usdImaging
        usdImagingGL
        katanaPluginApi
        usdKatana
        vtKatana
        ${Boost_PYTHON_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${GLEW_LIBRARIES}

    CPPFILES
        debugCodes.cpp
        usdVMP.cpp
)

