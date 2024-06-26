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
#include <vector>

#include <pxr/base/gf/gamma.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/pointBased.h>

#include <FnAPI/FnAPI.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readGprim.h"
#include "usdKatana/readXformable.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

#if KATANA_VERSION_MAJOR >= 3
#include "vtKatana/array.h"
#endif // KATANA_VERSION_MAJOR >= 3

#include <FnLogging/FnLogging.h>

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("UsdKatanaReadGprim");

void UsdKatanaReadGprim(const UsdGeomGprim& gprim,
                        const UsdKatanaUsdInPrivateData& data,
                        UsdKatanaAttrMap& attrs)
{
    UsdKatanaReadXformable(gprim, data, attrs);
}

FnKat::Attribute UsdKatanaGeomGetDisplayColorAttr(const UsdGeomGprim& gprim,
                                                  const UsdKatanaUsdInPrivateData& data)
{
    // Eval color.
    VtArray<GfVec3f> color;
    if (!gprim.GetDisplayColorPrimvar().ComputeFlattened(
        &color, data.GetCurrentTime())) {
        return FnKat::Attribute();
    }

    if (color.size() < 1) {
        FnLogWarn("Size 0 displaycolor from "<< gprim.GetPrim().GetName());
        return FnKat::Attribute();
    }

    // Build Katana attribute.
    // XXX(USD): what about alpha->opacity? warn?
    FnKat::FloatBuilder colorBuilder(3);
    std::vector<float> colorVec;
    colorVec.resize(3);
    colorVec[0] = color[0][0];
    colorVec[1] = color[0][1];
    colorVec[2] = color[0][2];
    colorBuilder.set(colorVec);

    FnKat::GroupBuilder groupBuilder;
    groupBuilder.set("inputType", FnKat::StringAttribute("color3"));
    groupBuilder.set("scope", FnKat::StringAttribute("primitive"));
    groupBuilder.set("value", colorBuilder.build());
    return groupBuilder.build();
}

Foundry::Katana::Attribute UsdKatanaGeomGetWindingOrderAttr(const UsdGeomGprim& gprim,
                                                            const UsdKatanaUsdInPrivateData& data)
{
    TfToken orientation = UsdGeomTokens->rightHanded;
    gprim.GetOrientationAttr().Get(&orientation);

    // Due to an inconsistency in Katana versions <4.5, this function reversed orientations.
    // For compatibility, preserve that reversal when building against older versions.
#if ((KATANA_VERSION_MAJOR == 4) && (KATANA_VERSION_MINOR >= 5)) || (KATANA_VERSION_MAJOR > 4)
    if (orientation == UsdGeomTokens->leftHanded)
#else
    if (orientation == UsdGeomTokens->rightHanded)
#endif
    {
        return FnKat::StringAttribute("clockwise");
    }
    else
    {
        return FnKat::StringAttribute("counterclockwise");
    }
}

namespace {

#if KATANA_VERSION_MAJOR >= 3

template <typename T_USD, typename T_ATTR>
FnKat::Attribute _ConvertGeomAttr(const UsdAttribute& usdAttr,
                                  const int tupleSize,
                                  const UsdKatanaUsdInPrivateData& data)
{
    if (!usdAttr.HasValue())
    {
        return FnKat::Attribute();
    }

    const double currentTime = data.GetCurrentTime();
    const std::vector<double>& motionSampleTimes = data.GetMotionSampleTimes(usdAttr);

    // Flag to check if we discovered the topology is varying, in
    // which case we only output the sample at the curent frame.
    bool varyingTopology = false;

    const bool isMotionBackward = data.IsMotionBackward();

    std::map<float, VtArray<T_USD>> timeToSampleMap;
    for (double relSampleTime : motionSampleTimes) {
        double time = currentTime + relSampleTime;

        // Eval attr.
        VtArray<T_USD> attrArray;
        usdAttr.Get(&attrArray, time);

        if (!timeToSampleMap.empty()) {
            if (timeToSampleMap.begin()->second.size() != attrArray.size()) {
                timeToSampleMap.clear();
                varyingTopology = true;
                break;
            }
        }
        float correctedSampleTime =
            isMotionBackward ? UsdKatanaUtils::ReverseTimeSample(relSampleTime) : relSampleTime;
        timeToSampleMap.insert({correctedSampleTime, attrArray});
    }

    // Varying topology was found, build for the current frame only.
    if (varyingTopology) {
        VtArray<T_USD> attrArray;
        usdAttr.Get(&attrArray, currentTime);
        return VtKatanaMapOrCopy<T_USD>(attrArray);
    } else {
        return VtKatanaMapOrCopy<T_USD>(timeToSampleMap);
    }
}

#else

template <typename T_USD, typename T_ATTR>
FnKat::Attribute _ConvertGeomAttr(const UsdAttribute& usdAttr,
                                  const int tupleSize,
                                  const UsdKatanaUsdInPrivateData& data)
{
    if (!usdAttr.HasValue())
    {
        return FnKat::Attribute();
    }

    const double currentTime = data.GetCurrentTime();
    const std::vector<double>& motionSampleTimes = data.GetMotionSampleTimes(usdAttr);

    // Flag to check if we discovered the topology is varying, in
    // which case we only output the sample at the curent frame.
    bool varyingTopology = false;

    // Used to compare value sizes to identify varying topology.
    int arraySize = -1;

    const bool isMotionBackward = data.IsMotionBackward();

    FnKat::DataBuilder<T_ATTR> attrBuilder(tupleSize);
    TF_FOR_ALL(iter, motionSampleTimes)
    {
        double relSampleTime = *iter;
        double time = currentTime + relSampleTime;

        // Eval attr.
        VtArray<T_USD> attrArray;
        usdAttr.Get(&attrArray, time);

        if (arraySize == -1) {
            arraySize = attrArray.size();
        } else if ( attrArray.size() != static_cast<size_t>(arraySize) ) {
            // Topology has changed. Don't create this or subsequent samples.
            varyingTopology = true;
            break;
        }

        std::vector<typename T_ATTR::value_type>& attrVec = attrBuilder.get(
            isMotionBackward ? UsdKatanaUtils::ReverseTimeSample(relSampleTime) : relSampleTime);

        UsdKatanaUtils::ConvertArrayToVector(attrArray, &attrVec);
    }

    // Varying topology was found, build for the current frame only.
    if (varyingTopology)
    {
        FnKat::DataBuilder<T_ATTR> defaultBuilder(tupleSize);
        VtArray<T_USD> attrArray;

        usdAttr.Get(&attrArray, currentTime);
        std::vector<typename T_ATTR::value_type> &attrVec = defaultBuilder.get(0);
        UsdKatanaUtils::ConvertArrayToVector(attrArray, &attrVec);

        return defaultBuilder.build();
    }

    return attrBuilder.build();
}

#endif // KATANA_VERSION_MAJOR >= 3

} // anon namespace

FnKat::Attribute UsdKatanaGeomGetPAttr(const UsdGeomPointBased& points,
                                       const UsdKatanaUsdInPrivateData& data)
{
    FnKat::Attribute skinnedPointsAttr;
    if (data.GetEvaluateUsdSkelBindings())
    {
        skinnedPointsAttr = UsdKatanaUtils::ApplySkinningToPoints(points, data);
    }

    if (skinnedPointsAttr.isValid())
    {
        return skinnedPointsAttr;
    }
    return _ConvertGeomAttr<GfVec3f, FnKat::FloatAttribute>(
        points.GetPointsAttr(), 3, data);
}

Foundry::Katana::Attribute UsdKatanaGeomGetNormalAttr(const UsdGeomPointBased& points,
                                                      const UsdKatanaUsdInPrivateData& data)
{
    return _ConvertGeomAttr<GfVec3f, FnKat::FloatAttribute>(
            points.GetNormalsAttr(), 3, data);
}

Foundry::Katana::Attribute UsdKatanaGeomGetVelocityAttr(const UsdGeomPointBased& points,
                                                        const UsdKatanaUsdInPrivateData& data)
{
    return _ConvertGeomAttr<GfVec3f, FnKat::FloatAttribute>(
            points.GetVelocitiesAttr(), 3, data);

}

Foundry::Katana::Attribute UsdKatanaGeomGetAccelerationAttr(const UsdGeomPointBased& points,
                                                            const UsdKatanaUsdInPrivateData& data)
{
    return _ConvertGeomAttr<GfVec3f, FnKat::FloatAttribute>(
            points.GetAccelerationsAttr(), 3, data);
}

PXR_NAMESPACE_CLOSE_SCOPE

