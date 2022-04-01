// These files began life as part of the main USD distribution
// https://github.com/PixarAnimationStudios/USD.
// In 2019, Foundry and Pixar agreed Foundry should maintain and curate
// these plug-ins, and they moved to
// https://github.com/TheFoundryVisionmongers/KatanaUsdPlugins
// under the same Modified Apache 2.0 license, as shown below.
//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "usdInShipped/declareCoreOps.h"

#include <FnGeolib/op/FnGeolibOp.h>

#include "pxr/pxr.h"
#include "usdKatana/bootstrap.h"
#include "usdKatana/usdInPluginRegistry.h"
#include "vtKatana/bootstrap.h"

#include "pxr/usd/kind/registry.h"
#include "pxr/usd/usdGeom/basisCurves.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/nurbsPatch.h"
#include "pxr/usd/usdGeom/pointInstancer.h"
#include "pxr/usd/usdGeom/points.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/subset.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdLux/boundableLightBase.h"
#include "pxr/usd/usdLux/cylinderLight.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/geometryLight.h"
#include "pxr/usd/usdLux/lightFilter.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/sphereLight.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdSkel/root.h"

#include "usdInShipped/attrfnc_materialReference.h"

PXR_NAMESPACE_USING_DIRECTIVE

void registerUsdInShippedLightLightListFnc();
void registerUsdInShippedLightFilterLightListFnc();
void registerUsdInShippedUiUtils();
void registerUsdInResolveMaterialBindingsOp();

DEFINE_GEOLIBOP_PLUGIN(UsdInCore_XformOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_ScopeOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_MeshOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_GeomSubsetOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_NurbsPatchOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_PointInstancerOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_PointsOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_BasisCurvesOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_LookOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_LightOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_LightFilterOp)

DEFINE_GEOLIBOP_PLUGIN(UsdInCore_ModelOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_CameraOp)

DEFINE_GEOLIBOP_PLUGIN(UsdInCore_ConstraintsOp)
DEFINE_GEOLIBOP_PLUGIN(UsdInCore_LooksGroupOp)

DEFINE_ATTRIBUTEFUNCTION_PLUGIN(MaterialReferenceAttrFnc);
DEFINE_ATTRIBUTEFUNCTION_PLUGIN(LibraryMaterialNamesAttrFnc);

void registerPlugins()
{
    USD_OP_REGISTER_PLUGIN(UsdInCore_XformOp, "UsdInCore_XformOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_ScopeOp, "UsdInCore_ScopeOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_MeshOp, "UsdInCore_MeshOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_GeomSubsetOp, "UsdInCore_GeomSubsetOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_NurbsPatchOp, "UsdInCore_NurbsPatchOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_PointInstancerOp, "UsdInCore_PointInstancerOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_PointsOp, "UsdInCore_PointsOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_BasisCurvesOp, "UsdInCore_BasisCurvesOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_LookOp, "UsdInCore_LookOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_LightOp, "UsdInCore_LightOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_LightFilterOp, "UsdInCore_LightFilterOp", 0, 1);

    USD_OP_REGISTER_PLUGIN(UsdInCore_ModelOp, "UsdInCore_ModelOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_CameraOp, "UsdInCore_CameraOp", 0, 1);

    USD_OP_REGISTER_PLUGIN(UsdInCore_ConstraintsOp, "UsdInCore_ConstraintsOp", 0, 1);
    USD_OP_REGISTER_PLUGIN(UsdInCore_LooksGroupOp, "UsdInCore_LooksGroupOp", 0, 1);

    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomXform>("UsdInCore_XformOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomScope>("UsdInCore_ScopeOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomMesh>("UsdInCore_MeshOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomSubset>("UsdInCore_GeomSubsetOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomNurbsPatch>("UsdInCore_NurbsPatchOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomPointInstancer>(
        "UsdInCore_PointInstancerOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomPoints>("UsdInCore_PointsOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomBasisCurves>("UsdInCore_BasisCurvesOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdShadeMaterial>("UsdInCore_LookOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdSkelRoot>("UsdInCore_XformOp");

    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxBoundableLightBase>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxCylinderLight>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxDomeLight>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxGeometryLight>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxDistantLight>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxSphereLight>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxDiskLight>("UsdInCore_LightOp");
    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdLuxRectLight>("UsdInCore_LightOp");

    UsdKatanaUsdInPluginRegistry::RegisterUsdType<UsdGeomCamera>("UsdInCore_CameraOp");

    // register a default op to handle prims with unknown types
    UsdKatanaUsdInPluginRegistry::RegisterUnknownUsdType("UsdInCore_ScopeOp");

    UsdKatanaUsdInPluginRegistry::RegisterKind(KindTokens->model, "UsdInCore_ModelOp");
    UsdKatanaUsdInPluginRegistry::RegisterKind(KindTokens->subcomponent, "UsdInCore_ModelOp");

    registerUsdInShippedLightLightListFnc();
    registerUsdInShippedLightFilterLightListFnc();
    registerUsdInShippedUiUtils();
    registerUsdInResolveMaterialBindingsOp();

    REGISTER_PLUGIN(MaterialReferenceAttrFnc, "UsdInMaterialReference", 0, 1);
    REGISTER_PLUGIN(LibraryMaterialNamesAttrFnc, "UsdInLibraryMaterialNames", 0, 1);

    UsdKatanaBootstrap();
    VtKatanaBootstrap();
}
