set(LIB_NAME pxrUsdInShipped)

set(CPPFILES
    register.cpp

    attrfnc_materialReference.cpp
    basisCurves.cpp
    camera.cpp
    constraints.cpp
    geomSubset.cpp
    light.cpp
    lightFilter.cpp
    material.cpp
    materialsGroup.cpp
    mesh.cpp
    model.cpp
    nurbsPatch.cpp
    pointInstancer.cpp
    points.cpp
    scope.cpp
    uiUtils.cpp
    xform.cpp
)

set(LIBRARIES
    katanaAttrfncApi
    katanaOpApi
    katanaPluginApi
    kind
    arch
    tf
    sdf
    vt
    usd
    usdGeom
    usdKatana
    usdShade
    usdLux
    usdRi
    ${Boost_PYTHON_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${PYTHON_LIBRARIES}
)

add_library(${LIB_NAME} SHARED ${CPPFILES})

target_link_libraries(${LIB_NAME} PRIVATE ${LIBRARIES})
target_include_directories(${LIB_NAME} PRIVATE ${KATANA_API_INCLUDE_DIRS})

set_target_properties(${LIB_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH "$ORIGIN/../../lib"
)

install(TARGETS ${LIB_NAME} DESTINATION "${INSTALL_SUBDIR}/plugin/Libs")
if(CMAKE_SYSTEM_NAME MATCHES Windows)
    install(
        FILES $<TARGET_PDB_FILE:${LIB_NAME}>
        DESTINATION "${INSTALL_SUBDIR}/plugin/Libs"
        OPTIONAL
    )
endif()


# Copy test files and add test target if tests are enabled.
if(BUILD_TESTS)

    # Read the current environment variables and append paths.
    set(KATANA_RESOURCES $ENV{KATANA_RESOURCES})
    list(APPEND KATANA_RESOURCES
        ${INSTALL_DIR}/plugin
    )
    set(PYTHONPATH $ENV{PYTHONPATH})
    list(APPEND PYTHONPATH
        ${USD_LIBRARY_DIR}/python
    )
    set(PATH $ENV{PATH})
    list(APPEND PATH
        ${USD_ROOT}/bin
        ${USD_LIBRARY_DIR}
        ${INSTALL_DIR}/lib
    )

    # Note that the ; character is used by CMake as a delimiter, which means
    # that on Windows it needs to be scaped (a number of times).
    set(PATHSEP ":")
    if(CMAKE_SYSTEM_NAME MATCHES Windows)
        set(PATHSEP "\\\\\\;")
    endif()

    # Join the lists into a single string.
    list(JOIN KATANA_RESOURCES ${PATHSEP} KATANA_RESOURCES_STR)
    list(JOIN PYTHONPATH ${PATHSEP} PYTHONPATH_STR)
    list(JOIN PATH ${PATHSEP} PATH_STR)

    # Join all the environment variables together.
    set(ENVARS "KATANA_RESOURCES=${KATANA_RESOURCES_STR}")
    set(ENVARS "${ENVARS}\\;PYTHONPATH=${PYTHONPATH_STR}")
    set(ENVARS "${ENVARS}\\;PATH=${PATH_STR}")

    # List of tests to run.
    set(TESTS "Basic" "PointInstancer")

    foreach(TEST ${TESTS})
        file(GLOB_RECURSE FILES "testenv/${TEST}/**")
        install(
            FILES ${FILES}
            DESTINATION "${INSTALL_SUBDIR}/tests/${TEST}"
        )
        add_test(
            NAME
                "testPxrUsdInShipped${TEST}"
            COMMAND
                "${KATANA_BIN}"
                "--script"
                "${INSTALL_DIR}/tests/${TEST}/testPxrUsdInShipped${TEST}.py"
            WORKING_DIRECTORY
                "${INSTALL_DIR}/tests/${TEST}"
        )
        set_tests_properties("testPxrUsdInShipped${TEST}" PROPERTIES
            ENVIRONMENT ${ENVARS}
        )
    endforeach()
endif()
