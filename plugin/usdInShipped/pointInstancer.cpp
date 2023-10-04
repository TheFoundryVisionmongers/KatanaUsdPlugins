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
    FnKat::GroupAttribute childAttrs = sourcesSSCAttrs.getChildByName("c");
    for (int64_t i = 0; i < childAttrs.getNumberOfChildren(); ++i)
    {
        FnKat::GroupAttribute sourceAttrs = childAttrs.getChildByIndex(i);
        // If this source contains ops (group under x) then we need to feed it 
        // through the UsdIn op for it to be created properly.
        if (sourceAttrs.getChildByName("x").isValid())
        {
            FnKat::GroupAttribute attrsGroup = sourceAttrs.getChildByName("a");
            FnKat::StringAttribute primPathAttr =
                attrsGroup.getChildByName("usdPrimPath");
            if (primPathAttr.isValid())
            {
                auto usdPrimPathValues = primPathAttr.getNearestSample(0);
                
                for (size_t i = 0; i < usdPrimPathValues.size(); ++i)
                {
                    std::string primPath(usdPrimPathValues[i]);
                    if (!SdfPath::IsValidPathString(primPath))
                    {
                        continue;
                    }
                    
                    // Get the usd prim at the given source path.
                    UsdPrim prim = usdInArgs->GetStage()->GetPrimAtPath(
                            SdfPath(primPath));
                    std::string nameToUse = prim.GetName();
                    
                    ArgsBuilder ab;
                    ab.update(usdInArgs);
                    ab.rootLocation =
                        interface.getOutputLocationPath() + "/" + nameToUse;
                    ab.isolatePath = primPath;

                    interface.createChild(
                        nameToUse, "UsdIn",
                        FnKat::GroupBuilder()
                            .update(interface.getOpArg())
                            .set("childOfIntermediate", FnKat::IntAttribute(1))
                            .set("staticScene", sourceAttrs)
                            .build(),
                        FnKat::GeolibCookInterface::ResetRootFalse,
                        new UsdKatanaUsdInPrivateData(prim, ab.build(), &privateData),
                        UsdKatanaUsdInPrivateData::Delete);
                }
            }
        }
        else
        {
            interface.createChild(
                childAttrs.getChildName(i), "UsdIn.BuildIntermediate",
                FnKat::GroupBuilder().update(opArgs).set("staticScene", sourceAttrs).build(),
                FnKat::GeolibCookInterface::ResetRootFalse,
                new UsdKatanaUsdInPrivateData(usdInArgs->GetRootPrim(), usdInArgs, &privateData),
                UsdKatanaUsdInPrivateData::Delete);
        }
    }

    // Create "instance array" child using StaticSceneCreate.
    //
    interface.execOp("StaticSceneCreate", instancesSSCAttrs);
}
