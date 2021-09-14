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
#include "pxr/pxr.h"
#include "usdKatana/attrMap.h"
#include "usdKatana/readLight.h"
#include "usdKatana/readPrim.h"
#include "usdKatana/readXformable.h"
#include "usdKatana/utils.h"

#include "pxr/base/tf/stringUtils.h"

#include "pxr/usd/usdLux/light.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/geometryLight.h"
#include "pxr/usd/usdLux/sphereLight.h"
#include "pxr/usd/usdLux/cylinderLight.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/shapingAPI.h"
#include "pxr/usd/usdLux/shadowAPI.h"
#include "pxr/usd/usdRi/lightAPI.h"
#include "pxr/usd/usdRi/textureAPI.h"
#include "pxr/usd/usdRi/pxrEnvDayLight.h"
#include "pxr/usd/usdRi/pxrAovLight.h"

#include <FnGeolibServices/FnAttributeFunctionUtil.h>
#include <FnLogging/FnLogging.h>
#include <pystring/pystring.h>

#include <stack>

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("PxrUsdKatanaReadLight");

// Convert USD radius to light.size (which acts like diameter)
static void
_SetLightSizeFromRadius(PxrUsdKatanaAttrMap &geomBuilder,
                        UsdAttribute radiusAttr,
                        UsdTimeCode time)
{
    VtValue radiusVal;
    radiusAttr.Get(&radiusVal, time);
    float radius = radiusVal.Get<float>();
    geomBuilder.set("light.size", FnKat::FloatAttribute(2.0f*radius));
}

void
PxrUsdKatanaReadLight(
        const UsdLuxLight& light,
        const PxrUsdKatanaUsdInPrivateData& data,
        PxrUsdKatanaAttrMap& attrs)
{
    const UsdPrim lightPrim = light.GetPrim();
    const UsdTimeCode currentTimeCode = data.GetCurrentTime();
    const bool prmanOutputTarget = data.hasOutputTarget("prman");
    attrs.SetUSDTimeCode(currentTimeCode);
    PxrUsdKatanaAttrMap lightBuilder;
    lightBuilder.SetUSDTimeCode(currentTimeCode);
    PxrUsdKatanaAttrMap geomBuilder;
    geomBuilder.SetUSDTimeCode(currentTimeCode);
    FnKat::GroupBuilder materialBuilder;

    // UsdLuxLight
    lightBuilder
        .Set("intensity", light.GetIntensityAttr())
        .Set("exposure", light.GetExposureAttr())
        .Set("diffuse", light.GetDiffuseAttr())
        .Set("specular", light.GetSpecularAttr())
        .Set("normalize", light.GetNormalizeAttr())
        .Set("color", light.GetColorAttr())
        .Set("enableTemperature", light.GetEnableColorTemperatureAttr())
        .Set("temperature", light.GetColorTemperatureAttr())
        .Set("color", light.GetColorAttr());

        // Prman still has different names for these attributes, so we need
        // to write them out like this, until we move to a more SdrRegistry
        // focused import.
        if(prmanOutputTarget)
        {
        lightBuilder
            .Set("areaNormalize", light.GetNormalizeAttr())
            .Set("lightColor", light.GetColorAttr())
            ;
        }

    if (lightPrim)
    {
        UsdLuxShapingAPI shapingAPI(lightPrim);
        // Prman still has different names for these attributes, so we need
        // to write them out like this, until we move to a more SdrRegistry
        // focused import.
        if(prmanOutputTarget)
        {
            lightBuilder
                .Set("emissionFocus", shapingAPI.GetShapingFocusAttr())
                .Set("emissionFocusTint", shapingAPI.GetShapingFocusTintAttr())
                .Set("coneAngle", shapingAPI.GetShapingConeAngleAttr())
                .Set("coneSoftness", shapingAPI.GetShapingConeSoftnessAttr())
                .Set("iesProfile", shapingAPI.GetShapingIesFileAttr())
                .Set("iesProfileScale", shapingAPI.GetShapingIesAngleScaleAttr())
                .Set("iesProfileNormalize", shapingAPI.GetShapingIesNormalizeAttr())
                ;

            UsdLuxShadowAPI shadowAPI(lightPrim);
            lightBuilder.Set("enableShadows", shadowAPI.GetShadowEnableAttr());
        }

        lightBuilder
            .Set("shapingFocus", shapingAPI.GetShapingFocusAttr())
            .Set("shapingFocusTint", shapingAPI.GetShapingFocusTintAttr())
            .Set("shapingConeAngle", shapingAPI.GetShapingConeAngleAttr())
            .Set("shapingConeSoftness", shapingAPI.GetShapingConeSoftnessAttr())
            .Set("shapingIesFile", shapingAPI.GetShapingIesFileAttr())
            .Set("shapingIesAngleScale", shapingAPI.GetShapingIesAngleScaleAttr())
            .Set("shapingIesNormalize", shapingAPI.GetShapingIesNormalizeAttr())
            ;

        UsdLuxShadowAPI shadowAPI(lightPrim);
        lightBuilder
            .Set("shadowEnable", shadowAPI.GetShadowEnableAttr())
            .Set("shadowColor", shadowAPI.GetShadowColorAttr())
            .Set("shadowDistance", shadowAPI.GetShadowDistanceAttr())
            .Set("shadowFalloff", shadowAPI.GetShadowFalloffAttr())
            .Set("shadowFalloffGamma", shadowAPI.GetShadowFalloffGammaAttr());

        UsdRiLightAPI riLightAPI(lightPrim);
        lightBuilder
            .Set("intensityNearDist", riLightAPI.GetRiIntensityNearDistAttr())
            .Set("traceLightPaths", riLightAPI.GetRiTraceLightPathsAttr())
            .Set("thinShadow", riLightAPI.GetRiShadowThinShadowAttr())
            .Set("fixedSampleCount",
                 riLightAPI.GetRiSamplingFixedSampleCountAttr())
            .Set("importanceMultiplier",
                 riLightAPI.GetRiSamplingImportanceMultiplierAttr())
            .Set("lightGroup", riLightAPI.GetRiLightGroupAttr());
    }

    if (UsdLuxSphereLight l = UsdLuxSphereLight(lightPrim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(),
                                currentTimeCode);
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                                FnKat::StringAttribute("PxrSphereLight"));
        }

        materialBuilder.set("usdLightShader",
                        FnKat::StringAttribute("UsdLuxSphereLight"));

        lightBuilder.Set("radius", l.GetRadiusAttr());
        lightBuilder.Set("treatAsPoint", l.GetTreatAsPointAttr());
    }

    if (UsdLuxDiskLight l = UsdLuxDiskLight(lightPrim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(),
                                currentTimeCode);
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrDiskLight"));
        }

        materialBuilder.set("usdLightShader",
                            FnKat::StringAttribute("UsdLuxDiskLight"));

        lightBuilder.Set("radius", l.GetRadiusAttr());
    }

    if (UsdLuxCylinderLight l = UsdLuxCylinderLight(lightPrim))
    {
        _SetLightSizeFromRadius(geomBuilder, l.GetRadiusAttr(),
                                currentTimeCode);
        geomBuilder.Set("light.width", l.GetLengthAttr());
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrCylinderLight"));
        }

        materialBuilder.set("usdLightShader",
                        FnKat::StringAttribute("UsdLuxCylinderLight"));

        lightBuilder.Set("length", l.GetLengthAttr());
        lightBuilder.Set("radius", l.GetRadiusAttr());
        lightBuilder.Set("treatAsLine", l.GetTreatAsLineAttr());
    }

    if (UsdLuxRectLight l = UsdLuxRectLight(lightPrim))
    {
        geomBuilder.Set("light.width", l.GetWidthAttr());
        geomBuilder.Set("light.height", l.GetHeightAttr());
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrRectLight"));
        }

        materialBuilder.set("usdLightShader",
                        FnKat::StringAttribute("UsdLuxRectLight"));

        // Non-standard lightColorMap preserved for backwards compatibility.
        UsdRiTextureAPI textureAPI(lightPrim);
        lightBuilder
            .Set("colorMapGamma", textureAPI.GetRiTextureGammaAttr())
            .Set("colorMapSaturation", textureAPI.GetRiTextureSaturationAttr())
            .Set("lightColorMap", l.GetTextureFileAttr())
            .Set("textureFile", l.GetTextureFileAttr())
            .Set("width", l.GetWidthAttr())
            .Set("height", l.GetHeightAttr());
    }

    if (UsdLuxDistantLight l = UsdLuxDistantLight(lightPrim))
    {
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrDistantLight"));
        }

        materialBuilder.set("usdLightShader",
                        FnKat::StringAttribute("UsdLuxDistantLight"));

        // Non-standard angleExtend preserved for backwards compatibility.
        lightBuilder
            .Set("angle", l.GetAngleAttr())
            .Set("angleExtent", l.GetAngleAttr())
            .Set("intensity", l.GetIntensityAttr());
    }

    if (UsdLuxGeometryLight l = UsdLuxGeometryLight(lightPrim))
    {
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrMeshLight"));
        }
        materialBuilder.set("usdLightShader",
                        FnKat::StringAttribute("UsdLuxMeshLight"));
        SdfPathVector geo;
        if (l.GetGeometryRel().GetForwardedTargets(&geo) && !geo.empty()) {
            if (geo.size() > 1) {
                FnLogWarn("Multiple geometry targest detected for "
                          "USD geometry light " << lightPrim.GetPath()
                          << "; using first only");
            }
            std::string kat_loc =
                PxrUsdKatanaUtils::ConvertUsdPathToKatLocation(geo[0], data);
            geomBuilder.set("areaLightGeometrySource",
                      FnKat::StringAttribute(kat_loc));
        }
    }

    if (UsdLuxDomeLight l = UsdLuxDomeLight(lightPrim))
    {
        if (prmanOutputTarget)
        {
            materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrDomeLight"));
        }
        materialBuilder.set("usdLightShader",
                        FnKat::StringAttribute("UsdLuxDomeLight"));
        lightBuilder.Set("lightColorMap", l.GetTextureFileAttr());
        // Note: The prman backend ignores texture:format since that is
        // specified inside the renderman texture file format.
        UsdRiTextureAPI textureAPI(lightPrim);
        lightBuilder
            .Set("colorMapGamma", textureAPI.GetRiTextureGammaAttr())
            .Set("colorMapSaturation", textureAPI.GetRiTextureSaturationAttr())
            .Set("textureFile", l.GetTextureFileAttr())
            .Set("textureFormat", l.GetTextureFormatAttr());
    }

    // Prman specific light.
    if (UsdRiPxrEnvDayLight l = UsdRiPxrEnvDayLight(lightPrim))
    {
        materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrEnvDayLight"));
        lightBuilder
            .Set("day", l.GetDayAttr())
            .Set("haziness", l.GetHazinessAttr())
            .Set("hour", l.GetHourAttr())
            .Set("latitude", l.GetLatitudeAttr())
            .Set("longitude", l.GetLongitudeAttr())
            .Set("month", l.GetMonthAttr())
            .Set("skyTint", l.GetSkyTintAttr())
            .Set("sunDirection", l.GetSunDirectionAttr())
            .Set("sunSize", l.GetSunSizeAttr())
            .Set("sunTint", l.GetSunTintAttr())
            .Set("year", l.GetYearAttr())
            .Set("zone", l.GetZoneAttr());
    }

    // Prman specific light.
    if (UsdRiPxrAovLight l = UsdRiPxrAovLight(lightPrim))
    {
        materialBuilder.set("prmanLightShader",
                            FnKat::StringAttribute("PxrAovLight"));
        lightBuilder
            .Set("aovName", l.GetAovNameAttr())
            .Set("inPrimaryHit", l.GetInPrimaryHitAttr())
            .Set("inReflection", l.GetInReflectionAttr())
            .Set("inRefraction", l.GetInRefractionAttr())
            .Set("invert", l.GetInvertAttr())
            .Set("onVolumeBoundaries", l.GetOnVolumeBoundariesAttr())
            .Set("useColor", l.GetUseColorAttr())
            .Set("useThroughput", l.GetUseThroughputAttr());
        // XXX aovSuffix, writeToDisk
    }

    // TODO: Pxr visibleInRefractionPath, cheapCaustics, maxDistance
    // surfSat, MsApprox

    // TODO portals
    // TODO UsdRi extensions

    FnKat::GroupBuilder primStatements;
    PxrUsdKatanaReadPrimPrmanStatements(lightPrim, currentTimeCode.GetValue(),
        primStatements, prmanOutputTarget);
    if (prmanOutputTarget)
    {
        // Gather prman statements
        attrs.set("prmanStatements", primStatements.build());
        materialBuilder.set("prmanLightParams", lightBuilder.build());
    }
    

    materialBuilder.set("usdLightParams", lightBuilder.build());
    attrs.set("material", materialBuilder.build());

    attrs.set("geometry", geomBuilder.build());
    attrs.set("type", FnKat::StringAttribute("light"));
    attrs.set("usd", primStatements.build());

    // This attribute makes the light discoverable by the GafferThree node.
    FnKat::GroupBuilder gafferBuilder;
    gafferBuilder.set("packageClass", FnAttribute::StringAttribute("LightPackage"));
    attrs.set("info.gaffer", gafferBuilder.build());

    PxrUsdKatanaReadXformable(light, data, attrs);
}

PXR_NAMESPACE_CLOSE_SCOPE
