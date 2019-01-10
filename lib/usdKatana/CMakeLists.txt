set(LIB_NAME usdKatana)

set(PUBLICHEADERS
    api.h

    attrMap.h
    baseMaterialHelpers.h
    blindDataObject.h
    bootstrap.h
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
    bootstrap.cpp
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
    arch
    pcp
    kind
    work
    glf
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
    katanaOpApi
    katanaPluginApi
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_PYTHON_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${PYTHON_LIBRARIES}
)

# Hack to work around https://github.com/PixarAnimationStudios/USD/issues/738
if(CMAKE_SYSTEM_NAME MATCHES Windows)
set_source_files_properties(cache.cpp PROPERTIES COMPILE_FLAGS "/Od")
endif()

add_definitions(-DUSDKATANA_EXPORTS)

add_library(${LIB_NAME} SHARED ${CPPFILES})

target_link_libraries(${LIB_NAME} PRIVATE ${LIBRARIES})
target_include_directories(${LIB_NAME} PRIVATE ${KATANA_API_INCLUDE_DIRS})

set_target_properties(${LIB_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

install(TARGETS ${LIB_NAME} DESTINATION ${INSTALL_SUBDIR}/lib)
if(CMAKE_SYSTEM_NAME MATCHES Windows)
    install(
        FILES $<TARGET_PDB_FILE:${LIB_NAME}>
        DESTINATION ${INSTALL_SUBDIR}/lib
        OPTIONAL
    )
endif()

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
