set(LIB_NAME pxrUsdIn)

set(CPPFILES
    pxrUsdIn.cpp
)

set(LIBRARIES
    usdKatana
    katanaOpApi
    katanaPluginApi
    katanaAttrfncApi
    tf
    sdf
    usd
    usdLux
    usdUtils
    usdGeom
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