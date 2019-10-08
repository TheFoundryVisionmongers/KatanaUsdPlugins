# This library builds source files from the Katana installation
set(PXR_PACKAGE katanaOpApi)

file(GLOB cppFiles
    ${GEOLIB_API_SOURCE_DIR}/FnAsset/client/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnAsset/plugin/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnAttribute/client/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnGeolib/op/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnGeolib/util/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnGeolibServices/client/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnLogging/client/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnPluginManager/client/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/FnPluginSystem/*.cpp
    ${GEOLIB_API_SOURCE_DIR}/pystring/.cpp
)

pxr_static_library(${PXR_PACKAGE}
    INCLUDE_DIRS
        ${KATANA_API_INCLUDE_DIRS}

    CPPFILES
        ${cppFiles}
)

