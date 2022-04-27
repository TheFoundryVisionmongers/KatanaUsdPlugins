# from USD/cmake/macros/Public.cmake
function(pxr_katana_nodetypes NODE_TYPES)
    if(PXR_INSTALL_SUBDIR)
        set(installDir ${PXR_INSTALL_SUBDIR}/plugin/Plugins/${pyModuleName})
    else()
        set(installDir ${CMAKE_INSTALL_PREFIX}/plugin/Plugins/${pyModuleName})
    endif()

    set(pyFiles "")
    set(importLines "")

    foreach (nodeType ${NODE_TYPES})
        list(APPEND pyFiles ${nodeType}.py)
        set(importLines "from . import ${nodeType}\n")
    endforeach()

    foreach(pyfile ${pyFiles})
        _replace_root_python_module(
            ${CMAKE_CURRENT_SOURCE_DIR}/${pyfile}
            ${installDir}/${pyfile}
        )
    endforeach()

    # Install a __init__.py that imports all the known node types
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated_NodeTypes_init.py"
         "${importLines}")

    foreach(pyfile ${pyFiles})
        _replace_root_python_module(
            ${CMAKE_CURRENT_BINARY_DIR}/generated_NodeTypes_init.py
            ${installDir}/__init__.py
        )
    endforeach()
endfunction() # pxr_katana_nodetypes


# from USD/cmake/macros/Public.cmake
function(pxr_katana_python_plugin)
    # Installs the PYTHON_MODULE_FILES to /lib/python
    # Installs the PYTHON_PLUGIN_REGISTRY_FILES files to /plugin/{PLUGIN_TYPE}
    set(options)
    set(oneValueArgs
        MODULE_NAME
        PLUGIN_TYPE
    )
    set(multiValueArgs
        PYTHON_PLUGIN_REGISTRY_FILES # Files used to register plugins
        PYTHON_MODULE_FILES # Module files to be installed into lib/python
    )
    cmake_parse_arguments(args
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )
    if(PXR_INSTALL_SUBDIR)
        set(pluginInstallDir ${PXR_INSTALL_SUBDIR}/plugin/${args_PLUGIN_TYPE})
        set(pythonInstallDir ${PXR_INSTALL_SUBDIR}/lib/python)
    else()
        set(pluginInstallDir ${CMAKE_INSTALL_PREFIX}/plugin/${args_PLUGIN_TYPE})
        set(pythonInstallDir ${CMAKE_INSTALL_PREFIX}/lib/python)
    endif()

    if(args_PYTHON_PLUGIN_REGISTRY_FILES)
        foreach(plugin_registry_file ${args_PYTHON_PLUGIN_REGISTRY_FILES})
            _replace_root_python_module(
                ${CMAKE_CURRENT_SOURCE_DIR}/${plugin_registry_file}
                ${pluginInstallDir}/${plugin_registry_file}
            )
        endforeach()
    endif()
    if(args_PYTHON_MODULE_FILES)
        foreach(py_module_file ${args_PYTHON_MODULE_FILES})
            _replace_root_python_module(
                ${CMAKE_CURRENT_SOURCE_DIR}/${py_module_file}
                ${pythonInstallDir}/${py_module_file}
            )
        endforeach()
    endif()
endfunction() # pxr_katana_lookFileBake

# from USD/cmake/macros/Private.cmake
function(_get_python_module_name LIBRARY_FILENAME MODULE_NAME)
    # Library names are either something like tf.so for shared libraries
    # or _tf.so for Python module libraries. We want to strip the leading
    # "_" off.
    string(REPLACE "_" "" LIBNAME ${LIBRARY_FILENAME})
    string(SUBSTRING ${LIBNAME} 0 1 LIBNAME_FL)
    string(TOUPPER ${LIBNAME_FL} LIBNAME_FL)
    string(SUBSTRING ${LIBNAME} 1 -1 LIBNAME_SUFFIX)
    set(${MODULE_NAME}
        "${LIBNAME_FL}${LIBNAME_SUFFIX}"
        PARENT_SCOPE
    )
endfunction() # _get_python_module_name

# from USD/cmake/macros/Private.cmake
# Install compiled python files alongside the python object,
# e.g. lib/python/${PXR_PY_PACKAGE_NAME}/Ar/__init__.pyc
function(_install_python LIBRARY_NAME)
    set(options  "")
    set(oneValueArgs "")
    set(multiValueArgs FILES)
    cmake_parse_arguments(ip
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(PXR_INSTALL_SUBDIR)
        set(libPythonPrefix ${PXR_INSTALL_SUBDIR}/lib/python)
    else()
        set(libPythonPrefix ${CMAKE_INSTALL_PREFIX}/lib/python)
    endif()
    _get_python_module_name(${LIBRARY_NAME} LIBRARY_INSTALLNAME)

    set(files_copied "")
    foreach(file ${ip_FILES})
        set(filesToInstall "")

        set(installDest
            "${libPythonPrefix}/${PXR_PY_PACKAGE_NAME}/${LIBRARY_INSTALLNAME}")
        if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
            set(installDest
                "${libPythonPrefix}/${LIBRARY_INSTALLNAME}")
        endif()

        # Only attempt to compile .py files. Files like plugInfo.json may also
        # be in this list
        if (${file} MATCHES ".py$")
            get_filename_component(file_we ${file} NAME_WE)

            # Preserve any directory prefix, just strip the extension. This
            # directory needs to exist in the binary dir for the COMMAND below
            # to work.
            get_filename_component(dir ${file} PATH)
            if (dir)
                file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${dir})
                set(file_we ${dir}/${file_we})
                set(installDest ${installDest}/${dir})
            endif()

            set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.pyc)
            list(APPEND files_copied ${outfile})
            if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
              if(WIN32)
                set(pythonexe ${BIN_BUNDLE_PATH}/python)
              else()
                # assuming Linux
                set(pythonexe ${BIN_BUNDLE_PATH}/python.sh)
              endif()
            else()
              set(pythonexe ${Python_EXECUTABLE})
            endif()
            _replace_root_python_module(
                ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                ${CMAKE_CURRENT_BINARY_DIR}/${file}
            )
            add_custom_command(OUTPUT ${outfile}
                COMMAND
                ${pythonexe}
                ${KATANA_USD_PLUGINS_SRC_ROOT}/cmake/macros/compilePython.py
                ${CMAKE_CURRENT_BINARY_DIR}/${file}
                ${CMAKE_CURRENT_BINARY_DIR}/${file}
                ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.pyc
            )
            list(APPEND filesToInstall ${CMAKE_CURRENT_BINARY_DIR}/${file})
            list(APPEND filesToInstall ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.pyc)
        elseif (${file} MATCHES ".qss$")
            # XXX -- Allow anything or allow nothing?
            list(APPEND filesToInstall ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        else()
            message(FATAL_ERROR "Cannot have non-Python file ${file} in PYTHON_FILES.")
        endif()

        if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
            foreach(current_file ${filesToInstall})
                get_filename_component(filename ${current_file} NAME)
                add_custom_command(TARGET ${LIBRARY_NAME}
                    COMMAND ${CMAKE_COMMAND} -E copy ${current_file}
                        ${installDest}/${filename}
                )
            endforeach()
        endif()

        # Note that we always install under lib/python/${PXR_PY_PACKAGE_NAME},
        # even if we are in the third_party project. This means the import will
        # always look like 'from ${PXR_PY_PACKAGE_NAME} import X'. We need to
        # do this per-loop iteration because the installDest may be different
        # due to the presence of subdirs.
        install(
            FILES
                ${filesToInstall}
            DESTINATION
                "${installDest}"
        )
    endforeach()

    # Add the target.
    add_custom_target(${LIBRARY_NAME}_pythonfiles
        DEPENDS ${files_copied}
    )
    # Foundry(MF): changed as the python target does not exist
    #add_dependencies(python ${LIBRARY_NAME}_pythonfiles)
    add_dependencies(${LIBRARY_NAME} ${LIBRARY_NAME}_pythonfiles)

    _get_folder("_python" folder)
    set_target_properties(${LIBRARY_NAME}_pythonfiles
        PROPERTIES
            FOLDER "${folder}"
    )
endfunction() #_install_python

# from USD/cmake/macros/Private.cmake
function(_get_folder suffix result)
    # XXX -- Shouldn't we set PXR_PREFIX everywhere?
    if(PXR_PREFIX)
        set(folder "${PXR_PREFIX}")
    elseif(PXR_INSTALL_SUBDIR)
        set(folder "${PXR_INSTALL_SUBDIR}")
    else()
        set(folder "misc")
    endif()
    if(suffix)
        set(folder "${folder}/${suffix}")
    endif()
    set(${result} ${folder} PARENT_SCOPE)
endfunction()

# from USD/cmake/macros/Private.cmake
function(_get_install_dir path out)
    if (PXR_INSTALL_SUBDIR)
        set(${out} ${PXR_INSTALL_SUBDIR}/${path} PARENT_SCOPE)
    else()
        set(${out} ${CMAKE_INSTALL_PREFIX}/${path} PARENT_SCOPE)
    endif()
endfunction() # get_install_dir

# from USD/cmake/macros/Private.cmake
function(_install_resource_files NAME pluginInstallPrefix pluginToLibraryPath)
    # Resource files install into a structure that looks like:
    # lib/
    #    resources/
    #        resourceFileA
    #        subdir/
    #            resourceFileB
    #            resourceFileC
    #        ...
    #
    _get_resources_dir(${pluginInstallPrefix} resourcesPath)

    foreach(resourceFile ${ARGN})
        # A resource file may be specified like <src file>:<dst file> to
        # indicate that it should be installed to a different location in
        # the resources area. Check if this is the case.
        string(REPLACE ":" ";" resourceFile "${resourceFile}")
        list(LENGTH resourceFile n)
        if (n EQUAL 1)
           set(resourceDestFile ${resourceFile})
        elseif (n EQUAL 2)
           list(GET resourceFile 1 resourceDestFile)
           list(GET resourceFile 0 resourceFile)
        else()
           message(FATAL_ERROR
               "Failed to parse resource path ${resourceFile}")
        endif()

        get_filename_component(dirPath ${resourceDestFile} PATH)
        get_filename_component(destFileName ${resourceDestFile} NAME)

        # plugInfo.json go through an initial template substitution step files
        # install it from the binary (gen) directory specified by the full
        # path. Otherwise, use the original relative path which is relative to
        # the source directory.
        if (${destFileName} STREQUAL "plugInfo.json")
            _plugInfo_subst(${NAME} "${pluginToLibraryPath}" ${resourceFile})
            set(resourceFile "${CMAKE_CURRENT_BINARY_DIR}/${resourceFile}")
        endif()

        if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
            set(resourcesDestPath ${PLUGINS_RES_BUNDLE_PATH}/Usd/lib/resources)
            if (${destFileName} STREQUAL "plugInfo.json")
                configure_file(
                    ${resourceFile}
                    ${resourcesDestPath}/${dirPath}/${destFileName}
                    COPYONLY
                )
            else()
                configure_file(
                    ${CMAKE_CURRENT_SOURCE_DIR}/${resourceFile}
                    ${resourcesDestPath}/${dirPath}/${destFileName}
                    COPYONLY
                )
            endif()
        endif()

        install(
            FILES ${resourceFile}
            DESTINATION ${resourcesPath}/${dirPath}
            RENAME ${destFileName}
        )
    endforeach()
endfunction() # _install_resource_files

# Function to automatically replace pxr python module usage with the specified
# PXR_PY_PACKAGE_NAME module name. E.g `from pxr` becomes `from fnpxr`.
function(_replace_root_python_module INPUT_FILE OUTPUT_FILE)

    # We need to check the file exists before we run configure file.
    # The output file exists check
    set(output_file_exists OFF)
    if(EXISTS ${OUTPUT_FILE})
        set(output_file_exists ON)
    endif()
    # We must always run a configure file.
    configure_file(${INPUT_FILE} ${OUTPUT_FILE} COPYONLY)
    if(${PXR_PY_PACKAGE_NAME} STREQUAL "pxr")
        # We must always configure file, since some methods rely on the output
        # file being valid and present. However, we do not need to perform
        # any replacements.
        return()
    endif()

    # We want to restrict how much we write the output files, since the
    # compiler will regenerate objects if the output file is re-written to
    # again.
    if(${output_file_exists})
        get_filename_component(out_ext ${INPUT_FILE} EXT)
        if(out_ext)
            if((${out_ext} STREQUAL ".py"))
                set(force_run_replace ON)
            else()
                set(force_run_replace OFF)
            endif()
        else()
            set(force_run_replace OFF)
        endif()
        unset(out_ext)
    else()
        set(force_run_replace ON)
    endif()

    file(READ ${INPUT_FILE} filedata)
    # Some simple regex to replace usage of the default pxr with our
    # defined ${PXR_PY_PACKAGE_NAME}.  We must always have two regex groups
    # here as there are instances where we want to keep prefixes and suffixes
    # of the expressions used to find precise python related uses of pxr.
    set(regex_replacements
        "(\")pxr(\\.)"
        "(')pxr(\\.)"
        "(import )pxr()"
        "(from )pxr()")
    foreach(regex_replace ${regex_replacements})
        string(REGEX REPLACE
            ${regex_replace}
            "\\1${PXR_PY_PACKAGE_NAME}\\2"
            filedata "${filedata}")
    endforeach()

    # Check the output file to see if it matches the same as we are about to
    # write. We don't want to write if they are the same, as this will cause
    # recompilation even if the code hasnt changed.
    # Return early if we did not need to update the output file.
    if(NOT force_run_replace)
        file(READ ${OUTPUT_FILE} filedata_comp)
        if(filedata_comp STREQUAL filedata)
            return()
        endif()
    endif()

    # Either the output file didn't exist, or there have been other changes
    message(STATUS "PxrUsdUtils.cmake: Converting Python namespace and installing file to: ${OUTPUT_FILE}")
    file(WRITE ${OUTPUT_FILE} "${filedata}")
endfunction()

function(_get_resources_dir_name output)
    set(${output}
        resources
        PARENT_SCOPE)
endfunction() # _get_resources_dir_name

function(_get_resources_dir pluginsPrefix output)
    _get_resources_dir_name(resourcesDir)
    set(${output}
        ${pluginsPrefix}/${resourcesDir}
        PARENT_SCOPE)
endfunction() # _get_resources_dir

function(_plugInfo_subst libTarget pluginToLibraryPath plugInfoPath)
    _get_resources_dir_name(PLUG_INFO_RESOURCE_PATH)
    set(PLUG_INFO_ROOT "..")
    set(PLUG_INFO_PLUGIN_NAME "${PXR_PY_PACKAGE_NAME}.${libTarget}")
    set(PLUG_INFO_LIBRARY_PATH "${pluginToLibraryPath}")

    configure_file(
        ${plugInfoPath}
        ${CMAKE_CURRENT_BINARY_DIR}/${plugInfoPath}
    )
endfunction() # _plugInfo_subst

function(_katana_build_install libTarget installPathSuffix)
    # Output location for internal Katana build steps
    set_target_properties("${libTarget}"
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
            ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
        LIBRARY_OUTPUT_DIRECTORY_DEBUG
            ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
        LIBRARY_OUTPUT_DIRECTORY_RELEASE
            ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
    )
    if(WIN32)
        set_target_properties(${libTarget}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY
                    ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
                RUNTIME_OUTPUT_DIRECTORY_DEBUG
                    ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
                RUNTIME_OUTPUT_DIRECTORY_RELEASE
                    ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
                ARCHIVE_OUTPUT_DIRECTORY
                    ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
                ARCHIVE_OUTPUT_DIRECTORY_DEBUG
                    ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
                ARCHIVE_OUTPUT_DIRECTORY_RELEASE
                    ${PXR_INSTALL_SUBDIR}/${installPathSuffix}
        )
    endif()
endfunction() # _katana_build_install

function(pxr_katana_install_plugin_resources)
    set(options
    )
    set(oneValueArgs
        MODULE_NAME
        PLUGIN_TYPE
    )
    set(multiValueArgs
        FILES
    )
    cmake_parse_arguments(args
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(PXR_INSTALL_SUBDIR)
        set(installDir ${PXR_INSTALL_SUBDIR}/plugin/${args_PLUGIN_TYPE})
    else()
        set(installDir ${CMAKE_INSTALL_PREFIX}/plugin/${args_PLUGIN_TYPE})
    endif()
    if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
        bundle_files(
            TARGET
            ${args_MODULE_NAME}
            DESTINATION_FOLDER
            ${installDir}
            FILES
            ${args_FILES}
        )
    else()
        install(FILES ${args_FILES}
            DESTINATION ${installDir})
    endif()
endfunction() # pxr_katana_install_plugin_resources
