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
#include "usdKatana/attrMap.h"

#include <pxr/pxr.h>

#include "usdKatana/utils.h"

PXR_NAMESPACE_OPEN_SCOPE

void UsdKatanaAttrMap::set(const std::string& path, const Foundry::Katana::Attribute& attr)
{
    // on mutation, seed the groupBuilder with the lastBuild value and clear
    if (_lastBuilt.isValid())
    {
        _groupBuilder.update(_lastBuilt);
        _lastBuilt = Foundry::Katana::GroupAttribute();
    }
    
    _groupBuilder.set(path, attr);
}

UsdKatanaAttrMap& UsdKatanaAttrMap::Set(const std::string& path, const UsdAttribute& attr)
{
    VtValue val;
    if (attr.HasAuthoredValue() && attr.Get(&val, _usdTimeCode)) {
        FnKat::Attribute kat_attr = UsdKatanaUtils::ConvertVtValueToKatAttr(val);
        _groupBuilder.set(path, kat_attr);
    }
    return *this;
}

void UsdKatanaAttrMap::del(const std::string& path)
{
    // on mutation, seed the groupBuilder with the lastBuild value and clear
    if (_lastBuilt.isValid())
    {
        _groupBuilder.update(_lastBuilt);
        _lastBuilt = Foundry::Katana::GroupAttribute();
    }
    
    _groupBuilder.del(path);
}

FnAttribute::GroupAttribute UsdKatanaAttrMap::build()
{
    if (_lastBuilt.isValid())
    {
        return _lastBuilt;
    }
    
    _lastBuilt = _groupBuilder.build();
    return _lastBuilt;
}

void UsdKatanaAttrMap::toInterface(FnKat::GeolibCookInterface& interface)
{
    FnAttribute::GroupAttribute groupAttr = build();
    size_t numChildren = groupAttr.getNumberOfChildren();
    for (size_t i = 0; i < numChildren; i++)
    {
        const std::string childName = groupAttr.getChildName(i);
        const FnKat::Attribute childAttr = groupAttr.getChildByIndex(i);

        if (childAttr.getType() == kFnKatAttributeTypeGroup)
        {
            FnAttribute::GroupAttribute existingAttr = interface.getOutputAttr(childName);
            if (existingAttr.isValid())
            {
                // If we are setting a group attribute and an existing
                // group attribute exists, merge them.
                interface.setAttr(childName,
                    FnAttribute::GroupBuilder()
                        .update(existingAttr)
                        .deepUpdate(childAttr)
                        .build());
            }
            else
            {
                interface.setAttr(childName, childAttr);
            }
        }
        else
        {
            interface.setAttr(childName, childAttr);
        }
    }
}

bool UsdKatanaAttrMap::isBuilt()
{
    return _lastBuilt.isValid();
}

PXR_NAMESPACE_CLOSE_SCOPE

