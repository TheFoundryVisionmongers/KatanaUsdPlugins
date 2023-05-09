// Copyright (c) 2023 The Foundry Visionmongers Ltd.
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
#include "usdKatana/readVolume.h"

#include <map>
#include <string>

#include <pxr/pxr.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>

#include <FnLogging/FnLogging.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

PXR_NAMESPACE_OPEN_SCOPE

using FieldMap = std::map<TfToken, SdfPath>;

FnLogSetup("UsdKatanaReadVolume");

void UsdKatanaReadVolume(const UsdVolVolume& volume,
                         const UsdKatanaUsdInPrivateData& data,
                         UsdKatanaAttrMap& attrs)
{
    attrs.set("type", FnKat::StringAttribute("volume"));

    // Set all attributes for fields
    FieldMap map = volume.GetFieldPaths();
    for (const auto& pair : map)
    {
        FnAttribute::GroupBuilder gb;
        gb.set("fieldName", FnAttribute::StringAttribute(pair.first.GetString()));
        std::string kat_loc =
            UsdKatanaUtils::ConvertUsdPathToKatLocation(SdfPath(pair.second.GetString()), data);
        gb.set("fieldId", FnAttribute::StringAttribute(kat_loc));

        FnAttribute::GroupAttribute fieldGroup = gb.build();
        std::stringstream os;
        os << "fields." << pair.first.GetString();
        attrs.set(os.str(), fieldGroup);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
