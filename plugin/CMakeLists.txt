add_subdirectory(pxrUsdIn)
add_subdirectory(pxrUsdInPrman)
add_subdirectory(pxrUsdInShipped)
if (ENABLE_USD_RENDER_INFO_PLUGIN)
    add_subdirectory(usdRenderInfoPlugin)
endif()

