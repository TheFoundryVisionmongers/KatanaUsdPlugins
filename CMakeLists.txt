project(usd_for_katana)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
set(INSTALL_SUBDIR "third_party/katana")
set(INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${INSTALL_SUBDIR})

# Enable CMake testing.
if(BUILD_TESTS)
    enable_testing()
endif()

# Find required libraries.
find_package(KatanaAPI REQUIRED)
find_package(TBB COMPONENTS tbb REQUIRED)
add_definitions(${TBB_DEFINITIONS})
find_package(PythonLibs 2.7 REQUIRED)
find_package(Boost COMPONENTS thread REQUIRED)

# Validate header and library directories.
if(NOT USD_INCLUDE_DIR AND USD_ROOT)
    set(USD_INCLUDE_DIR "${USD_ROOT}/include")
endif()
if(NOT USD_LIBRARY_DIR AND USD_ROOT)
    set(USD_LIBRARY_DIR "${USD_ROOT}/lib")
endif()
if(NOT DEFINED USD_INCLUDE_DIR)
    message(SEND_ERROR "USD_INCLUDE_DIR is required")
    return()
endif()
if(NOT DEFINED USD_LIBRARY_DIR)
    message(SEND_ERROR "USD_LIBRARY_DIR is required")
    return()
endif()

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

link_directories(${USD_LIBRARY_DIR})

include_directories(SYSTEM ${USD_INCLUDE_DIR})
include_directories(SYSTEM ${TBB_INCLUDE_DIRS})
include_directories(SYSTEM ${PYTHON_INCLUDE_DIR})

include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/lib/")
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/plugin/")

add_subdirectory(lib)
add_subdirectory(plugin)
add_subdirectory(python)
