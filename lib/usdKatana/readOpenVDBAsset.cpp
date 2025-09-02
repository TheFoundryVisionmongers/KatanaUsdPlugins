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
#include "usdKatana/readOpenVDBAsset.h"

#include <pxr/pxr.h>
#include <pxr/usd/usdVol/openVDBAsset.h>

#include <FnLogging/FnLogging.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readXformable.h"
#include "usdKatana/usdInPrivateData.h"
#include "vtKatana/array.h"

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("UsdKatanaReadOpenVDBAsset");

void UsdKatanaReadOpenVDBAsset(const UsdVolOpenVDBAsset& field,
                               const UsdKatanaUsdInPrivateData& data,
                               UsdKatanaAttrMap& attrs)
{
    UsdKatanaReadXformable(field, data, attrs);
    attrs.set("type", FnKat::StringAttribute("openvdbasset"));
    attrs.set("tabs.scenegraph.stopExpand", FnKat::IntAttribute(1));

    FnAttribute::GroupBuilder fgb;
    // Set all attributes for a fieldAsset type.
    const double currentTime = data.GetCurrentTime();
    SdfAssetPath filePath;
    field.GetFilePathAttr().Get(&filePath, currentTime);
    fgb.set("filePath", FnKat::StringAttribute(filePath.GetResolvedPath()));

    TfToken fieldName;
    field.GetFieldNameAttr().Get(&fieldName);
    fgb.set("fieldName", FnKat::StringAttribute(fieldName.GetString()));

    int fieldIndex = 0;
    if (field.GetFieldIndexAttr().Get(&fieldIndex))
        fgb.set("fieldIndex", FnKat::IntAttribute(fieldIndex));

    TfToken fieldDataType;
    if (field.GetFieldDataTypeAttr().Get(&fieldDataType))
        fgb.set("fieldDataType", FnKat::StringAttribute(fieldDataType.GetString()));

    TfToken fieldClass;
    if (field.GetFieldClassAttr().Get(&fieldClass))
        fgb.set("fieldClass", FnKat::StringAttribute(fieldClass.GetString()));

    FnAttribute::GroupAttribute fieldAttrGroup = fgb.build();

    attrs.set("fieldAttributes", fieldAttrGroup);
}

PXR_NAMESPACE_CLOSE_SCOPE
