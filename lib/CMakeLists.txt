add_subdirectory(katanaOpApi)
add_subdirectory(katanaPluginApi)
add_subdirectory(usdKatana)
add_subdirectory(usdKatanaSchemas)

if (NOT ${KATANA_API_VERSION} VERSION_LESS "3.0.0")
    add_subdirectory(vtKatana)
endif()
