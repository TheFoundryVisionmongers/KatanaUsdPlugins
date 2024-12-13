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
#include "usdKatana/readLightFilter.h"

#include <stack>
#include <string>
#include <unordered_set>

#include <pxr/base/tf/stringUtils.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdr/registry.h>
#include <pxr/usd/usd/schemaRegistry.h>
#include <pxr/usd/usd/tokens.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/geometryLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/lightFilter.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdRi/splineAPI.h>

#include <FnGeolibServices/FnAttributeFunctionUtil.h>
#include <FnLogging/FnLogging.h>
#include <pystring/pystring.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readPrim.h"
#include "usdKatana/readXformable.h"
#include "usdKatana/utils.h"

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("UsdKatanaReadLightFilter");

using std::string;
using std::vector;
using FnKat::GroupBuilder;


// Similar to katana's group builder, but takes in usd attributes.
struct _UsdBuilder {
    GroupBuilder &_builder;
    double _time;

    _UsdBuilder& Set(std::string kat_name, UsdAttribute attr) {
        VtValue val;
        if (attr.HasAuthoredValueOpinion()
            && attr.Get(&val, _time)) {
            FnKat::Attribute kat_attr = UsdKatanaUtils::ConvertVtValueToKatAttr(val);
            _builder.set(kat_name.c_str(), kat_attr);
        }
        return *this;
    }

    _UsdBuilder& SetSpline(std::string kat_prefix, const std::string& valueSuffix,
                           UsdRiSplineAPI spline) {
        // Knot count
        VtFloatArray posVec;
        if (UsdAttribute posAttr = spline.GetPositionsAttr()) {
            if (!posAttr.Get(&posVec)) {
                FnLogWarn("Invalid spline positions type: " <<
                          posAttr.GetTypeName().GetAsToken().GetString() <<
                          ": " << posAttr.GetPath().GetString());
                return *this;
            }
        }

        // Interpolation
        std::string interp = "unknown";
        _builder.set(kat_prefix, FnKat::IntAttribute(posVec.size()));
        Set( kat_prefix + "_Knots", spline.GetPositionsAttr() );
        Set( kat_prefix + valueSuffix, spline.GetValuesAttr() );
        _builder.set(kat_prefix + "_Interpolation",
                     FnKat::StringAttribute(interp));
        return *this;
    }
};

void UsdKatanaReadLightFilter(const UsdPrim& filterPrim,
                              const UsdKatanaUsdInPrivateData& data,
                              UsdKatanaAttrMap& attrs)
{
    const SdfPath primPath = filterPrim.GetPath();
    const double currentTime = data.GetCurrentTime();

    GroupBuilder materialBuilder;
    GroupBuilder filterBuilder;

    // Gather prman statements
    FnKat::GroupBuilder primStatements;
    const bool prmanOutputTarget = data.hasOutputTarget("prman");
    UsdKatanaReadPrimPrmanStatements(filterPrim, currentTime, primStatements, prmanOutputTarget);
    if (prmanOutputTarget)
    {
        attrs.set("prmanStatements", primStatements.build());
        materialBuilder.set("prmanLightfilterParams", filterBuilder.build());
    }
    attrs.set("usd", primStatements.build());

    std::unordered_set<std::string> shaderIds =
        UsdKatanaUtils::GetShaderIds(filterPrim, data.GetCurrentTime());
    for (const std::string& shaderId : shaderIds)
    {
        UsdKatanaUtils::ShaderToAttrsBySdr(
            filterPrim, shaderId, data.GetCurrentTime(), materialBuilder);
    }

    attrs.set("material", materialBuilder.build());
    UsdKatanaReadXformable(UsdGeomXformable(filterPrim), data, attrs);
    attrs.set("type", FnKat::StringAttribute("light filter"));

    // This attribute makes the light filter adoptable by the GafferThree node.
    FnKat::GroupBuilder gafferBuilder;
    gafferBuilder.set("packageClass", FnAttribute::StringAttribute("LightFilterPackage"));
    attrs.set("info.gaffer", gafferBuilder.build());
}

PXR_NAMESPACE_CLOSE_SCOPE
