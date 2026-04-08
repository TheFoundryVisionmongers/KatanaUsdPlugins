# Copyright 2020 Foundry
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
# names, trademarks, service marks, or product names of the Licensor
# and its affiliates, except as required to comply with Section 4(c) of
# the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.


macro(add_boost_interface)
    if(NOT DEFINED Python3_VERSION_MAJOR)
    message(FATL_ERROR "Unable to read Python3_VERSION_MAJOR from Python "
        "FindPackage, therefore unable to build Boost_PYTHON_COMPONENT")
    endif()
    set(Boost_PYTHON_COMPONENT
            python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR})
    set(Boost_PYTHON_COMPONENT ${Boost_PYTHON_COMPONENT} PARENT_SCOPE)
    if(USE_KATANA_BOOST)
        # Setup the variables to use the Katana builds.
        set(BOOST_LIBRARYDIR ${KATANA_API_LOCATION}/bin)
        set(BOOST_INCLUDEDIR ${KATANA_API_LOCATION}/external/foundryboost/include)

        set(Boost_NO_SYSTEM_PATHS ON)
        set(Boost_NO_BOOST_CMAKE ON)
        set(Boost_USE_MULTITHREADED ON)
        set(Boost_USE_RELEASE_LIBS ON)
        set(Boost_USE_DEBUG_LIBS OFF)
        set(Boost_NAMESPACE foundryboost)
        set(Boost_USE_STATIC_LIBS OFF)
        if(MSVC)
            add_definitions(-DBOOST_ALL_NO_LIB)
            add_definitions(-DBOOST_ALL_DYN_LINK)
            set(Boost_ARCHITECTURE -x64)
            if(MSVC_VERSION GREATER_EQUAL 1930)
                set(Boost_COMPILER -vc143)
            elseif(MSVC_VERSION GREATER_EQUAL 1920)
                set(Boost_COMPILER -vc142)
            elseif(MSVC_VERSION GREATER_EQUAL 1910)
                set(Boost_COMPILER -vc141)
            elseif(MSVC_VERSION GREATER_EQUAL 1900)
                set(Boost_COMPILER -vc140)
            else()
                message(FATL_ERROR "Unable to find MSVC version for detecting Boost version in "
                    "Katana")
            endif()
        endif()
        add_compile_definitions(Boost_NAMESPACE=${Boost_NAMESPACE})
    endif()

    # TODO(rk): Temporary policy downgrade to OLD until we change boost settings in Conan.
    cmake_policy(SET CMP0167 OLD)
    find_package(Boost
        COMPONENTS
            atomic # Required by thread
            ${Boost_PYTHON_COMPONENT}
            chrono # Required by thread
            date_time
            thread
            system
        REQUIRED)
endmacro() # add_boost_interface


macro(add_python_interface)
    if (TARGET Python3::Python)
        return()
    endif()

    if(USE_KATANA_PYTHON)
        include(${KATANA_API_LOCATION}/plugin_apis/cmake/python-variables.cmake)

        add_library(Python3::Python INTERFACE IMPORTED)
        # Parent_scope required for Python3_EXECUTABLE variable is used in
        # the parent cmake files.
        set(Python3_EXECUTABLE
            ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_EXECUTABLE}
            PARENT_SCOPE)
        set(Python3_LIBRARIES
            ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_LIB})

        set(Python3_INCLUDE_DIRS ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_INCLUDE_FOLDER})
        set(_py_dir python${KATANA_PYTHON_VERSION_MAJOR}.${KATANA_PYTHON_VERSION_MINOR})
        if(EXISTS ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_INCLUDE_FOLDER}/${_py_dir})
            list(APPEND Python3_INCLUDE_DIRS ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_INCLUDE_FOLDER}/${_py_dir})
        endif()
        if(EXISTS ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_INCLUDE_FOLDER}/${_py_dir}m)
            list(APPEND Python3_INCLUDE_DIRS ${KATANA_API_LOCATION}/bin/${KATANA_PYTHON_INCLUDE_FOLDER}/${_py_dir}m)
        endif()
        unset(_py_dir)
        set_target_properties(Python3::Python
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Python3_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${Python3_LIBRARIES}"
        )
        set(Python3_VERSION_MAJOR ${KATANA_PYTHON_VERSION_MAJOR} PARENT_SCOPE)
        set(Python3_VERSION_MINOR ${KATANA_PYTHON_VERSION_MINOR} PARENT_SCOPE)
    elseif(DEFINED Python3_ROOT_DIR)
        find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
        if(Python3_INCLUDE_DIRS AND Python3_LIBRARIES AND Python3_EXECUTABLE)
            # add_library(Python::Python INTERFACE IMPORTED)
            set_target_properties(Python3::Python
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${Python3_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES "${Python3_LIBRARIES}"
            )
        else()
            message(FATAL_ERROR "Cannot find Python libraries or headers"
                " using find_package(Python). Ensure the Python3_ROOT_DIR is"
                " specified correctly. Or Ensure that Python3_EXECUTABLE is"
                " defined in your build script"
            )
        endif()
    elseif(DEFINED Python3_DIR AND DEFINED Python3_EXECUTABLE)
        find_package(Python3 CONFIG REQUIRED)
    else()
        message(FATAL_ERROR "Cannot search for Python libraries, must"
            " specify either USE_KATANA_PYTHON to use the Python shipped "
            " with Katana, Python3_ROOT_DIR to use default CMake"
            " FindPackage or Python3_DIR and Python3_EXECUTABLE to use a"
            " custom cmake config"
        )
    endif()
endmacro() #add_python_interface

macro(add_tbb_interface)
    if(USE_KATANA_TBB)
        # We want to create CMake interfaces to make linking neater and to
        # reduce complexity later in the build
        if (TARGET TBB::tbb)
            return()
        endif()
        add_library(TBB::tbb INTERFACE IMPORTED)
        add_library(TBB::tbbmalloc INTERFACE IMPORTED)
        add_library(TBB::tbbmalloc_proxy INTERFACE IMPORTED)
        if(KATANA_API_LOCATION)
            if(UNIX)
                set(tbb_lib_suffix so)
                set(tbb_lib_prefix lib)
            elseif(WIN32)
                set(tbb_lib_suffix lib)
                unset(tbb_lib_prefix)
            endif() # OS Type
            set_target_properties(TBB::tbb
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES
                        "${KATANA_API_LOCATION}/external/tbb/include"
                    INTERFACE_COMPILE_DEFINITIONS
                        "__TBB_NO_IMPLICIT_LINKAGE=1"
                    INTERFACE_LINK_LIBRARIES
                        "${KATANA_API_LOCATION}/bin/${tbb_lib_prefix}tbb.${tbb_lib_suffix}"
            )
            set_target_properties(TBB::tbbmalloc
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES
                        "${KATANA_API_LOCATION}/external/tbb/include"
                    INTERFACE_COMPILE_DEFINITIONS
                        "__TBB_NO_IMPLICIT_LINKAGE=1"
                    INTERFACE_LINK_LIBRARIES
                        "${KATANA_API_LOCATION}/bin/${tbb_lib_prefix}tbbmalloc.${tbb_lib_suffix}"
            )
            set_target_properties(TBB::tbbmalloc_proxy
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES
                        "${KATANA_API_LOCATION}/external/tbb/include"
                    INTERFACE_COMPILE_DEFINITIONS
                        "__TBB_NO_IMPLICIT_LINKAGE=1"
                    INTERFACE_LINK_LIBRARIES
                        "${KATANA_API_LOCATION}/bin/${tbb_lib_prefix}tbbmalloc_proxy.${tbb_lib_suffix}"
            )
        else()
            message(FATAL_ERROR "KATANA_API_LOCATION must be set if using the"
                " USE_KATANA_TBB option")
        endif() # If KATANA_API_LOCATION
    elseif(DEFINED TBB_DIR)
        find_package(TBB CONFIG REQUIRED)
    else()
        find_package(TBB REQUIRED)
        add_library(TBB::tbb INTERFACE IMPORTED)
        add_library(TBB::tbbmalloc INTERFACE IMPORTED)
        add_library(TBB::tbbmalloc_proxy INTERFACE IMPORTED)
        if(TBB_tbb_FOUND)
            set_target_properties(TBB::tbb
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
                    INTERFACE_COMPILE_DEFINITIONS "__TBB_NO_IMPLICIT_LINKAGE=1"
                    INTERFACE_LINK_LIBRARIES "${TBB_tbb_LIBRARY}"
            )
            set_target_properties(TBB::tbbmalloc
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
                    INTERFACE_COMPILE_DEFINITIONS "__TBB_NO_IMPLICIT_LINKAGE=1"
                    INTERFACE_LINK_LIBRARIES "${TBB_tbbmalloc_LIBRARY}"
            )
            set_target_properties(TBB::tbbmalloc_proxy
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
                    INTERFACE_COMPILE_DEFINITIONS "__TBB_NO_IMPLICIT_LINKAGE=1"
                    INTERFACE_LINK_LIBRARIES "${TBB_tbbmalloc_proxy_LIBRARY}"
            )
        else()
            message(FATAL_ERROR "Unable to find tbb library")
        endif()
    endif() # If USE_KATANA_TBB
endmacro() #add_tbb_interface


macro(add_usd_interface)
    if(USE_KATANA_USD)
        set(USD_LIBRARY_DIR ${KATANA_API_LOCATION}/bin)
        set(USD_INCLUDE_DIR ${KATANA_API_LOCATION}/external/FnUSD/include)
        set(PXR_LIB_PREFIX fn)
        if(UNIX)
            set(PXR_LIB_PREFIX libfn)
        endif()
        set(PXR_PY_PACKAGE_NAME fnpxr PARENT_SCOPE)
        find_package(USD REQUIRED)
    elseif(USE_FOUNDRY_FIND_USD)
        find_package(USD REQUIRED)
    else()
        if(USD_USING_CMAKE_THIRDPARTY_TARGET_DEPENDENCIES)
            find_package(OpenEXR CONFIG REQUIRED)
            find_package(OpenSubdiv REQUIRED)
        endif()
        if(NOT DEFINED USD_ROOT)
            message(FATAL_ERROR "Build option USD_ROOT is not defined")
        endif()
        include(${USD_ROOT}/pxrConfig.cmake)
    endif()
endmacro() #add_use_interface
