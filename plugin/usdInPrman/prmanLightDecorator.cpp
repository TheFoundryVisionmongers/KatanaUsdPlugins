// Copyright (c) 2020 The Foundry Visionmongers Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
// names, trademarks, service marks, or product names of the Licensor
// and its affiliates, except as required to comply with Section 4(c) of
// the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/getenv.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/geometryLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include "usdInPrman/declarePackageOps.h"
#include "usdKatana/attrMap.h"
#include "usdKatana/readPrim.h"

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_ENV_SETTING(USD_IMPORT_USD_LUX_LIGHTS_WITH_PRMAN_SHADERS,
                      false,
                      "If set to true basic UsdLux prims will import with RenderMan light shader"
                      "information as well. Off by default. RfK must also be setup in the"
                      "environment.");

USDKATANA_USDIN_PLUGIN_DEFINE(UsdInPrmanLuxLight_LocationDecorator, privateData, opArgs, interface)
{
    static const bool importUsdLuxAsPrman =
        TfGetEnvSetting(USD_IMPORT_USD_LUX_LIGHTS_WITH_PRMAN_SHADERS);
    if (!importUsdLuxAsPrman)
    {
        return;
    }
    if (!privateData.hasOutputTarget("prman"))
    {
        return;
    }
    // Skip writing if a prman light shader exists.  This is because
    // its probably been read in via the SdrRegistry importing logic.
    if (interface.getOutputAttr("material.prmanLightShader").isValid())
    {
        return;
    }
    std::string locationType =
        FnKat::StringAttribute(interface.getOutputAttr("type")).getValue("", false);

    if (locationType == "light")
    {
        const UsdPrim lightPrim = privateData.GetUsdPrim();
        if (!lightPrim)
        {
            return;
        }
        const UsdLuxBoundableLightBase light = UsdLuxBoundableLightBase(lightPrim);
        if (!light)
        {
            return;
        }
        const UsdTimeCode currentTimeCode = privateData.GetCurrentTime();
        UsdKatanaAttrMap lightBuilder;
        lightBuilder.SetUSDTimeCode(currentTimeCode);
        FnKat::GroupBuilder materialBuilder;

        lightBuilder.Set("intensity", light.GetIntensityAttr())
            .Set("exposure", light.GetExposureAttr())
            .Set("diffuse", light.GetDiffuseAttr())
            .Set("specular", light.GetSpecularAttr())
            .Set("color", light.GetColorAttr())
            .Set("enableTemperature", light.GetEnableColorTemperatureAttr())
            .Set("temperature", light.GetColorTemperatureAttr())
            .Set("areaNormalize", light.GetNormalizeAttr())
            .Set("lightColor", light.GetColorAttr());

        UsdLuxShapingAPI shapingAPI(lightPrim);
        lightBuilder.Set("emissionFocus", shapingAPI.GetShapingFocusAttr())
            .Set("emissionFocusTint", shapingAPI.GetShapingFocusTintAttr())
            .Set("coneAngle", shapingAPI.GetShapingConeAngleAttr())
            .Set("coneSoftness", shapingAPI.GetShapingConeSoftnessAttr())
            .Set("iesProfile", shapingAPI.GetShapingIesFileAttr())
            .Set("iesProfileScale", shapingAPI.GetShapingIesAngleScaleAttr())
            .Set("iesProfileNormalize", shapingAPI.GetShapingIesNormalizeAttr());

        UsdLuxShadowAPI shadowAPI(lightPrim);
        lightBuilder.Set("enableShadows", shadowAPI.GetShadowEnableAttr())
            .Set("shadowColor", shadowAPI.GetShadowColorAttr())
            .Set("shadowDistance", shadowAPI.GetShadowDistanceAttr())
            .Set("shadowFalloff", shadowAPI.GetShadowFalloffAttr())
            .Set("shadowFalloffGamma", shadowAPI.GetShadowFalloffGammaAttr());

        FnKat::StringAttribute lightShader;
        if (UsdLuxSphereLight l = UsdLuxSphereLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrSphereLight");
            lightBuilder.Set("radius", l.GetRadiusAttr())
                .Set("treatAsPoint", l.GetTreatAsPointAttr());
        }

        if (UsdLuxDiskLight l = UsdLuxDiskLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrDiskLight");
            lightBuilder.Set("radius", l.GetRadiusAttr());
        }
        if (UsdLuxCylinderLight l = UsdLuxCylinderLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrCylinderLight");
            lightBuilder.Set("length", l.GetLengthAttr())
                .Set("radius", l.GetRadiusAttr())
                .Set("treatAsLine", l.GetTreatAsLineAttr());
        }
        if (UsdLuxRectLight l = UsdLuxRectLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrRectLight");
            lightBuilder.Set("colorMapGamma", lightPrim.GetAttribute(TfToken("ri:texture:gamma")))
                .Set("colorMapSaturation", lightPrim.GetAttribute(TfToken("ri:texture:saturation")))
                .Set("lightColorMap", l.GetTextureFileAttr())
                .Set("width", l.GetWidthAttr())
                .Set("height", l.GetHeightAttr());
        }

        if (UsdLuxDistantLight l = UsdLuxDistantLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrDistantLight");
            lightBuilder.Set("angle", l.GetAngleAttr())
                .Set("angleExtent", l.GetAngleAttr())
                .Set("intensity", l.GetIntensityAttr());
        }

        if (UsdLuxGeometryLight l = UsdLuxGeometryLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrMeshLight");
        }

        if (UsdLuxDomeLight l = UsdLuxDomeLight(lightPrim))
        {
            lightShader = FnKat::StringAttribute("PxrDomeLight");
            lightBuilder.Set("lightColorMap", l.GetTextureFileAttr())
                .Set("colorMapGamma", lightPrim.GetAttribute(TfToken("ri:texture:gamma")))
                .Set("colorMapSaturation",
                     lightPrim.GetAttribute(TfToken("ri:texture:saturation")));
        }

        FnKat::GroupBuilder primStatements;
        UsdKatanaReadPrimPrmanStatements(lightPrim, currentTimeCode.GetValue(), primStatements,
                                         true);
        // Gather prman statements
        interface.setAttr("prmanStatements", primStatements.build());
        // materialBuilder.set("prmanLightParams", lightBuilder.build());
        interface.setAttr("material.prmanLightParams", lightBuilder.build());
        interface.setAttr("material.prmanLightShader", lightShader);
    }
}
