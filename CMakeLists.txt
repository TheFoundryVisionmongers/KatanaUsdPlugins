project(usd_for_katana)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
set(INSTALL_SUBDIR "third_party/katana")
set(INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${INSTALL_SUBDIR})

if(CMAKE_SYSTEM_NAME MATCHES Windows)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4305")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4624")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4506")

    add_definitions(-DTBB_USE_THREADING_TOOLS=0)
    add_definitions(-DTBB_USE_ASSERT=0)

    add_definitions(
        # Force use of the UTF16 variants of CRT and Win32 functions.
        -DUNICODE
        -D_UNICODE

        # Disable noisy and unwanted warnings about the use of standard C
        # functions.
        -D_CRT_SECURE_NO_DEPRECATE
        -D_CRT_SECURE_NO_WARNINGS
        -D_CRT_NONSTDC_NO_DEPRECATE
        -D_CRT_NONSTDC_NO_WARNINGS
        -D_SCL_SECURE_NO_DEPRECATE
        -D_SCL_SECURE_NO_WARNINGS

        # Disable min/max macros defined in windows.h
        -DNOMINMAX

        # Reduce namespace pollution when #including windows.h
        -DWIN32_LEAN_AND_MEAN
    )

    # Generate PDB files also in release.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

add_definitions(
    -DFNATTRIBUTE_STATIC=1
    -DFNPLUGINMANAGER_STATIC=1
    -DFNPLUGINSYSTEM_STATIC=1
    -DFNATTRIBUTEFUNCTION_STATIC=1
    -DFNASSET_STATIC=1
    -DFNGEOLIB_STATIC=1
    -DFNGEOLIBSERVICES_STATIC=1
    -DFNLOGGING_STATIC=1
    -DPYSTRING_STATIC=1
    -DFNATTRIBUTEMODIFIER_STATIC=1
    -DFNCONFIG_STATIC=1
    -DFNDEFAULTATTRIBUTEPRODUCER_STATIC=1
    -DFNRENDEROUTPUTLOCATION_STATIC=1
    -DFNSCENEGRAPHGENERATOR_STATIC=1
    -DFNSCENEGRAPHITERATOR_STATIC=1
    -DFNVIEWERMODIFIER_STATIC=1
)

# Enable CMake testing.
if(BUILD_TESTS)
    enable_testing()
endif()

# Disable Boost's autolinking.
add_definitions(-DBOOST_ALL_NO_LIB)

# Find required libraries.
find_package(KatanaAPI REQUIRED)
find_package(TBB COMPONENTS tbb REQUIRED)
add_definitions(${TBB_DEFINITIONS})
find_package(PythonLibs 2.7 REQUIRED)
find_package(Boost COMPONENTS python regex system thread REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

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
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/lib/")
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/plugin/")

add_subdirectory(lib)
add_subdirectory(plugin)
add_subdirectory(python)
