set(PXR_PACKAGE usdKatana)

set(vtKatana_LIBRARY "")
if (NOT ${KATANA_API_VERSION} VERSION_LESS "3.0.0")
    set(vtKatana_LIBRARY "vtKatana")
endif()

pxr_shared_library(${PXR_PACKAGE}
    LIBRARIES
        vt
        sdf
        usdHydra
        usdSkel
        usdGeom
        usdVol
        usdImaging
        usdRi
        usdUI
        usdUtils
        katanaPluginApi
        ${vtKatana_LIBRARY}
        Boost::system
        Boost::regex
        Boost::thread
        Boost::filesystem

    PUBLIC_CLASSES
        attrMap
        baseMaterialHelpers
        blindDataObject
        cache
        debugCodes
        locks
        tokens
        katanaLightAPI
        lookAPI
        utils

        usdInArgs
        usdInPrivateData
        usdInPluginRegistry

        readBasisCurves
        readBlindData
        readCamera
        readConstraintTarget
        readPrimitive
        readGprim
        readGeomSubset
        readLight
        readLightFilter
        readMaterial
        readMesh
        readModel
        readNurbsPatch
        readPointInstancer
        readPoints
        readPrim
        readVolume
        readOpenVDBAsset
        readXformable

        bootstrap

    PUBLIC_HEADERS
        api.h

    PYMODULE_CPPFILES
        wrapBlindDataObject.cpp
        wrapCache.cpp
        wrapKatanaLightAPI.cpp
        wrapLookAPI.cpp
        module.cpp

    PYMODULE_FILES
        __init__.py

    RESOURCE_FILES
        plugInfo.json
        generatedSchema.usda
        schema.usda:usdKatana/schema.usda
)

# Hack to work around https://github.com/PixarAnimationStudios/USD/issues/738
if(CMAKE_SYSTEM_NAME MATCHES Windows)
set_source_files_properties(cache.cpp PROPERTIES COMPILE_FLAGS "/Od")
endif()
