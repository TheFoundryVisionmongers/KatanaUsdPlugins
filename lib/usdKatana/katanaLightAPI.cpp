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
#include "usdKatana/katanaLightAPI.h"

#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/schemaRegistry.h>
#include <pxr/usd/usd/tokens.h>
#include <pxr/usd/usd/typed.h>

PXR_NAMESPACE_OPEN_SCOPE

// Register the schema with the TfType system.
TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<UsdKatanaKatanaLightAPI,
        TfType::Bases< UsdAPISchemaBase > >();

}

/* virtual */
UsdKatanaKatanaLightAPI::~UsdKatanaKatanaLightAPI()
{
}

/* static */
UsdKatanaKatanaLightAPI
UsdKatanaKatanaLightAPI::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdKatanaKatanaLightAPI();
    }
    return UsdKatanaKatanaLightAPI(stage->GetPrimAtPath(path));
}


/* virtual */
UsdSchemaKind UsdKatanaKatanaLightAPI::_GetSchemaKind() const
{
    return UsdKatanaKatanaLightAPI::schemaKind;
}

/* static */
bool
UsdKatanaKatanaLightAPI::CanApply(
    const UsdPrim &prim, std::string *whyNot)
{
    return prim.CanApplyAPI<UsdKatanaKatanaLightAPI>(whyNot);
}

/* static */
UsdKatanaKatanaLightAPI
UsdKatanaKatanaLightAPI::Apply(const UsdPrim &prim)
{
    if (prim.ApplyAPI<UsdKatanaKatanaLightAPI>()) {
        return UsdKatanaKatanaLightAPI(prim);
    }
    return UsdKatanaKatanaLightAPI();
}

/* static */
const TfType &
UsdKatanaKatanaLightAPI::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<UsdKatanaKatanaLightAPI>();
    return tfType;
}

/* static */
bool
UsdKatanaKatanaLightAPI::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
UsdKatanaKatanaLightAPI::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
UsdKatanaKatanaLightAPI::GetIdAttr() const
{
    return GetPrim().GetAttribute(UsdKatanaTokens->katanaId);
}

UsdAttribute
UsdKatanaKatanaLightAPI::CreateIdAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdKatanaTokens->katanaId,
                       SdfValueTypeNames->StringArray,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdKatanaKatanaLightAPI::GetCenterOfInterestAttr() const
{
    return GetPrim().GetAttribute(UsdKatanaTokens->geometryCenterOfInterest);
}

UsdAttribute
UsdKatanaKatanaLightAPI::CreateCenterOfInterestAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdKatanaTokens->geometryCenterOfInterest,
                       SdfValueTypeNames->Double,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

namespace {
static inline TfTokenVector
_ConcatenateAttributeNames(const TfTokenVector& left,const TfTokenVector& right)
{
    TfTokenVector result;
    result.reserve(left.size() + right.size());
    result.insert(result.end(), left.begin(), left.end());
    result.insert(result.end(), right.begin(), right.end());
    return result;
}
}

/*static*/
const TfTokenVector&
UsdKatanaKatanaLightAPI::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        UsdKatanaTokens->katanaId,
        UsdKatanaTokens->geometryCenterOfInterest,
    };
    static TfTokenVector allNames =
        _ConcatenateAttributeNames(
            UsdAPISchemaBase::GetSchemaAttributeNames(true),
            localNames);

    if (includeInherited)
        return allNames;
    else
        return localNames;
}

PXR_NAMESPACE_CLOSE_SCOPE

// ===================================================================== //
// Feel free to add custom code below this line. It will be preserved by
// the code generator.
//
// Just remember to wrap code in the appropriate delimiters:
// 'PXR_NAMESPACE_OPEN_SCOPE', 'PXR_NAMESPACE_CLOSE_SCOPE'.
// ===================================================================== //
// --(BEGIN CUSTOM CODE)--
