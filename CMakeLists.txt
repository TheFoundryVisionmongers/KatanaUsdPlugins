cmake_minimum_required(VERSION 3.12)
project(KatanaUsdPlugins)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macros)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/build-utils.cmake)

# Language standard is C++17.
set(CMAKE_CXX_STANDARD 17)

#Set default option value in case PXR_PY_PACKAGE_NAME is not defined
set(PXR_PY_PACKAGE_NAME "pxr"
    CACHE
    STRING
    "Name for the root python bindings package"
)

# Ensure we can get the root of this project at all times.
set(KATANA_USD_PLUGINS_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include(Support)
if(NOT GEOLIB_API_SOURCE_DIR)
    set(GEOLIB_API_SOURCE_DIR ${KATANA_API_SOURCE_DIR})
endif()

message(STATUS "KATANA_API_SOURCE_DIR is ${KATANA_API_SOURCE_DIR}")
message(STATUS "GEOLIB_API_SOURCE_DIR is ${GEOLIB_API_SOURCE_DIR}")

option(ENABLE_USD_EXPORT "Enables building and installing the \
    UsdExport subdirectory and supporting logic." ON)

option(ENABLE_USD_RENDER_INFO_PLUGIN "Enables building and installing the \
    UsdRenderInfoPlugin subdirectory and supporting logic." ON)

option(ENABLE_KATANAUSD_PYTHON_PLUGINS "Enabled building features which \
    require the Python modules from USD, these include UsdExport and the \
    usdKatana python module" ON)

option(USD_USING_CMAKE_THIRDPARTY_TARGET_DEPENDENCIES "When Enabled this \
    uses find_package on OpenEXR OpenImageIO and OpenSubdiv which are \
    required when linking to some of the USD libraries we use." OFF)

if(USE_KATANA_THIRDPARTY_LIBS)
    set(USD_USING_CMAKE_THIRDPARTY_TARGET_DEPENDENCIES OFF)
endif()

if(NOT ENABLE_KATANAUSD_PYTHON_PLUGINS)
    set(ENABLE_USD_EXPORT OFF)
endif()

if(MSVC)
    add_definitions(-DHAVE_SNPRINTF=ON)
endif()
add_subdirectory(lib)
add_subdirectory(plugin)
add_subdirectory(python)
