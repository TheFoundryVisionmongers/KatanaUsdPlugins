set(PXR_PACKAGE usdInShipped)

set(vtKatana_LIBRARY "")
if (NOT ${KATANA_API_VERSION} VERSION_LESS "3.0.0")
    set(vtKatana_LIBRARY "vtKatana")
endif()

pxr_plugin(${PXR_PACKAGE}
    KATANA_PLUGIN
    LIBRARIES
        katanaPluginApi
        usdKatana
        usdShade
        usdLux
        usdVol
        ${vtKatana_LIBRARY}
        Boost::system

    PRIVATE_CLASSES
        attrfnc_materialReference

    PRIVATE_HEADERS
        declareCoreOps.h

    CPPFILES
        register.cpp

        basisCurves.cpp
        camera.cpp
        constraints.cpp
        geomSubset.cpp
        light.cpp
        lightFilter.cpp
        material.cpp
        materialsGroup.cpp
        mesh.cpp
        volume.cpp
        openVDBAsset.cpp
        model.cpp
        nurbsPatch.cpp
        pointInstancer.cpp
        points.cpp
        resolveMaterialBindings.cpp
        scope.cpp
        uiUtils.cpp
        xform.cpp
)
