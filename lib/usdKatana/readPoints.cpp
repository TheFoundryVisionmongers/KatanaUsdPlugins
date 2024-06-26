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
#include "usdKatana/readPoints.h"

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/points.h>

#include <FnAPI/FnAPI.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readGprim.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

#if KATANA_VERSION_MAJOR >= 3
#include "vtKatana/array.h"
#endif // KATANA_VERSION_MAJOR >= 3

PXR_NAMESPACE_OPEN_SCOPE


static FnKat::Attribute
_GetWidthAttr(const UsdGeomPoints& points, double currentTime)
{
    VtFloatArray widths;
    if (!points.GetWidthsAttr().Get(&widths, currentTime))
    {
        return FnKat::Attribute();
    }

#if KATANA_VERSION_MAJOR >= 3
    return VtKatanaMapOrCopy(widths);
#else
    FnKat::FloatBuilder widthsBuilder(1);
    widthsBuilder.set(std::vector<float>(widths.begin(), widths.end()));


    return widthsBuilder.build();
#endif // KATANA_VERSION_MAJOR >= 3
}

void UsdKatanaReadPoints(const UsdGeomPoints& points,
                         const UsdKatanaUsdInPrivateData& data,
                         UsdKatanaAttrMap& attrs)
{
    const double currentTime = data.GetCurrentTime();

    //
    // Set all general attributes for a gprim type.
    //

    UsdKatanaReadGprim(points, data, attrs);

    //
    // Set more specific Katana type.
    //

    attrs.set("type", FnKat::StringAttribute("pointcloud"));

    //
    // Construct the 'geometry' attribute.
    //

    // position
    attrs.set("geometry.point.P", UsdKatanaGeomGetPAttr(points, data));

    // velocity
    FnKat::Attribute velocitiesAttr = UsdKatanaGeomGetVelocityAttr(points, data);
    if (velocitiesAttr.isValid())
    {
        attrs.set("geometry.point.v", velocitiesAttr);
    }

    // acceleration
    FnKat::Attribute accelAttr = UsdKatanaGeomGetAccelerationAttr(points, data);
    if (accelAttr.isValid())
    {
        attrs.set("geometry.point.accel", accelAttr);
    }

    // normals
    FnKat::Attribute normalsAttr = UsdKatanaGeomGetNormalAttr(points, data);
    if (normalsAttr.isValid())
    {
        // XXX RfK doesn't support uniform curve normals.
        TfToken interp = points.GetNormalsInterpolation();
        if (interp == UsdGeomTokens->faceVarying
         || interp == UsdGeomTokens->varying
         || interp == UsdGeomTokens->vertex) {
            attrs.set("geometry.point.N", normalsAttr);
        }
    }

    // width
    FnKat::Attribute widthsAttr = _GetWidthAttr(points, currentTime);
    if (widthsAttr.isValid())
    {
        attrs.set("geometry.point.width", widthsAttr);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

