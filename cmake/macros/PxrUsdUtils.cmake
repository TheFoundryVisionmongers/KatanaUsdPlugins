# from USD/cmake/macros/Public.cmake
function(pxr_katana_nodetypes NODE_TYPES)
    set(installDir ${PXR_INSTALL_SUBDIR}/plugin/Plugins/NodeTypes)

    set(pyFiles "")
    set(importLines "")

    foreach (nodeType ${NODE_TYPES})
        list(APPEND pyFiles ${nodeType}.py)
        set(importLines "import ${nodeType}\n")
    endforeach()

    install(
        PROGRAMS ${pyFiles}
        DESTINATION ${installDir}
    )

    # Install a __init__.py that imports all the known node types
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated_NodeTypes_init.py"
         "${importLines}")
    if(BUILD_KATANA_INTERNAL_USD_PLUGINS)
        bundle_files(
            TARGET 
            USD.NodeTypes.bundle
            DESTINATION_FOLDER
            ${PLUGINS_RES_BUNDLE_PATH}/Usd/plugin/Plugins/${pyModuleName}
            FILES
            ${pyFiles}
        )
    else()
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/generated_NodeTypes_init.py"
            DESTINATION "${installDir}"
            RENAME "__init__.py"
        )
    endif()
endfunction() # pxr_katana_nodetypes

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
# e.g. lib/python/pxr/Ar/__init__.pyc
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

    set(libPythonPrefix ${PXR_INSTALL_SUBDIR}/lib/python)
    _get_python_module_name(${LIBRARY_NAME} LIBRARY_INSTALLNAME)

    set(files_copied "")
    foreach(file ${ip_FILES})
        set(filesToInstall "")
        set(installDest
            "${libPythonPrefix}/pxr/${LIBRARY_INSTALLNAME}")

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
                add_custom_command(OUTPUT ${outfile}
                    COMMAND
                    ${BIN_BUNDLE_PATH}/python
                    ${KATANA_USD_PLUGINS_SRC_ROOT}/cmake/macros/compilePython.py
                    ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                    ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                    ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.pyc
                )
            else()
                add_custom_command(OUTPUT ${outfile}
                    COMMAND
                    ${PYTHON_EXECUTABLE}
                    ${KATANA_USD_PLUGINS_SRC_ROOT}/cmake/macros/compilePython.py
                    ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                    ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                    ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.pyc
                )
            endif()
            list(APPEND filesToInstall ${CMAKE_CURRENT_SOURCE_DIR}/${file})
            list(APPEND filesToInstall ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.pyc)
        elseif (${file} MATCHES ".qss$")
            # XXX -- Allow anything or allow nothing?
            list(APPEND filesToInstall ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        else()
            message(FATAL_ERROR "Cannot have non-Python file ${file} in PYTHON_FILES.")
        endif()

        # Note that we always install under lib/python/pxr, even if we are in
        # the third_party project. This means the import will always look like
        # 'from pxr import X'. We need to do this per-loop iteration because
        # the installDest may be different due to the presence of subdirs.
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
        set(${out} ${path} PARENT_SCOPE)
    endif()
endfunction() # get_install_dir

# from USD/cmake/macros/Private.cmake
function(_install_resource_files NAME pluginInstallPrefix pluginToLibraryPath)
    # Resource files install into a structure that looks like:
    # lib/
    #     usd/
    #         ${NAME}/
    #             resources/
    #                 resourceFileA
    #                 subdir/
    #                     resourceFileB
    #                     resourceFileC
    #                 ...
    #
    _get_resources_dir(${pluginInstallPrefix} ${NAME} resourcesPath)

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

        install(
            FILES ${resourceFile}
            DESTINATION ${resourcesPath}/${dirPath}
            RENAME ${destFileName}
        )
    endforeach()
endfunction() # _install_resource_files

function(_get_resources_dir_name output)
    set(${output}
        resources
        PARENT_SCOPE)
endfunction() # _get_resources_dir_name

function(_get_resources_dir pluginsPrefix pluginName output)
    _get_resources_dir_name(resourcesDir)
    set(${output}
        ${pluginsPrefix}/${pluginName}/${resourcesDir}
        PARENT_SCOPE)
endfunction() # _get_resources_dir

function(_plugInfo_subst libTarget pluginToLibraryPath plugInfoPath)
    _get_resources_dir_name(PLUG_INFO_RESOURCE_PATH)
    set(PLUG_INFO_ROOT "..")
    set(PLUG_INFO_PLUGIN_NAME "pxr.${libTarget}")
    set(PLUG_INFO_LIBRARY_PATH "${pluginToLibraryPath}")

    configure_file(
        ${plugInfoPath}
        ${CMAKE_CURRENT_BINARY_DIR}/${plugInfoPath}
    )
endfunction() # _plugInfo_subst

