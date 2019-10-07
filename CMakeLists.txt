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


add_subdirectory(lib)
add_subdirectory(plugin)
add_subdirectory(python)
