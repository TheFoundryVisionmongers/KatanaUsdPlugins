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

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/pointInstancer.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readPointInstancer.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
// Shift the given StaticSceneCreateOpArgs to the given destination.
//
// For example, the following StaticSceneCreateOpArgs and /root/world/geo are specified:
//
// sscOpArgs
// └── "c"
//     └── "world"
//         └── "c"
//             └── "geo"
//                 └── "c"
//                     └── "scene"
//                         └── ...
//
// The result set to sscOpArgs is:
//
// sscOpArgs ()
// └── "c"
//     └── "scene"
//         └── ...
void shiftStaticSceneCreateOpArgs(FnKat::GroupAttribute* sscOpArgs, std::string_view dest)
{
    if (dest.substr(0, 6) != "/root/")
    {
        return;
    }
    size_t cur = 6;
    bool foundElement = true;

    do
    {
        size_t next = dest.find('/', cur);
        if (next == cur)
        {
            break;
        }
        if (next == std::string::npos)
        {
            next = dest.size();
        }
        const std::string_view seg = dest.substr(cur, next - cur);
        cur = next + 1;

        foundElement = false;
        const FnKat::GroupAttribute childAttrs = sscOpArgs->getChildByName("c");
        for (int i = 0; i < childAttrs.getNumberOfChildren(); ++i)
        {
            if (childAttrs.getChildName(i) == seg)
            {
                *sscOpArgs = childAttrs.getChildByIndex(i);
                foundElement = true;
                break;
            }
        }
    } while (foundElement && cur < dest.size());
}
}  // namespace

USDKATANA_USDIN_PLUGIN_DEFINE(UsdInCore_PointInstancerOp, privateData, opArgs, interface)
{
    UsdGeomPointInstancer instancer =
        UsdGeomPointInstancer(privateData.GetUsdPrim());

    // Generate input attr map for consumption by the reader.
    //
    UsdKatanaAttrMap inputAttrMap;

    // Get the instancer's Katana location.
    //
    inputAttrMap.set("outputLocationPath",
            FnKat::StringAttribute(interface.getOutputLocationPath()));

    // Pass along UsdIn op args.
    //
    inputAttrMap.set("opArgs", opArgs);

    // Generate output attr maps.
    //
    // Instancer attr map: describes the instancer itself
    // Sources attr map: describes the instancer's "instance source" children.
    // Instances attr map: describes the instancer's "instance array" child.
    //
    UsdKatanaAttrMap instancerAttrMap;
    UsdKatanaAttrMap sourcesAttrMap;
    UsdKatanaAttrMap instancesAttrMap;
    UsdKatanaReadPointInstancer(instancer, privateData, instancerAttrMap, sourcesAttrMap,
                                instancesAttrMap, inputAttrMap);

    // Send instancer attrs directly to the interface.
    //
    instancerAttrMap.toInterface(interface);

    // Tell UsdIn to skip all children; we'll create them ourselves below.
    //
    interface.setAttr("__UsdIn.skipAllChildren", FnKat::IntAttribute(1));

    // Early exit if any errors were encountered.
    //
    if (FnKat::StringAttribute(interface.getOutputAttr("errorMessage"))
            .isValid() or
        FnKat::StringAttribute(interface.getOutputAttr("warningMessage"))
            .isValid()) {
        return;
    }

    // Build the other output attr maps.
    //
    FnKat::GroupAttribute sourcesSSCAttrs = sourcesAttrMap.build();
    FnKat::GroupAttribute instancesSSCAttrs = instancesAttrMap.build();
    if (not sourcesSSCAttrs.isValid() or not instancesSSCAttrs.isValid())
    {
        return;
    }

    // Create "instance source" children using BuildIntermediate.
    //
    UsdKatanaUsdInArgsRefPtr usdInArgs = privateData.GetUsdInArgs();

    // Prune the upper part of sourcesSSCAttrs from outputLocation.
    shiftStaticSceneCreateOpArgs(&sourcesSSCAttrs, interface.getOutputLocationPath());

    interface.execOp(
        "UsdIn.BuildIntermediate",
        FnKat::GroupBuilder().update(opArgs).set("staticScene", sourcesSSCAttrs).build(),
        new UsdKatanaUsdInPrivateData(usdInArgs->GetRootPrim(), usdInArgs, &privateData),
        UsdKatanaUsdInPrivateData::Delete);

    // Create "instance array" child using StaticSceneCreate.
    //
    interface.execOp("StaticSceneCreate", instancesSSCAttrs);
}
