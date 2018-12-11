set(LIB_NAME pxrUsdInPrman)

set(CPPFILES
    register.cpp
    prmanGeometryDecorator.cpp
)

set(LIBRARIES
    katanaAttrfncApi
    katanaOpApi
    katanaPluginApi
    kind
    usd
    usdGeom
    usdKatana
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
