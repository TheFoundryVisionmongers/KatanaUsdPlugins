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
#include "usdKatana/readLight.h"
#include "pxr/pxr.h"
#include "usdKatana/attrMap.h"
#include "usdKatana/katanaLightAPI.h"
#include "usdKatana/readPrim.h"
#include "usdKatana/readXformable.h"
#include "usdKatana/utils.h"

#include "pxr/base/tf/stringUtils.h"
#include "pxr/usd/ndr/declare.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderProperty.h"

#include "pxr/usd/usdLux/cylinderLight.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/geometryLight.h"
#include "pxr/usd/usdLux/lightAPI.h"
#include "pxr/usd/usdLux/boundableLightBase.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/shadowAPI.h"
#include "pxr/usd/usdLux/shapingAPI.h"
#include "pxr/usd/usdLux/sphereLight.h"

#include <FnGeolibServices/FnAttributeFunctionUtil.h>
#include <FnLogging/FnLogging.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("UsdKatanaReadLight");

// Convert USD radius to light.size (which acts like diameter)
static void _SetLightSizeFromRadius(UsdKatanaAttrMap& geomBuilder,
                                    UsdAttribute radiusAttr,
                                    UsdTimeCode time)
{
    VtValue radiusVal;
    radiusAttr.Get(&radiusVal, time);
    float radius = radiusVal.Get<float>();
    geomBuilder.set("light.size", FnKat::FloatAttribute(2.0f * radius));
}

static const std::unordered_map<std::string, std::string> s_rendererToContextName{{"prman", "ri"},
                                                                                  {"nsi", "dl"}};
static const std::unordered_map<std::string, std::string> s_contextNameToRenderer{{"ri", "prman"},
                                                                                  {"dl", "nsi"}};

void __handleUsdLuxLightTypes(const UsdPrim& prim,
                              const UsdTimeCode& currentTimeCode,
                              const UsdKatanaUsdInPrivateData& data,
                              FnKat::GroupBuilder& materialBuilder,
                              GfVec3d& implicitScale,
                              UsdKatanaAttrMap& geomBuilder)
{
    UsdKatanaAttrMap lightBuilder;
    lightBuilder.SetUSDTimeCode(currentTimeCode);
    // UsdLuxLight
    UsdLuxLightAPI light(prim);
    lightBuilder.Set("intensity", light.GetIntensityAttr())
        .Set("exposure", light.GetExposureAttr())
        .Set("diffuse", light.GetDiffuseAttr())
        .Set("specular", light.GetSpecularAttr())
        .Set("normalize", light.GetNormalizeAttr())
        .Set("color", light.GetColorAttr())
        .Set("enableTemperature", light.GetEnableColorTemperatureAttr())
        .Set("temperature", light.GetColorTemperatureAttr())
        .Set("color", light.GetColorAttr());

    UsdLuxShapingAPI shapingAPI(prim);
    lightBuilder.Set("shapingFocus", shapingAPI.GetShapingFocusAttr())
        .Set("shapingFocusTint", shapingAPI.GetShapingFocusTintAttr())
        .Set("shapingConeAngle", shapingAPI.GetShapingConeAngleAttr())
        .Set("shapingConeSoftness", shapingAPI.GetShapingConeSoftnessAttr())
        .Set("shapingIesFile", shapingAPI.GetShapingIesFileAttr())
        .Set("shapingIesAngleScale", shapingAPI.GetShapingIesAngleScaleAttr())
        .Set("shapingIesNormalize", shapingAPI.GetShapingIesNormalizeAttr());

    UsdLuxShadowAPI shadowAPI(prim);
    lightBuilder.Set("shadowEnable", shadowAPI.GetShadowEnableAttr())
        .Set("shadowColor", shadowAPI.GetShadowColorAttr())
        .Set("shadowDistance", shadowAPI.GetShadowDistanceAttr())
        .Set("shadowFalloff", shadowAPI.GetShadowFalloffAttr())
        .Set("shadowFalloffGamma", shadowAPI.GetShadowFalloffGammaAttr());

    FnAttribute::StringAttribute lightShaderAttr;

    // Capture an implicit scale from radius etc. to bake into the xform.  This
    // ensures prman light manipulators show up with an appropriate size.
    implicitScale = GfVec3d(1);

    if (UsdLuxCylinderLight l = UsdLuxCylinderLight(prim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(), currentTimeCode);
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxCylinderLight");
        geomBuilder.Set("light.width", l.GetLengthAttr());
        lightBuilder.Set("length", l.GetLengthAttr()).Set("radius", l.GetRadiusAttr());

        float length = 1.0f;
        l.GetLengthAttr().Get(&length, currentTimeCode);
        implicitScale[0] = length;

        bool treatAsLine = false;
        l.GetTreatAsLineAttr().Get(&treatAsLine, currentTimeCode);
        if (treatAsLine)
        {
            implicitScale[1] = 0.0;
            implicitScale[2] = 0.0;
        }
        else
        {
            float radius = 1.0f;
            l.GetRadiusAttr().Get(&radius, currentTimeCode);
            implicitScale[1] = 2.0 * radius;
            implicitScale[2] = 2.0 * radius;
        }
    }
    else if (UsdLuxDiskLight l = UsdLuxDiskLight(prim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(), currentTimeCode);
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxDiskLight");
        lightBuilder.Set("radius", l.GetRadiusAttr());
        float radius = 1.0f;
        l.GetRadiusAttr().Get(&radius, currentTimeCode);
        implicitScale = GfVec3d(2.0 * radius, 2.0 * radius, 1.0);
    }
    else if (UsdLuxDistantLight l = UsdLuxDistantLight(prim))
    {
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxDistantLight");
        lightBuilder.Set("angle", l.GetAngleAttr()).Set("angleExtent", l.GetAngleAttr());
    }
    else if (UsdLuxDomeLight l = UsdLuxDomeLight(prim))
    {
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxDomeLight");
        lightBuilder.Set("textureFile", l.GetTextureFileAttr())
            .Set("textureFormat", l.GetTextureFormatAttr());
    }
    else if (UsdLuxGeometryLight l = UsdLuxGeometryLight(prim))
    {
        SdfPathVector geo;
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxGeometryLight");
        if (l.GetGeometryRel().GetForwardedTargets(&geo) && !geo.empty())
        {
            if (geo.size() > 1)
            {
                FnLogWarn(
                    "Multiple geometry targets detected for "
                    "USD geometry light "
                    << prim.GetPath() << "; using first only");
            }
            std::string kat_loc = UsdKatanaUtils::ConvertUsdPathToKatLocation(geo[0], data);
            geomBuilder.set("areaLightGeometrySource", FnKat::StringAttribute(kat_loc));
        }
    }
    else if (UsdLuxRectLight l = UsdLuxRectLight(prim))
    {
        geomBuilder.Set("light.width", l.GetWidthAttr()).Set("light.height", l.GetHeightAttr());
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxRectLight");
        lightBuilder.Set("lightColorMap", l.GetTextureFileAttr())
            .Set("textureFile", l.GetTextureFileAttr())
            .Set("width", l.GetWidthAttr())
            .Set("height", l.GetHeightAttr());

        float width;
        l.GetWidthAttr().Get(&width, currentTimeCode);
        float height;
        l.GetHeightAttr().Get(&height, currentTimeCode);
        implicitScale = GfVec3d(width, height, 1.0);
    }
    else if (UsdLuxSphereLight l = UsdLuxSphereLight(prim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(), currentTimeCode);
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxSphereLight");
        lightBuilder.Set("radius", l.GetRadiusAttr()).Set("treatAsPoint", l.GetTreatAsPointAttr());

        bool treatAsPoint = false;
        l.GetTreatAsPointAttr().Get(&treatAsPoint, currentTimeCode);
        if (treatAsPoint)
        {
            implicitScale = GfVec3d(0);
        }
        else
        {
            float radius = 1.0f;
            l.GetRadiusAttr().Get(&radius, currentTimeCode);
            implicitScale = GfVec3d(2.0 * radius);
        }
    }

    if (lightShaderAttr.isValid())
    {
        materialBuilder.set("usdLightShader", lightShaderAttr);
        materialBuilder.set("usdLightParams", lightBuilder.build());
    }
}

void __handleSdrRegistryLights(const UsdPrim& prim,
                               const UsdTimeCode& currentTimeCode,
                               FnKat::GroupBuilder& materialBuilder,
                               UsdKatanaAttrMap& geomBuilder)
{
    std::unordered_set<std::string> lightShaderIds =
        UsdKatanaUtils::GetShaderIds(prim, currentTimeCode);
    for (const std::string& shaderId : lightShaderIds)
    {
        UsdKatanaUtils::ShaderToAttrsBySdr(prim, shaderId, currentTimeCode, materialBuilder);
    }
}

void UsdKatanaReadLight(const UsdPrim& prim,
                        const UsdKatanaUsdInPrivateData& data,
                        UsdKatanaAttrMap& attrs)
{
    const UsdTimeCode currentTimeCode = data.GetCurrentTime();
    attrs.SetUSDTimeCode(currentTimeCode);
    UsdKatanaAttrMap geomBuilder;
    geomBuilder.SetUSDTimeCode(currentTimeCode);
    FnKat::GroupBuilder materialBuilder;
    GfVec3d implicitScale(1);

    UsdKatanaKatanaLightAPI katanaLightAPI(prim);
    geomBuilder.Set("centerOfInterest", katanaLightAPI.GetCenterOfInterestAttr());

    __handleSdrRegistryLights(prim, currentTimeCode, materialBuilder, geomBuilder);
    // Run the UsdLux logic after trying the Sdr Logic
    __handleUsdLuxLightTypes(
        prim, currentTimeCode, data, materialBuilder, implicitScale, geomBuilder);

    attrs.set("material", materialBuilder.build());
    attrs.set("geometry", geomBuilder.build());
    attrs.set("type", FnKat::StringAttribute("light"));

    // This attribute makes the light discoverable by the GafferThree node.
    FnKat::GroupBuilder gafferBuilder;
    gafferBuilder.set("packageClass", FnAttribute::StringAttribute("LightPackage"));
    attrs.set("info.gaffer", gafferBuilder.build());

    UsdKatanaReadXformable(UsdGeomXformable(prim), data, attrs);

    // If we have an implicit scale, put it on the top of the xform.
    if (implicitScale != GfVec3d(1))
    {
        attrs.set("xform.lightSize.scale", FnKat::DoubleAttribute(implicitScale.data(), 3, 3));
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
