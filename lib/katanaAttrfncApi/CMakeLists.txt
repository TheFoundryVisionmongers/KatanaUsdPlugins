set(LIB_NAME katanaAttrfncApi)

file(GLOB CPPFILES
    ${KATANA_API_SOURCE_DIR}/FnAttribute/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnPluginManager/client/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnPluginSystem/*.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttributeFunction/plugin/*.cpp
)

add_library(${LIB_NAME} STATIC ${CPPFILES})
target_include_directories(${LIB_NAME} PUBLIC ${KATANA_API_INCLUDE_DIRS})
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS ${LIB_NAME} DESTINATION "${INSTALL_SUBDIR}/lib")
