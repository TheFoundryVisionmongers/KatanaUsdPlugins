set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macros)

# Ensure we can get the root of this project at all times.
set(KATANA_USD_PLUGINS_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include(Support)
if(NOT GEOLIB_API_SOURCE_DIR)
    set(GEOLIB_API_SOURCE_DIR ${KATANA_API_SOURCE_DIR})
endif()

message(STATUS "KATANA_API_SOURCE_DIR is ${KATANA_API_SOURCE_DIR}")
message(STATUS "GEOLIB_API_SOURCE_DIR is ${GEOLIB_API_SOURCE_DIR}")

set(PXR_INSTALL_SUBDIR "third_party/katana")

# Transform the KATANA_API versions into defines to be used by this code
add_definitions(
    -DPX_KATANA_ASSET_API_VER=${KATANA_API_ASSET_API_VERSION}
    -DPX_KATANA_AMP_API_VER=${KATANA_API_AMP_API_VERSION}
    -DPX_KATANA_SGG_API_VER=${KATANA_API_SGG_API_VERSION}
    -DPX_KATANA_RENDER_API_VER=${KATANA_API_RENDER_API_VERSION}
    -DPX_KATANA_VMP_API_VER=${KATANA_API_VMP_API_VERSION}
    -DPX_KATANA_OP_API_VER=${KATANA_API_OP_API_VERSION}
    -DPX_KATANA_ATTRFNC_API_VER=${KATANA_API_ATTRFNC_API_VERSION}
    )

add_subdirectory(lib)
add_subdirectory(plugin)
add_subdirectory(python)
