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
#include "usdKatana/readBasisCurves.h"

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/basisCurves.h>

#include <FnAPI/FnAPI.h>
#include <FnAttribute/FnDataBuilder.h>
#include <FnLogging/FnLogging.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readGprim.h"
#include "usdKatana/usdInPrivateData.h"

#if KATANA_VERSION_MAJOR >= 3
#include "vtKatana/array.h"
#endif // KATANA_VERSION_MAJOR >= 3

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("UsdKatanaReadBasisCurves");

static void _SetCurveAttrs(UsdKatanaAttrMap& attrs,
                           const UsdGeomBasisCurves& basisCurves,
                           double currentTime)
{
    VtIntArray vtxCts;
    basisCurves.GetCurveVertexCountsAttr().Get(&vtxCts, currentTime);

#if KATANA_VERSION_MAJOR >= 3
    auto countsAttr = VtKatanaMapOrCopy(vtxCts);
    attrs.set("geometry.numVertices", countsAttr);
#else
    std::vector<int> ctsVec(vtxCts.begin(), vtxCts.end());

    FnKat::IntBuilder numVertsBuilder(1);
    std::vector<int> &numVerts = numVertsBuilder.get();
    numVerts = ctsVec;

    attrs.set("geometry.numVertices", numVertsBuilder.build());
#endif // KATANA_VERSION_MAJOR >= 3

    VtFloatArray widths;
    basisCurves.GetWidthsAttr().Get(&widths, currentTime);
    TfToken interpolation = basisCurves.GetWidthsInterpolation();
    size_t numWidths = widths.size();
    if (numWidths == 1 && interpolation == UsdGeomTokens->constant)
    {
        attrs.set("geometry.constantWidth",
            FnKat::FloatAttribute(widths[0]));
    }
    else if (numWidths > 1 && interpolation == UsdGeomTokens->vertex)
    {
#if KATANA_VERSION_MAJOR >= 3
        auto widthsAttr = VtKatanaMapOrCopy(widths);
        attrs.set("geometry.point.width", widthsAttr);
#else
        FnKat::FloatBuilder widthsBuilder(1);
        widthsBuilder.set(
            std::vector<float>(widths.begin(), widths.end()));
        attrs.set("geometry.point.width", widthsBuilder.build());
#endif // KATANA_VERSION_MAJOR >= 3
    }
    else if (numWidths >= 1)
    {
        // Align on what AlembicIn does in that case
        FnKat::StringAttribute scopeAttr =
            FnKat::StringAttribute((interpolation == UsdGeomTokens->faceVarying) ? "vertex"
                                   : (interpolation == UsdGeomTokens->varying)   ? "vertex"
                                   : (interpolation == UsdGeomTokens->vertex)    ? "point"
                                   : (interpolation == UsdGeomTokens->uniform)   ? "face"
                                                                                 : "primitive");
        attrs.set("geometry.arbitrary.width.scope", scopeAttr);
        attrs.set("geometry.arbitrary.width.inputType", FnKat::StringAttribute("float"));

        auto widthsAttr = VtKatanaMapOrCopy(widths);
        attrs.set("geometry.arbitrary.width.value", widthsAttr);
    }

    TfToken curveType;
    basisCurves.GetTypeAttr().Get(&curveType, currentTime);
    attrs.set("geometry.degree",
        FnKat::IntAttribute(curveType == UsdGeomTokens->linear ? 1 : 3));

    TfToken wrapType;
    basisCurves.GetWrapAttr().Get(&wrapType, currentTime);
    attrs.set("geometry.closed", FnKat::IntAttribute(wrapType == UsdGeomTokens->periodic ? 1 : 0));

    TfToken basisType;
    basisCurves.GetBasisAttr().Get(&basisType, currentTime);
    attrs.set("geometry.vstep", FnKat::IntAttribute(basisType == UsdGeomTokens->bezier ? 3 : 1));
}

void UsdKatanaReadBasisCurves(const UsdGeomBasisCurves& basisCurves,
                              const UsdKatanaUsdInPrivateData& data,
                              UsdKatanaAttrMap& attrs)
{
    const bool prmanOutputTarget = data.hasOutputTarget("prman");
    //
    // Set all general attributes for a gprim type.
    //

    UsdKatanaReadGprim(basisCurves, data, attrs);

    //
    // Set more specific Katana type.
    //

    attrs.set("type", FnKat::StringAttribute("curves"));

    //
    // Set 'prmanStatements' attribute.
    //
    // The curves geometry.basis values are defined in the Katana attribute
    // conventions which can be found here: 
    // https://learn.foundry.com/katana/dev-guide/AttributeConventions/GeometryTypes.html#curves

    TfToken basis;
    basisCurves.GetBasisAttr().Get(&basis);
    if (basis == UsdGeomTokens->bezier)
    {
        if (prmanOutputTarget)
        {
            attrs.set("prmanStatements.basis.u",
                            FnKat::StringAttribute("bezier"));
            attrs.set("prmanStatements.basis.v",
                            FnKat::StringAttribute("bezier"));
        }
        attrs.set("geometry.basis", FnKat::IntAttribute(1));
    }
    else if (basis == UsdGeomTokens->bspline)
    {
        if(prmanOutputTarget)
        {
            attrs.set("prmanStatements.basis.u",
                            FnKat::StringAttribute("b-spline"));
            attrs.set("prmanStatements.basis.v",
                            FnKat::StringAttribute("b-spline"));
        }
        attrs.set("geometry.basis", FnKat::IntAttribute(2));
    }
    else if (basis == UsdGeomTokens->catmullRom)
    {
        if(prmanOutputTarget)
        {
            attrs.set("prmanStatements.basis.u",
                            FnKat::StringAttribute("catmull-rom"));
            attrs.set("prmanStatements.basis.v",
                            FnKat::StringAttribute("catmull-rom"));
        }
        attrs.set("geometry.basis", FnKat::IntAttribute(3));
    }
    else if (basis == UsdGeomTokens->hermite)
    {
        if(prmanOutputTarget)
        {
            attrs.set("prmanStatements.basis.u",
                            FnKat::StringAttribute("hermite"));
            attrs.set("prmanStatements.basis.v",
                            FnKat::StringAttribute("hermite"));
        }
        attrs.set("geometry.basis", FnKat::IntAttribute(4));
    }
    else if (basis == UsdGeomTokens->power)
    {
        if(prmanOutputTarget)
        {
            attrs.set("prmanStatements.basis.u",
                            FnKat::StringAttribute("power"));
            attrs.set("prmanStatements.basis.v",
                            FnKat::StringAttribute("power"));
        }
        attrs.set("geometry.basis", FnKat::IntAttribute(5));
    }
    else {
        FnLogWarn("Ignoring unsupported curve basis, " << basis.GetString()
                << ", in " << basisCurves.GetPath().GetString());
    }

    //
    // Construct the 'geometry' attribute.
    //

    _SetCurveAttrs(attrs, basisCurves, data.GetCurrentTime());
    
    // position
    attrs.set("geometry.point.P", UsdKatanaGeomGetPAttr(basisCurves, data));

    // normals
    FnKat::Attribute normalsAttr = UsdKatanaGeomGetNormalAttr(basisCurves, data);
    if (normalsAttr.isValid())
    {
        // XXX RfK doesn't support uniform normals for curves.
        // Additionally, varying and facevarying may not be correct for
        // periodic cubic curves.
        TfToken interp = basisCurves.GetNormalsInterpolation();
        if (interp == UsdGeomTokens->faceVarying
         || interp == UsdGeomTokens->varying
         || interp == UsdGeomTokens->vertex) {
            attrs.set("geometry.point.N", normalsAttr);
        }
    }

    // velocity
    FnKat::Attribute velocityAttr = UsdKatanaGeomGetVelocityAttr(basisCurves, data);
    if (velocityAttr.isValid())
    {
        attrs.set("geometry.point.v", velocityAttr);
    }

    // acceleration
    FnKat::Attribute accelAttr = UsdKatanaGeomGetAccelerationAttr(basisCurves, data);
    if (accelAttr.isValid())
    {
        attrs.set("geometry.point.accel", accelAttr);
    }

    // Add SPT_HwColor primvar
    attrs.set("geometry.arbitrary.SPT_HwColor",
              UsdKatanaGeomGetDisplayColorAttr(basisCurves, data));
}

PXR_NAMESPACE_CLOSE_SCOPE

