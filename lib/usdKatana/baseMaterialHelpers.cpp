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
#include "usdKatana/baseMaterialHelpers.h"

#include <pxr/pxr.h>

#include <pxr/usd/pcp/layerStack.h>
#include <pxr/usd/pcp/node.h>
#include <pxr/usd/pcp/primIndex.h>
#include <pxr/usd/sdf/relationshipSpec.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usdShade/material.h>

PXR_NAMESPACE_OPEN_SCOPE


static bool
_NodeRepresentsDirectReference(const PcpNodeRef &node)
{
    for (PcpNodeRef n = node; 
            n; // 0, or false, means we are at the root node
            n = n.GetParentNode()) {
        if (n.GetArcType() == PcpArcTypeReference) {
            if (! n.IsDueToAncestor()) {
                // direct reference!
                return true;
            }
        }
    }
    return false;
}


// This tests if a given node represents a "live" base material,
// i.e. once that hasn't been "flattened out" due to being
// pulled across a reference to a library.
static bool
_NodeRepresentsLiveBaseMaterial(const PcpNodeRef &node)
{
    bool isLiveBaseMaterial = false;
    for (PcpNodeRef n = node; 
            n; // 0, or false, means we are at the root node
            n = n.GetOriginNode()) {
        switch(n.GetArcType()) {
        case PcpArcTypeSpecialize:
            isLiveBaseMaterial = true;
            break;
        // dakrunch: specializes across references are actually still valid.
        // 
        // case PcpArcTypeReference:
        //     if (isLiveBaseMaterial) {
        //         // Node is within a base material, but that is in turn
        //         // across a reference. That means this is a library
        //         // material, so it is not live and we should flatten it
        //         // out.  Continue iterating, however, since this
        //         // might be referenced into some other live base material
        //         // downstream.
        //         isLiveBaseMaterial = false;
        //     }
        //     break;
        default:
            break;
        }
    }
    return isLiveBaseMaterial;
}

bool UsdKatana_IsAttrValFromDirectReference(const UsdAttribute& attr)
{
    return _NodeRepresentsDirectReference( attr.GetResolveInfo().GetNode() );
}

bool UsdKatana_IsAttrValFromBaseMaterial(const UsdAttribute& attr)
{
    return _NodeRepresentsLiveBaseMaterial( attr.GetResolveInfo().GetNode() );
}

static UsdPrim _GetParentMaterialPrim(const UsdPrim& prim)
{
    for (UsdPrim p{prim}; p.IsValid() && !p.IsPseudoRoot(); p = prim.GetParent())
    {
        if (p.IsA<UsdShadeMaterial>())
            return p;
    }
    return {};
}

bool UsdKatana_IsAttrValFromSiblingBaseMaterial(const UsdAttribute& attr)
{
    const PcpNodeRef sourceNode = attr.GetResolveInfo().GetNode();
    if (_NodeRepresentsLiveBaseMaterial(sourceNode))
    {
        // Get the material prims for both the material containing the attr and the source material
        // from the specialization arc. Ensure they are siblings of each other.
        const UsdPrim sourceMaterialPrim =
            _GetParentMaterialPrim(attr.GetPrim().GetStage()->GetPrimAtPath(sourceNode.GetPath()));
        const UsdPrim materialPrim = _GetParentMaterialPrim(attr.GetPrim());
        if (sourceMaterialPrim.IsA<UsdShadeMaterial>() && materialPrim.IsA<UsdShadeMaterial>())
        {
            if (sourceMaterialPrim.GetPath().GetParentPath() ==
                materialPrim.GetPath().GetParentPath())
            {
                return true;
            }
        }
    }

    return false;
}

bool UsdKatana_IsPrimDefFromBaseMaterial(const UsdPrim& prim)
{
    for(const PcpNodeRef &n: prim.GetPrimIndex().GetNodeRange()) {
        for (const SdfLayerRefPtr &l: n.GetLayerStack()->GetLayers()) {
            if (SdfPrimSpecHandle p = l->GetPrimAtPath(n.GetPath())) {
                if (SdfIsDefiningSpecifier(p->GetSpecifier())) {
                    return _NodeRepresentsLiveBaseMaterial(n);
                }
            }
        }
    }
    return false;
}

bool UsdKatana_IsPrimDefFromSiblingBaseMaterial(const UsdPrim& prim)
{
    const SdfPath primParentPath = prim.GetPath().GetParentPath();
    for (const PcpNodeRef& n : prim.GetPrimIndex().GetNodeRange())
    {
        for (const SdfLayerRefPtr& l : n.GetLayerStack()->GetLayers())
        {
            if (SdfPrimSpecHandle p = l->GetPrimAtPath(n.GetPath()))
            {
                if (SdfIsDefiningSpecifier(p->GetSpecifier()) &&
                    primParentPath == p->GetPath().GetParentPath())
                {
                    return _NodeRepresentsLiveBaseMaterial(n);
                }
            }
        }
    }
    return false;
}

bool UsdKatana_AreRelTargetsFromBaseMaterial(const UsdRelationship& rel)
{
    // Find the strongest opinion about the relationship targets.
    SdfRelationshipSpecHandle strongestRelSpec;
    SdfPropertySpecHandleVector propStack = rel.GetPropertyStack();
    for (const SdfPropertySpecHandle &prop: propStack) {
        if (SdfRelationshipSpecHandle relSpec =
            TfDynamic_cast<SdfRelationshipSpecHandle>(prop)) {
            if (relSpec->HasTargetPathList()) {
                strongestRelSpec = relSpec;
                break;
            }
        }
    }
    // Find which prim node introduced that opinion.
    if (strongestRelSpec) {
        for(const PcpNodeRef &node:
            rel.GetPrim().GetPrimIndex().GetNodeRange()) {
            if (node.GetPath() == strongestRelSpec->GetPath().GetPrimPath() &&
                node.GetLayerStack()->HasLayer(strongestRelSpec->GetLayer())) {
                return _NodeRepresentsLiveBaseMaterial(node);
            }
        }
    }
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE

