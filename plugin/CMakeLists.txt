add_subdirectory(usdIn)
add_subdirectory(usdInPrman)
add_subdirectory(usdInShipped)
if (ENABLE_USD_RENDER_INFO_PLUGIN)
    add_subdirectory(usdRenderInfoPlugin)
endif()

