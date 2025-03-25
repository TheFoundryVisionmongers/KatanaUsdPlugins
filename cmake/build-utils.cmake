# add_test_executable(target_name
#   [...SOURCES...]
# )
#
# A thin wrapper around add_executable() and add_test() to register a unit
# test binary.
#
# This function will also attempt to set up the correct library environment
# and paths so the test will run correctly when invoked via ctest.
function(usdKatana_add_test_executable target_name)
    cmake_parse_arguments(args "" "" "" ${ARGN})
    set(sources ${args_UNPARSED_ARGUMENTS})

    add_executable(${target_name} ${sources})

    if(LINUX)
        add_test(NAME ${target_name}
            COMMAND ${CMAKE_COMMAND} -E env
            KATANA_ROOT=${EXTERNAL_BUNDLE_PATH}
            LD_LIBRARY_PATH=${BIN_BUNDLE_PATH}:${PXR_INSTALL_SUBDIR}/lib:${BIN_BUNDLE_PATH}/${KATANA_PYTHON_LIB_FOLDER}
            $<TARGET_FILE:${target_name}>
        )
    else()
        add_test(NAME ${target_name}
            COMMAND ${CMAKE_COMMAND} -E env
            KATANA_ROOT=${EXTERNAL_BUNDLE_PATH}
            "PATH=${BIN_BUNDLE_PATH}\;${PXR_INSTALL_SUBDIR}/lib\;$ENV{PATH}"
            $<TARGET_FILE:${target_name}>
        )
    endif()

    target_compile_definitions(
        ${target_name}
        PRIVATE
        -DFNATTRIBUTE_STATIC=1
        -DFNGEOLIB_STATIC=1
    )
endfunction()
