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
set(TESTS "")
if(BUILD_TESTS)
    set(TESTS "Basic" "PointInstancer")
endif()
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
            ${CMAKE_COMMAND} -E env
                "KATANA_RESOURCES=${INSTALL_DIR}/plugin"
            ${CMAKE_COMMAND} -E env
                "PYTHONPATH=$PYTHONPATH:${USD_LIBRARY_DIR}/python"
            "${KATANA_API_BASE_DIR}/katana"
            "--script"
            "${INSTALL_DIR}/tests/${TEST}/testPxrUsdInShipped${TEST}.py"
        WORKING_DIRECTORY
            "${INSTALL_DIR}/tests/${TEST}"
    )
endforeach()
