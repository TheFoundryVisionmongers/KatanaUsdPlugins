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
#include "usdKatana/lightAPI.h"
#include "usdKatana/readPrim.h"
#include "usdKatana/readXformable.h"
#include "usdKatana/utils.h"

#include "pxr/base/tf/stringUtils.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderProperty.h"

#include "pxr/usd/usdLux/cylinderLight.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/geometryLight.h"
#include "pxr/usd/usdLux/light.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/shadowAPI.h"
#include "pxr/usd/usdLux/shapingAPI.h"
#include "pxr/usd/usdLux/sphereLight.h"

#include <FnGeolibServices/FnAttributeFunctionUtil.h>
#include <FnLogging/FnLogging.h>

#include <string>
#include <unordered_map>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("PxrUsdKatanaReadLight");

// Convert USD radius to light.size (which acts like diameter)
static void _SetLightSizeFromRadius(PxrUsdKatanaAttrMap& geomBuilder,
                                    UsdAttribute radiusAttr,
                                    UsdTimeCode time)
{
    VtValue radiusVal;
    radiusAttr.Get(&radiusVal, time);
    float radius = radiusVal.Get<float>();
    geomBuilder.set("light.size", FnKat::FloatAttribute(2.0f * radius));
}

static const std::unordered_map<std::string, std::string> s_rendererToContextName{{"prman", "ri"}};

void __handleUsdLuxLightTypes(const UsdLuxLight& light,
                              const UsdPrim& lightPrim,
                              const UsdTimeCode& currentTimeCode,
                              const PxrUsdKatanaUsdInPrivateData& data,
                              FnKat::GroupBuilder& materialBuilder,
                              PxrUsdKatanaAttrMap& geomBuilder)
{
    PxrUsdKatanaAttrMap lightBuilder;
    lightBuilder.SetUSDTimeCode(currentTimeCode);
    // UsdLuxLight
    lightBuilder.Set("intensity", light.GetIntensityAttr())
        .Set("exposure", light.GetExposureAttr())
        .Set("diffuse", light.GetDiffuseAttr())
        .Set("specular", light.GetSpecularAttr())
        .Set("normalize", light.GetNormalizeAttr())
        .Set("color", light.GetColorAttr())
        .Set("enableTemperature", light.GetEnableColorTemperatureAttr())
        .Set("temperature", light.GetColorTemperatureAttr())
        .Set("color", light.GetColorAttr());

    UsdLuxShapingAPI shapingAPI(lightPrim);
    lightBuilder.Set("shapingFocus", shapingAPI.GetShapingFocusAttr())
        .Set("shapingFocusTint", shapingAPI.GetShapingFocusTintAttr())
        .Set("shapingConeAngle", shapingAPI.GetShapingConeAngleAttr())
        .Set("shapingConeSoftness", shapingAPI.GetShapingConeSoftnessAttr())
        .Set("shapingIesFile", shapingAPI.GetShapingIesFileAttr())
        .Set("shapingIesAngleScale", shapingAPI.GetShapingIesAngleScaleAttr())
        .Set("shapingIesNormalize", shapingAPI.GetShapingIesNormalizeAttr());

    UsdLuxShadowAPI shadowAPI(lightPrim);
    lightBuilder.Set("shadowEnable", shadowAPI.GetShadowEnableAttr())
        .Set("shadowColor", shadowAPI.GetShadowColorAttr())
        .Set("shadowDistance", shadowAPI.GetShadowDistanceAttr())
        .Set("shadowFalloff", shadowAPI.GetShadowFalloffAttr())
        .Set("shadowFalloffGamma", shadowAPI.GetShadowFalloffGammaAttr());

    FnAttribute::StringAttribute lightShaderAttr;

    if (UsdLuxCylinderLight l = UsdLuxCylinderLight(lightPrim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(), currentTimeCode);
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxCylinderLight");
        geomBuilder.Set("light.width", l.GetLengthAttr());
        lightBuilder.Set("length", l.GetLengthAttr()).Set("radius", l.GetRadiusAttr());
    }
    else if (UsdLuxDiskLight l = UsdLuxDiskLight(lightPrim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(), currentTimeCode);
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxDiskLight");
        lightBuilder.Set("radius", l.GetRadiusAttr());
    }
    else if (UsdLuxDistantLight l = UsdLuxDistantLight(lightPrim))
    {
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxDistantLight");
        lightBuilder.Set("angle", l.GetAngleAttr()).Set("angleExtent", l.GetAngleAttr());
    }
    else if (UsdLuxDomeLight l = UsdLuxDomeLight(lightPrim))
    {
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxDomeLight");
        lightBuilder.Set("textureFile", l.GetTextureFileAttr())
            .Set("textureFormat", l.GetTextureFormatAttr());
    }
    else if (UsdLuxGeometryLight l = UsdLuxGeometryLight(lightPrim))
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
                    << lightPrim.GetPath() << "; using first only");
            }
            std::string kat_loc = PxrUsdKatanaUtils::ConvertUsdPathToKatLocation(geo[0], data);
            geomBuilder.set("areaLightGeometrySource", FnKat::StringAttribute(kat_loc));
        }
    }
    else if (UsdLuxRectLight l = UsdLuxRectLight(lightPrim))
    {
        geomBuilder.Set("light.width", l.GetWidthAttr()).Set("light.height", l.GetHeightAttr());
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxRectLight");
        lightBuilder.Set("lightColorMap", l.GetTextureFileAttr())
            .Set("textureFile", l.GetTextureFileAttr())
            .Set("width", l.GetWidthAttr())
            .Set("height", l.GetHeightAttr());
    }
    else if (UsdLuxSphereLight l = UsdLuxSphereLight(lightPrim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(), currentTimeCode);
        lightShaderAttr = FnAttribute::StringAttribute("UsdLuxSphereLight");
        lightBuilder.Set("radius", l.GetRadiusAttr()).Set("treatAsPoint", l.GetTreatAsPointAttr());
    }

    if (lightShaderAttr.isValid())
    {
        materialBuilder.set("usdLightShader", lightShaderAttr);
        materialBuilder.set("usdLightParams", lightBuilder.build());
    }
}

void __handleSdrRegistryLights(const UsdPrim& lightPrim,
                               const UsdTimeCode& currentTimeCode,
                               FnKat::GroupBuilder& materialBuilder,
                               PxrUsdKatanaAttrMap& geomBuilder)
{
    UsdKatanaLightAPI lightAPI(lightPrim);
    geomBuilder.Set("centerOfInterest", lightAPI.GetCenterOfInterestAttr());
    VtValue lightShaderIdsVal;
    lightAPI.GetIdAttr().Get(&lightShaderIdsVal, currentTimeCode);
    VtArray<std::string> lightShaderIds;
    if (!lightShaderIdsVal.IsEmpty() && lightShaderIdsVal.IsHolding<VtArray<std::string>>())
    {
        lightShaderIds = lightShaderIdsVal.UncheckedGet<VtArray<std::string>>();
    }

    SdrRegistry& sdrRegistry = SdrRegistry::GetInstance();

    for (const auto& id : lightShaderIds)
    {
        std::vector<std::string> idSplit = TfStringSplit(id, ":");
        if (idSplit.size() != 2)
        {
            continue;
        }

        const std::string& lightShaderPrefix = idSplit[0];
        const std::string& lightShaderId = idSplit[1];
        SdrShaderNodeConstPtr sdrNode;
        if (TfStringStartsWith(lightShaderId, "UsdLux"))
        {
            sdrNode = sdrRegistry.GetShaderNodeByIdentifier(
                TfToken(std::string(lightShaderId.begin() + 6, lightShaderId.end())));
        }
        else
        {
            sdrNode = sdrRegistry.GetShaderNodeByIdentifier(TfToken(lightShaderId));
        }
        if (!sdrNode)
        {
            continue;
        }

        PxrUsdKatanaAttrMap lightBuilder;
        lightBuilder.SetUSDTimeCode(currentTimeCode);
        const std::string shaderContext = sdrNode->GetContext().GetString();

        for (const auto& inputNameToken : sdrNode->GetInputNames())
        {
            const std::string& inputName = inputNameToken.GetString();
            const auto& rendererNameMappingIt = s_rendererToContextName.find(lightShaderPrefix);
            std::string usdAttributeName;
            if (rendererNameMappingIt != s_rendererToContextName.end())
            {
                if (shaderContext.empty())
                {
                    usdAttributeName = rendererNameMappingIt->second + ":" + inputName;
                }
                else
                {
                    usdAttributeName =
                        rendererNameMappingIt->second + ":" + shaderContext + ":" + inputName;
                }
            }
            else
            {
                if (shaderContext.empty())
                {
                    usdAttributeName = rendererNameMappingIt->second + ":" + inputName;
                }
                else
                {
                    usdAttributeName = lightShaderPrefix + ":" + shaderContext + ":" + inputName;
                }
            }
            // Try first without the inputs prefix(Pre USD 21.02,
            // light attributes did not have inputs: prefix)
            if (!lightPrim.HasAttribute(TfToken(usdAttributeName)))
            {
                // Check with inputs prefix (which was only added in 21.02 USD)
                usdAttributeName = "inputs:" + usdAttributeName;
                if (!lightPrim.HasAttribute(TfToken(usdAttributeName)))
                {
                    continue;
                }
            }
            lightBuilder.Set(inputName, lightPrim.GetAttribute(TfToken(usdAttributeName)));
        }

        materialBuilder.set(lightShaderPrefix + "LightShader",
                            FnKat::StringAttribute(lightShaderId));
        materialBuilder.set(lightShaderPrefix + "LightParams", lightBuilder.build());
    }
}

void PxrUsdKatanaReadLight(const UsdLuxLight& light,
                           const PxrUsdKatanaUsdInPrivateData& data,
                           PxrUsdKatanaAttrMap& attrs)
{
    const UsdPrim lightPrim = light.GetPrim();
    const UsdTimeCode currentTimeCode = data.GetCurrentTime();
    attrs.SetUSDTimeCode(currentTimeCode);
    PxrUsdKatanaAttrMap geomBuilder;
    geomBuilder.SetUSDTimeCode(currentTimeCode);
    FnKat::GroupBuilder materialBuilder;
    // Always set a default local value for the centerOfInterset to enable the
    // lighting tools workflow
    geomBuilder.set("centerOfInterest", FnAttribute::DoubleAttribute(20.0f));

    __handleSdrRegistryLights(lightPrim, currentTimeCode, materialBuilder, geomBuilder);
    // Run the UsdLux logic after trying the Sdr Logic
    __handleUsdLuxLightTypes(light, lightPrim, currentTimeCode, data, materialBuilder, geomBuilder);

    attrs.set("material", materialBuilder.build());
    attrs.set("geometry", geomBuilder.build());
    attrs.set("type", FnKat::StringAttribute("light"));

    // This attribute makes the light discoverable by the GafferThree node.
    FnKat::GroupBuilder gafferBuilder;
    gafferBuilder.set("packageClass", FnAttribute::StringAttribute("LightPackage"));
    attrs.set("info.gaffer", gafferBuilder.build());

    PxrUsdKatanaReadXformable(light, data, attrs);
}

PXR_NAMESPACE_CLOSE_SCOPE
