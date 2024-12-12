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
#include "usdKatana/readPrim.h"

#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/collectionAPI.h>
#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdRi/statementsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdUtils/pipeline.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/blindDataObject.h"
#include "usdKatana/tokens.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

#include <pystring/pystring.h>
#include <FnLogging/FnLogging.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(USD_KATANA_ALLOW_CUSTOM_MATERIAL_SCOPES, true,
        "If set to false, this will limit material assignments to materials "
        "scoped under a Looks location.");

TF_DEFINE_ENV_SETTING(USD_KATANA_API_SCHEMAS_AS_GROUP_ATTR, false,
        "If true, API schemas will be imported as group attributes instead "
        "of an array of strings. This provides easier support for CEL "
        "matching based on API schemas and an easier way to access the. "
        "instance name of Multiple Apply Schemas.");

FnLogSetup("UsdKatanaReadPrim");

static FnKat::Attribute _GetMaterialAssignAttrFromPath(const SdfPath& inputTargetPath,
                                                       const UsdKatanaUsdInPrivateData& data,
                                                       const SdfPath& errorContextPath)
{
    SdfPath targetPath = inputTargetPath;
    const UsdPrim targetPrim = data.GetUsdInArgs()->GetStage()->GetPrimAtPath(targetPath);
    const UsdPrim contextPrim = data.GetUsdInArgs()->GetStage()->GetPrimAtPath(errorContextPath);
    // If the target is inside a prototype, then it needs to be re-targeted
    // to the instance.
    //
    // XXX remove this special awareness once GetPrototypeWithContext is
    //     is available as the provided prim will automatically
    //     retarget (or provide enough context to retarget without
    //     tracking manually).
    if (targetPrim && targetPrim.IsInPrototype()) {
        if (!data.GetInstancePath().IsEmpty() && !data.GetPrototypePath().IsEmpty())
        {
            // Check if the source and the target of the relationship
            // belong to the same prototype.
            // If they do, we have the context necessary to do the
            // re-mapping.
            if (data.GetPrototypePath().GetCommonPrefix(targetPath).GetPathElementCount() > 0)
            {
                targetPath = data.GetInstancePath().AppendPath(
                    targetPath.ReplacePrefix(targetPath.GetPrefixes()[0],
                        SdfPath::ReflexiveRelativePath()));
            }
            else
            {
                // Warn saying the target of relationship isn't within
                // the same prototype as the source.
                FnLogWarn("Target path " << errorContextPath.GetString()
                                         << " isn't within the prototype "
                                         << data.GetPrototypePath());
                return FnKat::Attribute();
            }
        }
        else
        {
            // XXX
            // When loading beneath a prototype via an isolatePath
            // opArg, we can encounter targets which are within prototypes
            // but not within the context of a material.
            // While that would be an error according to the below
            // warning, it produces the expected results.
            // This case can occur when expanding pointinstancers as
            // the sources are made via execution of UsdIn again
            // at the sub-trees.

            // Warn saying target of relationship is in a prototype,
            // but the associated instance path is unknown!
            // FnLogWarn("Target path " << prim.GetPath().GetString()
            //         << " is within a prototype, but the associated "
            //         "instancePath is unknown.");
            // return FnKat::Attribute();
        }
    }
    // If the prim we're doing the assignment on is an instance and the target material prim is an
    // instance proxy of the same prim, we need to work out where the instance source will be to
    // reassign the binding to the "Prototypes/prototype/variants/m<n>/..." location.
    else if (contextPrim && targetPrim && contextPrim.IsInstance() &&
             targetPath.HasPrefix(errorContextPath) && targetPrim.IsInstanceProxy())
    {
        if (const FnKat::GroupAttribute& mappingAttr = data.getInstancePrototypeMapping();
            mappingAttr.isValid())
        {
            const FnAttribute::StringAttribute prototypePathAttr = mappingAttr.getChildByName(
                FnKat::DelimiterEncode(contextPrim.GetPrototype().GetPath().GetString()));
            const std::string prototypePath = prototypePathAttr.getValue("", false);
            const SdfPath path = targetPrim.GetPrimInPrototype()
                                     .GetPath()
                                     .MakeRelativePath(contextPrim.GetPrototype().GetPath())
                                     .MakeAbsolutePath(PXR_NS::SdfPath::AbsoluteRootPath());

            targetPath = SdfPath("/Prototypes/" + prototypePath + path.GetAsString());
            // ConvertUsdMaterialPathToKatLocation will prepend the required root path for us.
        }
    }

    // Convert the target path to the equivalent katana location.
    // XXX: Materials may have an atypical USD->Katana 
    // path mapping
    std::string location = UsdKatanaUtils::ConvertUsdMaterialPathToKatLocation(targetPath, data);

    static const bool allowCustomScopes = 
        TfGetEnvSetting(USD_KATANA_ALLOW_CUSTOM_MATERIAL_SCOPES);
        
    // XXX Materials containing only display terminals are causing issues
    //     with katana material manipulation workflows.
    //     For now: exclude any material assign which doesn't include
    //     /Looks/ in the path
    if (!allowCustomScopes && location.find(UsdKatanaTokens->katanaLooksScopePathSubstring)
            == std::string::npos)
    {
        return FnKat::Attribute();
    }

    return FnKat::StringAttribute(location);
}

static FnKat::Attribute _GetMaterialAssignAttr(const UsdPrim& prim,
                                               const UsdKatanaUsdInPrivateData& data)
{
    if (!prim || prim.GetPath() == SdfPath::AbsoluteRootPath()) {
        // Special-case to pre-empt coding errors.
        return FnKat::Attribute();
    }

    UsdRelationship usdRel =
        UsdShadeMaterialBindingAPI(prim).GetDirectBindingRel();
    if (usdRel) {
        // USD shading binding
        SdfPathVector targetPaths;
        usdRel.GetForwardedTargets(&targetPaths);
        if (targetPaths.size() > 0) {
            if (!targetPaths[0].IsPrimPath()) {
                FnLogWarn("Target path " << prim.GetPath().GetString() <<
                          " is not a prim");
                return FnKat::Attribute();
            }

            return _GetMaterialAssignAttrFromPath(
                    targetPaths[0], data, prim.GetPath());
        }
    }

    return FnKat::Attribute();
}

static FnKat::Attribute _GetCollectionBasedMaterialAssignments(
    const UsdPrim& prim,
    const UsdKatanaUsdInPrivateData& data)
{
    UsdShadeMaterialBindingAPI bindingAPI(prim);

    const TfTokenVector& purposes = data.GetUsdInArgs()->GetMaterialBindingPurposes();
    if (purposes.empty())
    {
        return FnKat::Attribute();    
    }

    FnAttribute::GroupBuilder gb(FnAttribute::GroupBuilder::BuilderModeStrict);
    bool hasBindings = false;

    for (const auto & purpose : purposes)
    {
        // We only hold a cache for purposes which we have been told about. If for whatever reason
        // the purpose here has not been declared on the UsdIn node, use an empty cache by default.
        UsdShadeMaterialBindingAPI::BindingsCache emptyCache;
        UsdShadeMaterialBindingAPI::BindingsCache* cache = data.GetBindingsCache(purpose);
        cache = cache ? cache : &emptyCache;

        if (const UsdShadeMaterial boundMaterial =
                bindingAPI.ComputeBoundMaterial(cache, data.GetCollectionQueryCache(), purpose))
        {
            hasBindings = true;
            gb.set(purpose == UsdShadeTokens->allPurpose ? "allPurpose" : purpose.GetText(),
                   _GetMaterialAssignAttrFromPath(
                       boundMaterial.GetPrim().GetPath(), data, prim.GetPath()));
        }
    }

    if (hasBindings)
    {
        return gb.build();
    }

    return FnKat::Attribute();
}

static bool
_GatherRibAttributes(
        const UsdPrim &prim, 
        double currentTime,
        FnKat::GroupBuilder& attrsBuilder)
{
    bool hasAttrs = false;

    // USD SHADING STYLE ATTRIBUTES
    if (prim) {
        UsdRiStatementsAPI riStatements(prim);
        const std::vector<UsdProperty> props = 
            riStatements.GetRiAttributes();
        std::string attrName;
        TF_FOR_ALL(propItr, props) {
            UsdProperty prop = *propItr;
            if (!prop) continue;

            std::string nameSpace = 
                riStatements.GetRiAttributeNameSpace(prop).GetString();
            nameSpace = TfStringReplace(nameSpace, ":", ".") + ".";

            attrName = nameSpace +
                riStatements.GetRiAttributeName(prop).GetString();

            // XXX asShaderParam really means:
            // "For arrays, as a single attr vs a type/value pair group"
            // The type/value pair group is meaningful for attrs who don't
            // have a formal type definition -- like a "user" RiAttribute.
            //
            // However, other array values (such as two-element shadingrate)
            // are not expecting the type/value pair form and will not
            // generate rib correctly. As such, we'll handle the "user"
            // attribute as a special case.
            const bool asShaderParam = (nameSpace != "user.");

            VtValue vtValue;
            UsdAttribute usdAttr = prim.GetAttribute(prop.GetName());
            if (usdAttr) {
                if (!usdAttr.Get(&vtValue, currentTime)) 
                    continue;
                attrsBuilder.set(attrName,
                                 UsdKatanaUtils::ConvertVtValueToKatAttr(vtValue, asShaderParam));
            }
            else {
                UsdRelationship usdRel = prim.GetRelationship(prop.GetName());
                attrsBuilder.set(attrName,
                                 UsdKatanaUtils::ConvertRelTargetsToKatAttr(usdRel, asShaderParam));
            }
            hasAttrs = true;
        }
    }

    return hasAttrs;
}

void UsdKatanaReadPrimPrmanStatements(const UsdPrim& prim,
                                      double currentTime,
                                      FnKat::GroupBuilder& statements,
                                      const bool prmanOutputTarget)
{
    if (prim.GetPath() == SdfPath::AbsoluteRootPath()) {
        // Special-case to pre-empt coding errors.
        return;
    }

    FnKat::GroupBuilder attrsBuilder;

    if (prmanOutputTarget)
    {
        // Rib attributes -> attributes.*
        _GatherRibAttributes(prim, currentTime, attrsBuilder);
    }
    //
    // Add gprim-specific prmanStatements.
    //
    
    if (UsdGeomGprim gprim = UsdGeomGprim(prim))
    {
        bool doubleSided = false;
        if (gprim.GetDoubleSidedAttr().Get(&doubleSided) && doubleSided)
        {
            statements.set("sides", FnKat::IntAttribute(2));
        }

        // orientation
        // uses non-literal mapping of lh/rh to better match prman behavior
        // as per: http://bugzilla.pixar.com/show_bug.cgi?id=110542
        TfToken orientation;
        if (gprim.GetOrientationAttr().Get(&orientation))
        {
            statements.set("orientation", FnKat::StringAttribute(
                orientation == UsdGeomTokens->leftHanded ? "inside" : "outside"));
        }
    }

    //
    // Take care of Pixar's conventional model-level shader space.
    //
    if (prmanOutputTarget)
    {
        if (UsdModelAPI(prim).IsModel()) {
            statements.set("scopedCoordinateSystem",
                FnKat::StringAttribute("ModelSpace"));
        }
    }

    // XXX:
    // Should we have subclasses add to or modify
    // this builder instead of setting attributes.NAMESPACE.ATTRNAME for each
    // new attr?  Are there performance implications?
    FnKat::GroupAttribute attributesGroup = attrsBuilder.build();
    if (attributesGroup.getNumberOfChildren()){
        statements.set("attributes", attributesGroup);
    }
}

static bool
_BuildScopedCoordinateSystems(
        const UsdPrim& prim,
        FnKat::GroupBuilder& coordSysBuilder)
{
    // We look at the immediate children of this prim for UsdRi-encoded
    // scopedCooordinateSystems, but emit them as a relative coordinate system
    // on this (parent) so they are applicable to all children of this node.

    if (!prim) {
        return false;
    }

    bool foundCoordSys = false;

    TF_FOR_ALL(childIt, prim.GetChildren()) {

        UsdRiStatementsAPI riStmts(*childIt);

        if (!riStmts.HasCoordinateSystem()) {
            continue;
        }

        const std::string gprimName = childIt->GetName();
        std::string usdCoordSysName = riStmts.GetScopedCoordinateSystem();
        if (usdCoordSysName.empty()) {
            usdCoordSysName = gprimName;
        }
        std::string coordSysName = usdCoordSysName;

        coordSysBuilder.set(coordSysName, FnKat::StringAttribute(gprimName));

        // XXX: For backward compatibility we will emit the same coordsys
        // again, prefixed with modelInstanceName.
        //
        // XXX: 20150126: Restoring this backward compatbility
        // shim that the tidscene SGG had.  We're finding we need it
        // to preserve assumptions made internally by the REYES eye
        // shaders.  Possibly we can remove this once we are no longer
        // supporting REYES shows.
        coordSysBuilder.set(UsdKatanaUtils::GetModelInstanceName(prim) + "_" + coordSysName,
                            FnKat::StringAttribute(gprimName));

        foundCoordSys = true;
    }
    
    return foundCoordSys;
}

static void _AppendPathToIncludeExcludeStr(
    const SdfPath &path,
    bool isIncludePath,
    const UsdPrim &prim, 
    const TfToken &srcCollectionName,
    std::stringstream &incExcStr)
{
    // Skip property paths as properties can't be included in a katana 
    // collection (although they can be included by CEL).
    if (path.IsPropertyPath()) {
        return;
    }

    if (path.HasPrefix(prim.GetPath())) {
        const size_t prefixLength = prim.GetPath().GetString().length();
        std::string relativePath = path.GetString().substr(prefixLength);

        // Follow katana convention for collections the "self" location relative 
        // path is "/". Absolute paths start with "/root/" relative paths start 
        // without the "/" though.
        if (relativePath == "")
            relativePath = "/";
        // Add the path and all descendants
        incExcStr << relativePath << " " << ((relativePath != "/") ? relativePath : "") << "//* ";
    }
    else
    {
        FnLogWarn("Collection " << srcCollectionName  <<
                  (isIncludePath ? " includes" : " excludes") << " path "
                  << path.GetString() << " which is not a descendant of the "
                  "collection-owning prim <" << prim.GetPath().GetString()
                  << ">");
    }
}


// CEL cannot use collections whose name contain ":" so we have to do something
// with those within namespaces (specifically the material-binding ones)
static
std::string _GetKatanaCollectionName(const TfToken &collectionName)
{
    return pystring::replace(collectionName.GetString(), ":", "__");
}

static std::string _GetKatanaCollectionPath(const SdfPath& collPrimPath,
                                            const TfToken& collectionName,
                                            const UsdPrim& prim,
                                            const TfToken& srcCollectionName,
                                            const UsdKatanaUsdInPrivateData& data)
{
    std::string katanaCollectionName(_GetKatanaCollectionName(collectionName));

    if (collPrimPath.HasPrefix(prim.GetPath())) {
        const size_t prefixLength = prim.GetPath().GetString().length();
        std::string relativePath = collPrimPath.GetString().substr(prefixLength);
        // follow katana convention for collections
        // the "self" location relative path is "/". 
        // Absolute paths start with "/root/"
        // relative paths start without the "/" though.
        if (relativePath == "")
            relativePath = "/";

        return TfStringPrintf("(%s/$%s)", relativePath.c_str(), 
                              katanaCollectionName.c_str());
    } else {
        FnLogWarn("Collection " << srcCollectionName   
            << " includes collection " << collPrimPath << ".collection:" << 
            collectionName << " which is not a descendant of the collection-"
            "owning prim <" << prim.GetPath().GetString() << ">");

        // If the collection is not a descendant, add the full 
        // katana location of the collection. 
        // This won't cause the collection to be included, but
        // does not cause any errors either and might give us 
        // a way to roundtrip the include back to USD.

        const std::string katPrimPath =
            UsdKatanaUtils::ConvertUsdPathToKatLocation(collPrimPath, data);
        return TfStringPrintf("(%s/$%s)", katPrimPath.c_str(), 
                              katanaCollectionName.c_str());
    }
}

static bool _BuildCollections(const UsdPrim& prim,
                              const UsdKatanaUsdInPrivateData& data,
                              FnKat::GroupBuilder& collectionsBuilder)
{
    std::vector<UsdCollectionAPI> collections = 
        UsdCollectionAPI::GetAllCollections(prim);

    const size_t prefixLength = prim.GetPath().GetString().length();

    for (const UsdCollectionAPI &collection : collections) {
        TfToken expansionRule; 
        collection.GetExpansionRuleAttr().Get(&expansionRule);

        if (expansionRule != UsdTokens->explicitOnly) {
            UsdRelationship includesRel = collection.GetIncludesRel();
            UsdRelationship excludesRel = collection.GetExcludesRel();
            
            SdfPathVector includes, excludes; 
            includesRel.GetTargets(&includes);
            excludesRel.GetTargets(&excludes);

            // Exclude the collection if it's empty.
            if (includes.empty()) {
                continue;
            }

            FnKat::StringBuilder collectionBuilder;

            std::stringstream incExcStr;
            incExcStr << "((";
            for (const SdfPath &p : includes) {
                TfToken collectionName;
                if (UsdCollectionAPI::IsCollectionAPIPath(p, &collectionName)) {
                    SdfPath collPrimPath= p.GetPrimPath();
                    std::string katCollStr = _GetKatanaCollectionPath(collPrimPath, 
                        collectionName, prim, collection.GetName(), data);
                    collectionBuilder.push_back(katCollStr);
                } else {
                    _AppendPathToIncludeExcludeStr(p, /*isIncludePath */ true, 
                            prim, collection.GetName(), incExcStr);
                }
            }
            incExcStr << ")";

            if (!excludes.empty()) {
                incExcStr << " - (";
                for (const SdfPath &p: excludes) {
                    _AppendPathToIncludeExcludeStr(p, /* isIncludePath */ false,
                            prim, collection.GetName(), incExcStr);
                }
                incExcStr << ")";
            }
            incExcStr << ")";

            // Add the string that encodes the includes and excludes if it's 
            // not empty.
            if (incExcStr.str() != "(() - ())" && incExcStr.str() != "(())") {
                collectionBuilder.push_back(incExcStr.str());
            }

            FnKat::StringAttribute collectionAttr = collectionBuilder.build();
            if (collectionAttr.getNearestSample(0).size() > 0) {
                collectionsBuilder.set(_GetKatanaCollectionName(collection.GetName()) + ".cel",
                                       collectionAttr);
            }
        } else {
            // Bake the collection as a flat list of member paths.
            const auto &mquery = collection.ComputeMembershipQuery();
            SdfPathSet includedPaths = UsdCollectionAPI::ComputeIncludedPaths(
                    mquery, prim.GetStage()); 
            FnKat::StringBuilder collectionBuilder;
            for (const SdfPath &p : includedPaths) {
                if (p.HasPrefix(prim.GetPath())) {
                    std::string relativePath = p.GetString().substr(prefixLength);
                    // follow katana convention for collections
                    // the "self" location relative path is "/". 
                    // Absolute paths start with "/root/"
                    // relative paths start without the "/" though.
                    if (relativePath == "")
                        relativePath = "/";
                    collectionBuilder.push_back(relativePath);
                } else {
                    FnLogWarn("Collection " << collection.GetName()  << " includes "
                        "path " << p.GetString() << " which is not a descendant "
                        "of the collection-owning prim <" 
                        << prim.GetPath().GetString() << ">");
                }
            }

            // If empty, no point creating collection
            FnKat::StringAttribute collectionAttr = collectionBuilder.build();
            if (collectionAttr.getNearestSample(0).size() > 0) {
                collectionsBuilder.set(_GetKatanaCollectionName(collection.GetName()) + ".baked",
                                       collectionAttr);
            }
        }
    }

    return collectionsBuilder.isValid();
}

static void _AddExtraAttributesOrNamespaces(const UsdPrim& prim,
                                            const UsdKatanaUsdInPrivateData& data,
                                            UsdKatanaAttrMap& attrs)
{
    const std::string& rootLocation = 
        data.GetUsdInArgs()->GetRootLocationPath();
    const double currentTime = data.GetCurrentTime();

    const UsdKatanaUsdInArgs::StringListMap& extraAttributesOrNamespaces =
        data.GetUsdInArgs()->GetExtraAttributesOrNamespaces();

    UsdKatanaUsdInArgs::StringListMap::const_iterator I = extraAttributesOrNamespaces.begin();
    for (; I != extraAttributesOrNamespaces.end(); ++I)
    {
        const std::string& name = (*I).first;
        const std::vector<std::string>& names = (*I).second;
        
        FnKat::GroupBuilder gb;

        for (std::vector<std::string>::const_iterator I = names.begin(),
                E = names.end(); I != E; ++I)
        {
            const std::string& propOrNamespace = (*I);
            
            std::vector<UsdAttribute> usdAttrs;
            std::vector<UsdRelationship> usdRelationships;
            
            if (UsdAttribute directAttribute =
                    prim.GetAttribute(TfToken(propOrNamespace)))
            {
                usdAttrs.push_back(directAttribute);
            }
            else if (UsdRelationship directRelationship =
                    prim.GetRelationship(TfToken(propOrNamespace)))
            {
                usdRelationships.push_back(directRelationship);
            }
            else
            {
                std::vector<UsdProperty> props = 
                        prim.GetPropertiesInNamespace(propOrNamespace);
                
                for (std::vector<UsdProperty>::const_iterator I =
                        props.begin(), E = props.end(); I != E; ++I)
                {
                    const UsdProperty & prop = (*I);
                    
                    if (UsdAttribute attr = prop.As<UsdAttribute>())
                    {
                        usdAttrs.push_back(attr);
                    }
                    else if (UsdRelationship rel =
                            prop.As<UsdRelationship>())
                    {
                        usdRelationships.push_back(rel);
                    }
                }
            }
            
            for (std::vector<UsdAttribute>::iterator I = usdAttrs.begin(),
                    E = usdAttrs.end(); I != E; ++I)
            {
                UsdAttribute & usdAttr = (*I);
                
                VtValue vtValue;
                if (!usdAttr.Get(&vtValue, currentTime))
                {
                    continue;
                }

                FnKat::Attribute attr = UsdKatanaUtils::ConvertVtValueToKatAttr(vtValue);

                if (!attr.isValid())
                {
                    continue;
                }
                
                gb.set(pystring::replace(
                    usdAttr.GetName().GetString(), ":", "."), attr);
            }
            
            for (std::vector<UsdRelationship>::iterator I =
                    usdRelationships.begin(), E = usdRelationships.end();
                            I != E; ++I)
            {
                UsdRelationship & usdRelationship = (*I);

                FnKat::StringAttribute attr =
                    UsdKatanaUtils::ConvertRelTargetsToKatAttr(usdRelationship);
                if (!attr.isValid())
                {
                    continue;
                }

                // Further prefix with the UsdIn root scenegraph
                // location in order to make it a valid katana path.
                // XXX, move this into UsdKatanaUtils::ConvertRelTargetsToKatAttr
                // for future implementations.

                FnKat::StringAttribute::array_type values =
                        attr.getNearestSample(0.0f);
                
                std::vector<std::string> prefixedValues;
                prefixedValues.reserve(values.size());
                
                for (size_t i = 0; i < values.size(); ++i)
                {
                    std::ostringstream buffer;
                    buffer << rootLocation;
                    
                    if (values[i][0] && values[i][0] != '/')
                    {
                        buffer << '/';
                    }
                    buffer << values[i];
                    
                    prefixedValues.push_back(buffer.str());
                }
                
                gb.set(pystring::replace(
                        usdRelationship.GetName().GetString(),
                                ":", "."), FnKat::StringAttribute(
                                        prefixedValues));
            }
        }

        FnKat::GroupAttribute result = gb.build();
        attrs.set(name, result.getChildByName(name));
    }
}

static bool
_AddCustomProperties(
        const UsdPrim &prim,
        double currentTime,
        FnKat::GroupBuilder& customBuilder)
{
    if (TfGetenv("USD_KATANA_ADD_CUSTOM_PROPERTIES", "1") == "0")
    {
        return false;
    }

    bool foundCustomProperties = false;

    const std::vector<UsdAttribute>& usdAttributes = prim.GetAttributes();
    for (size_t i = 0; i < usdAttributes.size(); ++i)
    {
        const UsdAttribute& usdAttr = usdAttributes[i];
        if (!usdAttr.IsCustom())
        {
            continue;
        }
        
        VtValue vtValue;
        if (!usdAttr.Get(&vtValue, currentTime))
        {
            continue;
        }

        FnKat::Attribute attr = UsdKatanaUtils::ConvertVtValueToKatAttr(vtValue);

        if (!attr.isValid())
        {
            continue;
        }
        
        customBuilder.set(pystring::replace(
            usdAttr.GetName().GetString(), ":", "."), attr);

        foundCustomProperties = true;
    }

    return foundCustomProperties;
}

FnKat::Attribute UsdKatanaGeomGetPrimvarGroup(const UsdGeomImageable& imageable,
                                              const UsdKatanaUsdInPrivateData& data)
{
    // Usd primvars -> Primvar attributes
    FnKat::GroupBuilder gdBuilder;

    std::vector<UsdGeomPrimvar> primvarAttrs = UsdGeomPrimvarsAPI(imageable).GetPrimvars();
    TF_FOR_ALL(primvar, primvarAttrs) {
        // Katana backends (such as RFK) are not prepared to handle
        // groups of primvars under geometry.arbitrary, which leaves us
        // without a ready-made way to incorporate namespaced primvars like
        // "primvars:skel:jointIndices".  Until we untangle that, skip importing
        // any namespaced primvars.
        if (primvar->NameContainsNamespaces())
            continue;

        // If there is a block from blind data, skip to avoid the cost
        UsdKatanaBlindDataObject kbd(imageable.GetPrim());

        // XXX If we allow namespaced primvars (by eliminating the
        // short-circuit above), we will require GetKbdAttribute to be able
        // to translate namespaced names...
        UsdAttribute blindAttr = kbd.GetKbdAttribute("geometry.arbitrary." +
                                                     primvar->GetPrimvarName().GetString());

        if (blindAttr.GetResolveInfo().ValueIsBlocked()) {
            continue;
        }

        TfToken          name, interpolation;
        SdfValueTypeName typeName;
        int              elementSize;

        // GetDeclarationInfo inclues all namespaces other than "primvars:" in
        // 'name'
        primvar->GetDeclarationInfo(&name, &typeName,
                                    &interpolation, &elementSize);

        // Name: this will eventually need to know how to translate namespaces
        std::string gdName = name;

        VtValue vtValue;
        VtIntArray indices;
        bool isFaceVarying = false;
        // Convert interpolation -> scope
        FnKat::StringAttribute scopeAttr;
        const bool isCurve = imageable.GetPrim().IsA<UsdGeomCurves>();
        if (isCurve && interpolation == UsdGeomTokens->varying)
        {
            // it's a curve, so "varying" == "vertex"
            scopeAttr = FnKat::StringAttribute("vertex");
        }
        else if (interpolation == UsdGeomTokens->faceVarying)
        {
            scopeAttr = FnKat::StringAttribute("vertex");
            if (primvar->GetAttr().Get(&vtValue, data.GetCurrentTime()) &&
                primvar->GetIndices(&indices, data.GetCurrentTime())) {
                isFaceVarying = true;
            }
        }
        else
        {
            scopeAttr = FnKat::StringAttribute(
                    (interpolation == UsdGeomTokens->faceVarying) ? "vertex" :
                    (interpolation == UsdGeomTokens->varying)     ? "point" :
                    (interpolation == UsdGeomTokens->vertex)      ? "point" /*see below*/ :
                    (interpolation == UsdGeomTokens->uniform)     ? "face" :
                    "primitive" );
        }

        // Resolve the value if not face-varying
        if (!isFaceVarying && !primvar->ComputeFlattened(
                &vtValue, data.GetCurrentTime()))
        {
            continue;
        }

        // Convert value to the required Katana attributes to describe it.
        FnKat::Attribute valueAttr, inputTypeAttr, elementSizeAttr;
        UsdKatanaUtils::ConvertVtValueToKatCustomGeomAttr(
            vtValue, elementSize, typeName.GetRole(), &valueAttr, &inputTypeAttr, &elementSizeAttr);

        // Bundle them into a group attribute
        FnKat::GroupBuilder attrBuilder;
        attrBuilder.set("scope", scopeAttr);
        attrBuilder.set("inputType", inputTypeAttr);

        if (!typeName.GetRole().GetString().empty()) {
            attrBuilder.set("usd.role",
                            FnKat::StringAttribute(typeName.GetRole().GetString()));
        }

        if (elementSizeAttr.isValid()) {
            attrBuilder.set("elementSize", elementSizeAttr);
        }

        if (isFaceVarying) {
            attrBuilder.set("indexedValue", valueAttr);
            attrBuilder.set("index", FnAttribute::IntAttribute(indices.data(), indices.size(), 1));
        } else {
            attrBuilder.set("value", valueAttr);
            // Note that 'varying' vs 'vertex' require special handling, as in
            // Katana they are both expressed as 'point' scope above. To get
            // 'vertex' interpolation we must set an additional
            // 'interpolationType' attribute.  So we will flag that here.
            if (interpolation == UsdGeomTokens->vertex) {
                attrBuilder.set("interpolationType",
                                FnKat::StringAttribute("subdiv"));
            }
        }

        gdBuilder.set(gdName, attrBuilder.build());
    }

    return gdBuilder.build();
}

void UsdKatanaReadPrim(const UsdPrim& prim,
                       const UsdKatanaUsdInPrivateData& data,
                       UsdKatanaAttrMap& attrs)
{
    const double currentTime = data.GetCurrentTime();

    const bool prmanOutputTarget = data.hasOutputTarget("prman");

    //
    // Set the 'kind' attribute to match the model kind.
    //

    TfToken kind;
    if (UsdModelAPI(prim).GetKind(&kind))
    {
        attrs.set("kind", FnKat::StringAttribute(kind.GetString()));
    }

    //
    // Set the 'materialAssign' attribute for locations that have shaders.
    //

    attrs.set("materialAssign", _GetMaterialAssignAttr(prim, data));

    //
    // Set the 'prmanStatements' attribute.
    //

    FnKat::GroupBuilder statementsBuilder;
    UsdKatanaReadPrimPrmanStatements(prim, data.GetCurrentTime(), statementsBuilder,
                                     prmanOutputTarget);
    FnKat::GroupAttribute statements = statementsBuilder.build();
    if (statements.getNumberOfChildren() > 0)
    {
        if (prmanOutputTarget)
        {
            attrs.set("prmanStatements", statements);
        }
        attrs.set("usd", statements);
    }

    //
    // Set the 'usd.materialBindings' attribute from collection-based material
    // bindings.
    //

    FnKat::Attribute bindingsAttr = _GetCollectionBasedMaterialAssignments(prim, data);
    if (bindingsAttr.isValid())
    {
        attrs.set("usd.materialBindings", bindingsAttr);
    }

    //
    // Set the 'visible' attribute. Since locations are visible by default
    // only set the attribute if the prim is imageable and invisible.
    //

    TfToken visibility;
    UsdGeomImageable imageable = UsdGeomImageable(prim);
    if (imageable && imageable.GetVisibilityAttr().Get(&visibility, currentTime))
    {
        if (visibility == UsdGeomTokens->invisible)
        {
            attrs.set("visible", FnKat::IntAttribute(0));
        }
    }

    //
    // Set the 'purpose' attribute to exactly match the USD attribute if it is not 'default'
    // (since that is the default value). And, if the 'purpose' happens to be 'proxy' or 'guide'
    // make invisible as well.
    //

    TfToken purpose;
    if (imageable && UsdGeomImageable(prim).GetPurposeAttr().Get(&purpose))
    {
        if (purpose != UsdGeomTokens->default_)
        {
            attrs.set("usd.purpose", FnKat::StringAttribute(purpose.GetString()));
        }

        if (purpose == UsdGeomTokens->proxy || purpose == UsdGeomTokens->guide)
        {
            attrs.set("visible", FnKat::IntAttribute(0));
        }
    }

    //
    // Set the primvar attributes
    //

    if (imageable)
    {
        FnKat::GroupAttribute primvarGroup = UsdKatanaGeomGetPrimvarGroup(imageable, data);

        if (primvarGroup.isValid())
        {
            FnKat::GroupBuilder arbBuilder;
            arbBuilder.update(primvarGroup);
            
            FnKat::GroupAttribute arbGroup = arbBuilder.build();
            if (arbGroup.getNumberOfChildren() > 0)
            {
                attrs.set("geometry.arbitrary", arbGroup);
            }
        }
    }

    //
    // Set the 'relativeScopedCoordinateSystems' attribute if such coordinate
    // systems are found in the children of this prim.
    //

    FnKat::GroupBuilder coordSysBuilder;
    if (_BuildScopedCoordinateSystems(prim, coordSysBuilder))
    {
        attrs.set("relativeScopedCoordinateSystems", coordSysBuilder.build());
    }

    //
    // Set the 'collections' attribute if any found
    //

    FnKat::GroupBuilder collectionsBuilder;
    if (_BuildCollections(prim, data, collectionsBuilder))
    {
        attrs.set("collections", collectionsBuilder.build());
    }


    //
    // Set the 'customProperties' attribute (if enabled by env variable).
    //

    FnKat::GroupBuilder customBuilder;
    if (_AddCustomProperties(prim, currentTime, customBuilder))
    {
        attrs.set("customProperties", customBuilder.build());
    }

    _AddExtraAttributesOrNamespaces(prim, data, attrs);

    // 
    // Store the applied apiSchemas metadata a either a list of
    // strings or a group of int attributes whose name will be
    // the name of the schema (or schema.instanceName) and whose
    // value will be 1 if the schema is active.
    //
    // In a future release, we'll retire the list of strings
    // representation.
    //
    TfTokenVector appliedSchemaTokens = prim.GetAppliedSchemas();
    if (!appliedSchemaTokens.empty()){
        static const bool apiSchemasAsGroupAttr = 
                TfGetEnvSetting(USD_KATANA_API_SCHEMAS_AS_GROUP_ATTR);
        if (apiSchemasAsGroupAttr){
            for (const TfToken& schema : appliedSchemaTokens){
                std::vector<std::string> tokenizedSchema = 
                    TfStringTokenize(schema.GetString(), ":");
                if (tokenizedSchema.size() == 1){                
                    // single apply schemas
                    std::string attrName = 
                        TfStringPrintf("info.usd.apiSchemas.%s",
                                       tokenizedSchema[0].c_str());
                    attrs.set(attrName, FnKat::IntAttribute(1));
                } 
                else if (tokenizedSchema.size() > 1){
                    // multi apply schemas
                    std::string instanceName = TfStringJoin(
                        tokenizedSchema.begin() + 1, tokenizedSchema.end());
                    std::string attrName = 
                        TfStringPrintf("info.usd.apiSchemas.%s.%s",
                                       tokenizedSchema[0].c_str(),
                                       instanceName.c_str());
                    attrs.set(attrName, FnKat::IntAttribute(1));
                }
                else{
                    TF_WARN("apiSchema token '%s' cannot be decomposed into "
                            "a schema name and an (optional) instance name.",
                            schema.GetText());
                }
            }
        } else{
            std::vector<std::string> appliedSchemas(appliedSchemaTokens.size());
            std::transform(appliedSchemaTokens.begin(), appliedSchemaTokens.end(), 
                           appliedSchemas.begin(), [](const TfToken& token){ 
                return token.GetString();
            });
            attrs.set("info.usd.apiSchemas", FnKat::StringAttribute(appliedSchemas));
        }
    }

    // 
    // Store the composed inherits metadata as a group attribute
    //
    SdfPathVector inheritPaths = prim.GetInherits().GetAllDirectInherits();
    if (!inheritPaths.empty()){
        FnKat::GroupBuilder inheritPathsBuilder;
        for (const auto& path : inheritPaths){
            inheritPathsBuilder.set(path.GetName(), FnKat::IntAttribute(1));
        }
        attrs.set("info.usd.inheritPaths", inheritPathsBuilder.build());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

