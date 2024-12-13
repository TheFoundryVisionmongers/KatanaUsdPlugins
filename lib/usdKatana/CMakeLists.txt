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
        childMaterialAPI
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
        wrapChildMaterialAPI.cpp
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

# === Tests ===
if (BUILD_KATANA_INTERNAL_USD_PLUGINS AND UNIX)

    set(PACKAGE_TESTS usdKatana.internal.Import)

    usdKatana_add_test_executable(${PACKAGE_TESTS}
        test/main.cpp
        test/readLightTest.cpp
        test/readLightFilterTest.cpp
    )

    target_compile_definitions(${PACKAGE_TESTS}
        PRIVATE
        -D_GLIBCXX_PERMIT_BACKWARD_HASH=1
    )

    file(COPY
        test/light1.usda
        test/light2.usda
        test/light3.usda
        test/light4.usda
        test/lightfilter1.usda
        DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}/test)
    file(COPY
        test/shaderDefs.usda
        test/empty.glslfx
        DESTINATION
        ${CMAKE_CURRENT_BINARY_DIR}/test/shaders)

    target_include_directories(${PACKAGE_TESTS}
        PRIVATE
        ${KATANA_API_INCLUDE_DIR}
        ${KATANA_USD_PLUGINS_SRC_ROOT}/lib
        ${KATANA_USD_PLUGINS_SRC_ROOT}/plugin
    )

    target_link_libraries(${PACKAGE_TESTS}
        PUBLIC
        usd
        sdf
        sdr
        tf
        usdShade

        PRIVATE
        ${PXR_PACKAGE}
        vtKatana
        katanaPluginApi
        katanaOpApi

        GTest::gtest
    )
endif()
