include(PxrUsdUtils)

find_package(KatanaAPI REQUIRED)
find_package(Boost COMPONENTS python thread system regex REQUIRED)
if(NOT TARGET GLEW::GLEW)
    find_package(GLEW CONFIG REQUIRED)
endif()
find_package(TBB CONFIG REQUIRED)
find_package(Python CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSubdiv REQUIRED)
find_package(PTex REQUIRED)

if(NOT DEFINED USD_ROOT)
    message(FATAL_ERROR "Build option USD_ROOT is not defined")
endif()
include(${USD_ROOT}/pxrConfig.cmake)

function(pxr_library NAME)
    set(options
    )
    set(oneValueArgs
        TYPE
        PRECOMPILED_HEADER_NAME
    )
    set(multiValueArgs
        PUBLIC_CLASSES
        PUBLIC_HEADERS
        PRIVATE_CLASSES
        PRIVATE_HEADERS
        CPPFILES
        LIBRARIES
        INCLUDE_DIRS
        RESOURCE_FILES
        PYTHON_PUBLIC_CLASSES
        PYTHON_PRIVATE_CLASSES
        PYTHON_PUBLIC_HEADERS
        PYTHON_PRIVATE_HEADERS
        PYTHON_CPPFILES
        PYMODULE_CPPFILES
        PYMODULE_FILES
        PYSIDE_UI_FILES
    )
    cmake_parse_arguments(args
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )
    foreach(cls ${args_PUBLIC_CLASSES})
        list(APPEND ${NAME}_CPPFILES ${cls}.cpp)
    endforeach()
    foreach(cls ${args_PRIVATE_CLASSES})
        list(APPEND ${NAME}_CPPFILES ${cls}.cpp)
    endforeach()
    string(TOUPPER ${NAME} uppercaseName)
    if(args_TYPE STREQUAL "STATIC")
        add_library(${NAME} STATIC "${args_CPPFILES};${${NAME}_CPPFILES}")
        set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        # no install for static libraries
        _get_install_dir("lib/usd" pluginInstallPrefix)
    elseif (args_TYPE STREQUAL "SHARED")
        if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
            add_katana_plugin(${NAME} 
                OUTPUT_PATH
                ${PLUGINS_RES_BUNDLE_PATH}/Usd/lib
                "${args_CPPFILES};${${NAME}_CPPFILES}"
            )
        else()
            add_library(${NAME} SHARED "${args_CPPFILES};${${NAME}_CPPFILES}")
        endif()
        set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        list(APPEND ${NAME}_DEFINITIONS ${uppercaseName}_EXPORTS=1)
        if(NOT BUILD_KATANA_INTERNAL_USD_PLUGINS)
            install(TARGETS ${NAME} DESTINATION "${PXR_INSTALL_SUBDIR}/lib")
        endif()
        _get_install_dir("lib/usd" pluginInstallPrefix)
    elseif (args_TYPE STREQUAL "PLUGIN")
        if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
            add_katana_plugin(${NAME} 
                OUTPUT_PATH
                ${PLUGINS_RES_BUNDLE_PATH}/Usd/plugin/Libs
                "${args_CPPFILES};${${NAME}_CPPFILES}"
            )
        else()
            add_library(${NAME} SHARED "${args_CPPFILES};${${NAME}_CPPFILES}")
        endif()
        set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        list(APPEND ${NAME}_DEFINITIONS ${uppercaseName}_EXPORTS=1)
        
        if(NOT BUILD_KATANA_INTERNAL_USD_PLUGINS)
            install(TARGETS ${NAME} DESTINATION "${PXR_INSTALL_SUBDIR}/plugin/Libs")
        endif()
        _get_install_dir("plugin" pluginInstallPrefix)
    else()
        message(FATAL_ERROR "Unsupported library type: " args_TYPE)
    endif()
    set(pluginToLibraryPath "")

    _install_resource_files(
        ${NAME}
        "${pluginInstallPrefix}"
        "${pluginToLibraryPath}"
        ${args_RESOURCE_FILES}
    )

    target_include_directories(
        ${NAME}
        PRIVATE
        ${args_INCLUDE_DIRS}
        ${KATANA_USD_PLUGINS_SRC_ROOT}/lib
        ${KATANA_USD_PLUGINS_SRC_ROOT}/plugin
        ${KATANA_API_INCLUDE_DIR}
        ${GEOLIB_SRC_API_INCLUDE_DIR}
        ${KATANA_SRC_API_INCLUDE_DIR}
    )
    target_compile_definitions(
        ${NAME}
        PRIVATE
        -DBOOST_ALL_NO_LIB
        -DFNASSET_STATIC=1
        -DFNATTRIBUTE_STATIC=1
        -DFNATTRIBUTEFUNCTION_STATIC=1
        -DFNATTRIBUTEMODIFIER_STATIC=1
        -DFNCONFIG_STATIC=1
        -DFNDEFAULTATTRIBUTEPRODUCER_STATIC=1
        -DFNGEOLIB_STATIC=1
        -DFNGEOLIBSERVICES_STATIC=1
        -DFNLOGGING_STATIC=1
        -DFNPLATFORM_STATIC=1
        -DFNPLUGINMANAGER_STATIC=1
        -DFNPLUGINSYSTEM_STATIC=1
        -DFNRENDEROUTPUTLOCATION_STATIC=1
        -DFNRENDEROUTPUTUTILS_STATIC=1
        -DFNRENDER_STATIC=1
        -DFNRENDERERINFO_STATIC=1
        -DFNSCENEGRAPHGENERATOR_STATIC=1
        -DFNSCENEGRAPHITERATOR_STATIC=1
        -DPYSTRING_STATIC=1
        -DFNDISPLAYDRIVER_STATIC=1
        -DFNVIEWERMODIFIER_STATIC=1
        -DFNVIEWERAPI_STATIC=1
        ${${NAME}_DEFINITIONS}
    )
    target_compile_options(
        ${NAME}
        PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -std=c++11 -Wno-unused-but-set-variable -Wno-deprecated -Wno-unused-local-typedefs>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4267 /wd4100 /wd4702 /wd4244 /wd4800 /wd4996 /wd4456 /wd4127 /wd4701 /wd4305 /wd4838 /wd4624 /wd4506 /wd4245 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DNOGDI /FIiso646.h>
    )
    target_link_libraries(
        ${NAME}
        PRIVATE
        ${args_LIBRARIES}
        $<$<CXX_COMPILER_ID:MSVC>:OPENGL32.lib>
    )

    # make a separate shared library for the Python wrapper
    if(args_PYMODULE_CPPFILES)
        add_library(_${NAME} SHARED ${args_PYMODULE_CPPFILES})

        target_include_directories(
            _${NAME}
            PRIVATE
            ${KATANA_USD_PLUGINS_SRC_ROOT}/lib
            ${KATANA_USD_PLUGINS_SRC_ROOT}/plugin
            ${KATANA_API_INCLUDE_DIR}
            ${GEOLIB_SRC_API_INCLUDE_DIR}
            ${KATANA_SRC_API_INCLUDE_DIR}
        )
        target_compile_definitions(
            _${NAME}
            PRIVATE
            -DBOOST_ALL_NO_LIB
            MFB_PACKAGE_NAME=${NAME}
            MFB_ALT_PACKAGE_NAME=${NAME}
            MFB_PACKAGE_MODULE="${pyModuleName}"
        )
        target_compile_options(
            _${NAME}
            PRIVATE
            $<$<CXX_COMPILER_ID:GNU>:-Wall -std=c++11 -Wno-deprecated -Wno-unused-local-typedefs>
            $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4244 /wd4305 /wd4100 /wd4459 /DWIN32_LEAN_AND_MEAN /DNOMINMAX>
        )
        target_link_libraries(
            _${NAME}
            PRIVATE
            ${args_LIBRARIES}
            ${NAME}
        )
        _get_python_module_name(_${NAME} pyModuleName)
        install(TARGETS _${NAME} DESTINATION "${PXR_INSTALL_SUBDIR}/lib/python/pxr/${pyModuleName}")

        if(args_PYMODULE_FILES)
            _install_python(
                _${NAME}
                FILES ${args_PYMODULE_FILES}
            )
        endif()

        if(WIN32)
            # Python modules must be suffixed with .pyd on Windows.
            set_target_properties(
                _${NAME}
                PROPERTIES
                SUFFIX ".pyd"
            )
        endif()
    endif()
endfunction()

macro(pxr_static_library NAME)
    pxr_library(${NAME} TYPE "STATIC" ${ARGN})
endmacro(pxr_static_library)

macro(pxr_shared_library NAME)
    pxr_library(${NAME} TYPE "SHARED" ${ARGN})
endmacro(pxr_shared_library)

macro(pxr_plugin NAME)
    pxr_library(${NAME} TYPE "PLUGIN" ${ARGN})
endmacro(pxr_plugin)

function(pxr_test_scripts)
    MESSAGE(AUTHOR_WARNING "Test scripts not implemented")
endfunction()

function(pxr_install_test_dir)
    MESSAGE(AUTHOR_WARNING "Test install dir not implemented")
endfunction()

function(pxr_register_test TEST_NAME)
    MESSAGE(AUTHOR_WARNING "Test register not implemented")
endfunction()
