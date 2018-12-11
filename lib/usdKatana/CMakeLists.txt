set(LIB_NAME usdKatana)

set(PUBLICHEADERS
    api.h

    attrMap.h
    baseMaterialHelpers.h
    blindDataObject.h
    cache.h
    debugCodes.h
    locks.h
    tokens.h
    lookAPI.h
    utils.h

    usdInArgs.h
    usdInPrivateData.h
    usdInPluginRegistry.h

    readBasisCurves.h
    readBlindData.h
    readCamera.h
    readConstraintTarget.h
    readGprim.h
    readGeomSubset.h
    readLight.h
    readLightFilter.h
    readMaterial.h
    readMesh.h
    readModel.h
    readNurbsPatch.h
    readPointInstancer.h
    readPoints.h
    readPrim.h
    readXformable.h
)

set(CPPFILES
    attrMap.cpp
    baseMaterialHelpers.cpp
    blindDataObject.cpp
    cache.cpp
    debugCodes.cpp
    locks.cpp
    tokens.cpp
    lookAPI.cpp
    utils.cpp

    usdInArgs.cpp
    usdInPrivateData.cpp
    usdInPluginRegistry.cpp

    readBasisCurves.cpp
    readBlindData.cpp
    readCamera.cpp
    readConstraintTarget.cpp
    readGprim.cpp
    readGeomSubset.cpp
    readLight.cpp
    readLightFilter.cpp
    readMaterial.cpp
    readMesh.cpp
    readModel.cpp
    readNurbsPatch.cpp
    readPointInstancer.cpp
    readPoints.cpp
    readPrim.cpp
    readXformable.cpp

    wrapBlindDataObject.cpp
    wrapCache.cpp
    wrapLookAPI.cpp
    module.cpp
)

set(LIBRARIES
    plug
    gf
    tf
    trace
    vt
    ar
    sdf
    usd
    usdHydra
    usdImaging
    usdImagingGL
    usdShade
    usdSkel
    usdGeom
    usdLux
    usdRi
    usdUI
    usdUtils
    cameraUtil
    katanaAttrfncApi
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
)

add_library(${LIB_NAME} SHARED ${CPPFILES})

target_link_libraries(${LIB_NAME} PRIVATE ${LIBRARIES})
target_include_directories(${LIB_NAME} PRIVATE ${KATANA_API_INCLUDE_DIRS})

set_target_properties(${LIB_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

install(TARGETS ${LIB_NAME} DESTINATION ${INSTALL_SUBDIR}/lib)

install(
    FILES
        ${PUBLICHEADERS}
    DESTINATION
        ${INSTALL_SUBDIR}/include/${LIB_NAME}
)
install(
    FILES
        plugInfo.json
        generatedSchema.usda
    DESTINATION
        ${INSTALL_SUBDIR}/lib/usd/${LIB_NAME}/resources
)
install(
    FILES
        schema.usda
    DESTINATION
        ${INSTALL_SUBDIR}/lib/usd/${LIB_NAME}/resources/${LIB_NAME}
)
