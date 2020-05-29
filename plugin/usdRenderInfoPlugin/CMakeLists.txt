
# add_library(UsdRenderInfo MODULE
#     main.cpp
#     UsdRenderInfoPlugin.cpp
# )

# add_definitions(-DFNLOGGING_STATIC)

# target_include_directories(UsdRenderInfo
#     PRIVATE
#     C:/k_3.6_r/kb_Release/Dist/plugin_apis/include
# )

# target_link_libraries(UsdRenderInfo
#     PRIVATE
#         C:/k_3.6_r/kb_Release/Dist/plugin_apis/lib/FnGeolibPluginApis.lib
#         # katanaPluginApi
# )

# set_target_properties(UsdRenderInfo PROPERTIES PREFIX "")
# install(TARGETS UsdRenderInfo DESTINATION Libs)


pxr_plugin(UsdRenderInfo
    KATANA_PLUGIN
    LIBRARIES
        usdShade
        ndr
        sdr
        sdf
        gf
        tf
        katanaPluginApi
        usdKatana
        vtKatana
        Boost::system

    CPPFILES
        main.cpp
        UsdRenderInfoPlugin.cpp
)
