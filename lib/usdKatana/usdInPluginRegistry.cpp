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
#include "usdKatana/usdInPluginRegistry.h"

#include <map>
#include <string>
#include <vector>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/pxr.h>
#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/schemaBase.h>

#include <FnLogging/FnLogging.h>

FnLogSetup("UsdInPluginRegistry");

PXR_NAMESPACE_OPEN_SCOPE


typedef std::map<std::string, std::string> _UsdTypeRegistry;
static _UsdTypeRegistry _usdTypeReg;
static _UsdTypeRegistry _usdTypeSiteReg;
typedef std::map<TfToken, std::string> _UsdSchemaRegistry;
static _UsdSchemaRegistry _usdSchemaReg;

/* static */
void UsdKatanaUsdInPluginRegistry::_RegisterUsdType(const std::string& tfTypeName,
                                                    const std::string& opName)
{
    _usdTypeReg[tfTypeName] = opName;
}

/* static */
void UsdKatanaUsdInPluginRegistry::_RegisterSchema(const TfToken& schemaName,
                                                   const std::string& opName)
{
    _usdSchemaReg[schemaName] = opName;
}

/* static */
void UsdKatanaUsdInPluginRegistry::_RegisterUsdTypeForSite(const std::string& tfTypeName,
                                                           const std::string& opName)
{
    _usdTypeSiteReg[tfTypeName] = opName;
}

bool 
_DoFindUsdType(
    const TfToken& usdTypeName,
    std::string* opName,
    const _UsdTypeRegistry& registry)
{
    // unfortunately, usdTypeName is diff from the tfTypeName which we use to
    // register.  do the conversion here mostly in case we want to walk up the
    // type hierarchy
    TfType tfType = PlugRegistry::FindDerivedTypeByName<UsdSchemaBase>(usdTypeName);
    std::string typeNameStr = tfType.GetTypeName();
    return TfMapLookup(registry, typeNameStr, opName);
}

/* static */
bool UsdKatanaUsdInPluginRegistry::FindUsdType(const TfToken& usdTypeName, std::string* opName)
{
    return _DoFindUsdType(usdTypeName, opName, _usdTypeReg);
}

/* static */
bool UsdKatanaUsdInPluginRegistry::FindUsdTypeForSite(const TfToken& usdTypeName,
                                                      std::string* opName)
{
    return _DoFindUsdType(usdTypeName, opName, _usdTypeSiteReg);
}

/* static */
bool UsdKatanaUsdInPluginRegistry::FindSchema(const TfToken& schemaName, std::string* opName)
{
    return TfMapLookup(_usdSchemaReg, schemaName, opName);
}

typedef std::map<TfToken, std::string> _KindRegistry;
static _KindRegistry _kindReg;
static _KindRegistry _kindExtReg;

/* static */
void UsdKatanaUsdInPluginRegistry::RegisterKind(const TfToken& kind, const std::string& opName)
{
    _kindReg[kind] = opName;
}

/* static */
void UsdKatanaUsdInPluginRegistry::RegisterKindForSite(const TfToken& kind,
                                                       const std::string& opName)
{
    _kindExtReg[kind] = opName;
}

/* static */
bool UsdKatanaUsdInPluginRegistry::HasKindsForSite()
{
    return _kindExtReg.size() > 0;
}

/* static */
bool UsdKatanaUsdInPluginRegistry::_DoFindKind(const TfToken& kind,
                                               std::string* opName,
                                               const _KindRegistry& reg)
{
    // can cache this if it becomes an issue.
    TfToken currKind = kind;
    while (!currKind.IsEmpty()) {
        if (TfMapLookup(reg, currKind, opName)) {
            return true;
        }
        if (KindRegistry::HasKind(currKind)) {
            currKind = KindRegistry::GetBaseKind(currKind);
        }
        else {
            FnLogWarn(TfStringPrintf("Unknown kind: '%s'", currKind.GetText()));
            return false;
        }
    }

    return false;
}

/* static */
bool UsdKatanaUsdInPluginRegistry::FindKind(const TfToken& kind, std::string* opName)
{
    return _DoFindKind(kind, opName, _kindReg);
}

/* static */
bool UsdKatanaUsdInPluginRegistry::FindKindForSite(const TfToken& kind, std::string* opName)
{
    return _DoFindKind(kind, opName, _kindExtReg);
}

typedef std::map<std::string, UsdKatanaUsdInPluginRegistry::OpDirectExecFnc> _OpDirectExecFncTable;

static _OpDirectExecFncTable _opDirectExecFncTable;

void UsdKatanaUsdInPluginRegistry::RegisterOpDirectExecFnc(const std::string& opName,
                                                           OpDirectExecFnc fnc)
{
    _opDirectExecFncTable[opName] = fnc;
}

void UsdKatanaUsdInPluginRegistry::ExecuteOpDirectExecFnc(
    const std::string& opName,
    const UsdKatanaUsdInPrivateData& privateData,
    FnKat::GroupAttribute opArgs,
    FnKat::GeolibCookInterface& interface)
{
    _OpDirectExecFncTable::iterator I = _opDirectExecFncTable.find(opName);
    
    if (I != _opDirectExecFncTable.end())
    {
        (*((*I).second))(privateData, opArgs, interface);
    }
}

typedef std::vector<UsdKatanaUsdInPluginRegistry::LightListFnc> _LightListFncList;
static _LightListFncList _lightListFncList;

void UsdKatanaUsdInPluginRegistry::RegisterLightListFnc(LightListFnc fnc)
{
    _lightListFncList.push_back(fnc);
}

void UsdKatanaUsdInPluginRegistry::ExecuteLightListFncs(UsdKatanaUtilsLightListAccess& lightList)
{
    for (auto i : _lightListFncList)
    {
        (*i)(lightList);
    }
}

typedef std::vector<UsdKatanaUsdInPluginRegistry::OpDirectExecFnc> _LocationDecoratorFncList;

static _LocationDecoratorFncList _locationDecoratorFncList;

void UsdKatanaUsdInPluginRegistry::RegisterLocationDecoratorOp(const std::string& opName)
{
    _OpDirectExecFncTable::iterator I = _opDirectExecFncTable.find(opName);
    
    if (I != _opDirectExecFncTable.end())
    {
        _locationDecoratorFncList.push_back((*I).second);
    }
    
}

FnKat::GroupAttribute UsdKatanaUsdInPluginRegistry::ExecuteLocationDecoratorOps(
    const UsdKatanaUsdInPrivateData& privateData,
    FnKat::GroupAttribute opArgs,
    FnKat::GeolibCookInterface& interface)
{
    for (auto i : _locationDecoratorFncList)
    {
        (*i)(privateData, opArgs, interface);
        
        opArgs = privateData.updateExtensionOpArgs(opArgs);
    }
    
    return opArgs;
}




PXR_NAMESPACE_CLOSE_SCOPE

