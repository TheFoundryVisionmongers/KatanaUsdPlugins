set(LIB_NAME katanaOpApi)

file(GLOB CPPFILES
    ${KATANA_API_SOURCE_DIR}/FnAsset/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnAsset/plugin/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttribute/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnGeolib/op/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnGeolib/util/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnGeolibServices/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnLogging/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnPluginManager/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnPluginSystem/*.cpp
    ${KATANA_API_SOURCE_DIR}/pystring/.cpp
)

add_library(${LIB_NAME} STATIC ${CPPFILES})
target_include_directories(${LIB_NAME} PUBLIC ${KATANA_API_INCLUDE_DIRS})
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS ${LIB_NAME} DESTINATION "${INSTALL_SUBDIR}/lib")
