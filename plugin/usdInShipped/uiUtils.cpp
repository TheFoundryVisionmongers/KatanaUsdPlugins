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
#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnGroupBuilder.h>
#include <FnPluginSystem/FnPlugin.h>
#include <FnDefaultAttributeProducer/plugin/FnDefaultAttributeProducerPlugin.h>
#include <FnDefaultAttributeProducer/plugin/FnDefaultAttributeProducerUtil.h>

#include "pxr/pxr.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
// Allows for attr hints to be described via attrs. This is used by
// UsdInVariantSelect to populate its pop-up menus with contextually
// relevant values.
class UsdInUtilExtraHintsDap : public FnDefaultAttributeProducer::DefaultAttributeProducer
{
public:
    static FnAttribute::GroupAttribute cook(
            const FnGeolibOp::GeolibCookInterface & interface,
            const std::string & attrRoot,
            const std::string & inputLocationPath,
            int inputIndex)
    {
        FnAttribute::GroupAttribute entries = interface.getAttr("__usdInExtraHints");

        if (!entries.isValid() || entries.getNumberOfChildren() == 0)
        {
            return FnAttribute::GroupAttribute();
        }
        
        // encoding is attrPath -> groupAttr
        // attrPath is encoded via DelimiterEncode
        
        FnAttribute::GroupBuilder gb;
        for (int64_t i = 0, e = entries.getNumberOfChildren(); i < e; ++i)
        {
            FnAttribute::GroupAttribute hintsAttr = entries.getChildByIndex(i);
            if (!hintsAttr.isValid())
            {
                continue;
            }
            
            
            FnDefaultAttributeProducer::DapUtil::SetAttrHints(
                    gb, FnAttribute::DelimiterDecode(entries.getChildName(i)),
                            hintsAttr);
        }
        return gb.build();
    }
};

DEFINE_DEFAULTATTRIBUTEPRODUCER_PLUGIN(UsdInUtilExtraHintsDap)
}

void registerUsdInShippedUiUtils()
{
    REGISTER_PLUGIN(UsdInUtilExtraHintsDap, "UsdInUtilExtraHintsDap", 0, 1);
}
