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

#include <FnGeolibServices/FnBuiltInOpArgsUtil.h>

#include <pxr/pxr.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/lightAPI.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readLight.h"
#include "usdKatana/usdInPluginRegistry.h"
#include "usdKatana/utils.h"

PXR_NAMESPACE_USING_DIRECTIVE

USDKATANA_USDIN_PLUGIN_DEFINE(UsdInCore_LightOp, privateData, opArgs, interface)
{
    UsdKatanaUsdInArgsRefPtr usdInArgs = privateData.GetUsdInArgs();
    UsdKatanaAttrMap attrs;

    UsdPrim prim(privateData.GetUsdPrim());

    UsdKatanaReadLight(prim, privateData, attrs);

    attrs.toInterface(interface);


    // Tell UsdIn to skip all children; we'll create them ourselves.
    interface.setAttr("__UsdIn.skipAllChildren", FnKat::IntAttribute(1));

    UsdLuxLightAPI light(prim);

    // Light filters.
    SdfPathVector filterPaths;
    light.GetFiltersRel().GetForwardedTargets(&filterPaths);
    if (!filterPaths.empty()) {
        // XXX For now the importAsReferences codepath is disabled.
        // To support light filter references we need to specify
        // info.gaffer.packageClass (and possibly more), otherwise
        // the gaffer infrastructure will mark these references
        // as orphaned.
        bool importAsReferences = false;
        if (importAsReferences) {
            // Create "light filter reference" child locations
            // TODO: We also need to handle the case of regular
            // light filters sitting as children below this light.
            FnGeolibServices::StaticSceneCreateOpArgsBuilder sscb(false);
            for (const SdfPath &filterPath: filterPaths) {
                const std::string ref_location = filterPath.GetName();
                const std::string filter_location =
                    UsdKatanaUtils::ConvertUsdPathToKatLocation(filterPath, usdInArgs);
                sscb.createEmptyLocation(ref_location,
                    "light filter reference");
                sscb.setAttrAtLocation(ref_location,
                    "info.gaffer.referencePath",
                    FnAttribute::StringAttribute(filter_location));
            }
            interface.execOp("StaticSceneCreate", sscb.build());
        } else {
            // Expand light filters directly beneath this light.
            for (const SdfPath& filterPath : filterPaths)
            {
                if (UsdPrim filterPrim = usdInArgs->GetStage()->GetPrimAtPath(filterPath))
                {
                    const std::string filterKatPath =
                        UsdKatanaUtils::ConvertUsdPathToKatLocation(filterPath, privateData);
                    interface.createChild(
                        filterPath.GetName(),
                        "UsdInCore_LightFilterOp",
                        opArgs,
                        FnKat::GeolibCookInterface::ResetRootFalse,
                        new UsdKatanaUsdInPrivateData(filterPrim, usdInArgs, &privateData),
                        UsdKatanaUsdInPrivateData::Delete);
                }
            }
        }
    }
}

namespace {
static void lightListFnc(UsdKatanaUtilsLightListAccess& lightList)
{
    UsdPrim prim = lightList.GetPrim();
    if (prim && (prim.HasAPI<UsdLuxLightAPI>() || prim.GetTypeName() == "Light")) {
        UsdLuxLightAPI light(prim);
        lightList.Set("path", lightList.GetLocation());
        lightList.SetLinks(light.GetLightLinkCollectionAPI(), "enable");
        lightList.Set("enable", true);
        lightList.SetLinks(light.GetShadowLinkCollectionAPI(), "geoShadowEnable");
    }

    TfType pxrAovLight = TfType::FindByName("UsdRiPxrAovLight");
    if (prim && !pxrAovLight.IsUnknown() && prim.IsA(pxrAovLight))
    {
        lightList.Set("hasAOV", true);
    }
}

}

void registerUsdInShippedLightLightListFnc()
{
    UsdKatanaUsdInPluginRegistry::RegisterLightListFnc(lightListFnc);
}
