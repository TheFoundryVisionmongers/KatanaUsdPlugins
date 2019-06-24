set(PXR_PACKAGE pxrUsdInPrman)

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        katanaAttrfncApi
        katanaOpApi
        katanaPluginApi
        kind
        sdf
        tf
        usd
        usdGeom
        usdKatana
        vtKatana
        ${Boost_PYTHON_LIBRARY}
    
    PRIVATE_HEADERS
        declarePackageOps.h
    
    CPPFILES
        register.cpp
        prmanGeometryDecorator.cpp        
)
