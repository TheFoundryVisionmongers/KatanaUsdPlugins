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
#include "pxr/pxr.h"

#include <FnPluginManager/FnPluginManager.h>
#include <FnRendererInfo/plugin/RendererInfoBase.h>
#include <FnRendererInfo/suite/FnRendererInfoSuite.h>
#include <FnRendererInfo/FnRendererInfoPluginClient.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/readMaterial.h"
#include "usdKatana/readPrim.h"
#include "usdKatana/utils.h"
#include "usdKatana/baseMaterialHelpers.h"

#include "pxr/base/tf/stringUtils.h"
#include "pxr/imaging/hio/glslfx.h"

#include "pxr/usd/usdGeom/scope.h"

#include "pxr/usd/usdShade/connectableAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/utils.h"

#include "pxr/usd/usdRi/materialAPI.h"
#include "pxr/usd/usdRi/risObject.h"
#include "pxr/usd/usdRi/risOslPattern.h"
#include "pxr/usd/usdRi/rslShader.h"
#include "pxr/usd/usdUI/nodeGraphNodeAPI.h"

#include <FnConfig/FnConfig.h>
#include <FnGeolibServices/FnAttributeFunctionUtil.h>
#include <FnLogging/FnLogging.h>
#include <pystring/pystring.h>

#include <map>
#include <mutex>
#include <stack>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("PxrUsdKatanaReadMaterial");

static std::string
_CreateShadingNode(
        UsdPrim shadingNode,
        double currentTime,
        FnKat::GroupBuilder& nodesBuilder,
        FnKat::GroupBuilder& interfaceBuilder,
        FnKat::GroupBuilder& layoutBuilder,
        const std::string & targetName,
        bool flatten);

std::string
_GetRenderTarget(const std::string& shaderId);

FnKat::Attribute
_GetMaterialAttr(
        const UsdShadeMaterial& materialSchema,
        double currentTime,
        const std::string& targetName,
        const bool prmanOutputTarget,
        bool flatten);

void
_UnrollInterfaceFromPrim(
        const UsdPrim& prim,
        const std::string& paramPrefix,
        FnKat::GroupBuilder& materialBuilder,
        FnKat::GroupBuilder& interfaceBuilder);

void
PxrUsdKatanaReadMaterial(
        const UsdShadeMaterial& material,
        bool flatten,
        const PxrUsdKatanaUsdInPrivateData& data,
        PxrUsdKatanaAttrMap& attrs,
        const std::string& looksGroupLocation,
        const std::string& materialDestinationLocation)
{
    UsdPrim prim = material.GetPrim();
    UsdStageRefPtr stage = prim.GetStage();
    SdfPath primPath = prim.GetPath();
    std::string katanaPath = prim.GetName();

    const bool prmanOutputTarget = data.hasOutputTarget("prman");
    std::string targetName = FnConfig::Config::get("DEFAULT_RENDERER");
    // To ensure that the target field on a material node is set to work
    // with the current renderer, we use the config.  In the future we would
    // like to be able to load in multiple supported output shaders for generic
    // materials, but for now we keep it simple and retain the default of prman
    // We may need more info from the USD file to determine which renderer the
    // material was designed for, and therefore what attributes to set.
    if (targetName.empty())
    {
        targetName = "prman";
    }

    // we do this before ReadPrim because ReadPrim calls ReadBlindData
    // (primvars only) which we don't want to stomp here.
    attrs.set("material", _GetMaterialAttr(material, data.GetCurrentTime(),
        targetName, prmanOutputTarget, flatten));

    const std::string& parentPrefix = (looksGroupLocation.empty()) ?
        data.GetUsdInArgs()->GetRootLocationPath() : looksGroupLocation;

    std::string fullKatanaPath = !materialDestinationLocation.empty()
            ? materialDestinationLocation
            : PxrUsdKatanaUtils::ConvertUsdMaterialPathToKatLocation(
                    primPath, data);

    if (!fullKatanaPath.empty() &&
            pystring::startswith(fullKatanaPath, parentPrefix)) {
        katanaPath = fullKatanaPath.substr(parentPrefix.size()+1);

        // these paths are relative in katana
        if (!katanaPath.empty() && katanaPath[0] == '/') {
            katanaPath = katanaPath.substr(1);
        }
    }


    attrs.set("material.katanaPath", FnKat::StringAttribute(katanaPath));
    attrs.set("material.usdPrimName", FnKat::StringAttribute(prim.GetName()));

    PxrUsdKatanaReadPrim(material.GetPrim(), data, attrs);

    attrs.set("type", FnKat::StringAttribute("material"));

    // clears out prmanStatements.
    attrs.set("prmanStatements", FnKat::Attribute());
}


////////////////////////////////////////////////////////////////////////
// Protected methods

static std::string
_GetKatanaTerminalName(const std::string& terminalName)
{
    if (terminalName.empty())
    {
        return terminalName;
    }

    size_t offset = 0;
    std::string prefix;
    if (TfStringStartsWith(terminalName, "ri:"))
    {
        // ri:terminalName -> prmanTerminalName.
        offset = strlen("ri:");
        prefix = "prman";
    }
    else if (TfStringStartsWith(terminalName, "glslfx:"))
    {
        // glslfx:terminalName -> usdTerminalName.
        offset = strlen("glslfx:");
        prefix = "usd";
    }
    else if (TfStringStartsWith(terminalName, "arnold:"))
    {
        // arnold:terminalName -> arnoldTerminalName.
        offset = strlen("arnold:");
        prefix = "arnold";
    }
    else if (TfStringStartsWith(terminalName, "nsi:"))
    {
        // nsi:terminalName -> dlTerminalName.
        offset = strlen("nsi:");
        prefix = "dl";
    }
    else
    {
        // terminalName -> usdTerminalName.
        offset = 0;
        prefix = "usd";
    }

    std::string result = prefix + terminalName.substr(offset);
    if (!prefix.empty())
    {
        result[prefix.size()] = toupper(result[prefix.size()]);
    }

    return result;
}

// Helper to revert encodings made in UsdExport/material.py.
static std::string
_DecodeUsdExportTerminalName(const std::string& terminalName)
{
    // Dots were replaced with colons.
    std::string result = TfStringReplace(terminalName, ":", ".");

    // "prmanBxdf" was replaced with "prmanSurface".
    if (result == "prmanSurface")
    {
        result = "prmanBxdf";
    }

    return result;
}

template<typename UsdShadeT>
static void
_ProcessShaderConnections(
    const UsdPrim& prim,
    const UsdShadeT& connection,
    const std::string& handle,
    double currentTime,
    FnKat::GroupBuilder& nodesBuilder,
    FnKat::GroupBuilder& paramsBuilder,
    FnKat::GroupBuilder& interfaceBuilder,
    FnKat::GroupBuilder& layoutBuilder,
    FnKat::GroupBuilder& connectionsBuilder,
    const std::string& targetName,
    bool flatten)
{
    static_assert(std::is_same<UsdShadeT, UsdShadeInput>::value ||
                  std::is_same<UsdShadeT, UsdShadeOutput>::value,
                  "UsdShadeT must be an input or an output.");

    std::string connectionId = connection.GetBaseName();

    // We do not try to extract presentation metadata from parameters -
    // only material interface attributes should bother recording such.

    // We can have multiple incoming connection, we get a whole set of paths
    SdfPathVector sourcePaths;
    if (UsdShadeConnectableAPI::GetRawConnectedSourcePaths(
            connection, &sourcePaths))
    {
        bool multipleConnections = sourcePaths.size() > 1;

        // Check the relationship(s) representing this connection to see if
        // the targets come from a base material. If so, ignore them.
        bool createConnections =
            flatten ||
            !UsdShadeConnectableAPI::IsSourceConnectionFromBaseMaterial(
                connection);

        int connectionIdx = 0;
        for (const SdfPath& sourcePath : sourcePaths)
        {
            // We only care about connections to output properties
            if (!sourcePath.IsPropertyPath())
            {
                continue;
            }

            UsdShadeConnectableAPI source =
                UsdShadeConnectableAPI::Get(prim.GetStage(),
                                            sourcePath.GetPrimPath());
            if (!static_cast<bool>(source))
            {
                continue;
            }

            TfToken sourceName;
            UsdShadeAttributeType sourceType;
            std::tie(sourceName, sourceType) =
                UsdShadeUtils::GetBaseNameAndType(
                    sourcePath.GetNameToken());
            if (sourceType != UsdShadeAttributeType::Output)
            {
                continue;
            }

            std::string targetHandle = _CreateShadingNode(
                source.GetPrim(),
                currentTime,
                nodesBuilder,
                interfaceBuilder,
                layoutBuilder,
                targetName,
                flatten);

            // These targets are local, so include them.
            if (createConnections)
            {
                bool validData = false;
                UsdShadeShader shaderSchema = UsdShadeShader(prim);
                if (shaderSchema)
                {
                    validData = true;
                }
                // Only assume the connection needs terminal decoding if
                // it is for an invalid node. I.e the NetworkMaterial node.
                std::string connAttrName = connectionId;
                if (!validData)
                {
                    connAttrName = _DecodeUsdExportTerminalName(
                        _GetKatanaTerminalName(connectionId));
                }

                // In the case of multiple input connections for array
                // types, we append a ":idx" to the name.
                if (multipleConnections)
                {
                    connAttrName += ":" + std::to_string(connectionIdx);
                    connectionIdx++;
                }
                std::string sourceStr = sourceName.GetString();
                if (!validData)
                {
                    sourceStr = _DecodeUsdExportTerminalName(sourceStr);
                }
                sourceStr += '@' + targetHandle;
                connectionsBuilder.set(connAttrName,
                                       FnKat::StringAttribute(sourceStr));
            }
        }
    }
    else
    {
        // This input may author an opinion which blocks connections (eg, a
        // connection from a base material). A blocked connection manifests
        // as an authored connection, but no connections can be determined.
        UsdAttribute inputAttr = connection.GetAttr();
        bool hasAuthoredConnections = inputAttr.HasAuthoredConnections();
        SdfPathVector conns;
        inputAttr.GetConnections(&conns);

        // Use a NullAttribute to capture the block
        if (hasAuthoredConnections && conns.empty())
        {
            connectionsBuilder.set(connectionId, FnKat::NullAttribute());
        }
    }

    // produce the value here and let katana handle the connection part
    // correctly..
    UsdAttribute attr = connection.GetAttr();
    VtValue vtValue;
    if (!attr.Get(&vtValue, currentTime))
    {
        return;
    }

    // If the attribute value comes from a base material, leave it
    // empty -- we will inherit it from the parent katana material.
    if (flatten || !PxrUsdKatana_IsAttrValFromBaseMaterial(attr))
    {
        paramsBuilder.set(connectionId,
                          PxrUsdKatanaUtils::ConvertVtValueToKatAttr(vtValue));
    }
}

static void
_GatherShadingParameters(
    const UsdShadeShader &shaderSchema,
    const std::string &handle,
    double currentTime,
    FnKat::GroupBuilder& nodesBuilder,
    FnKat::GroupBuilder& paramsBuilder,
    FnKat::GroupBuilder& interfaceBuilder,
    FnKat::GroupBuilder& layoutBuilder,
    FnKat::GroupBuilder& connectionsBuilder,
    const std::string & targetName,
    bool flatten)
{
    UsdPrim prim = shaderSchema.GetPrim();
    std::string primName = prim.GetName();

    std::vector<UsdShadeInput> shaderInputs = shaderSchema.GetInputs();
    TF_FOR_ALL(shaderInputIter, shaderInputs)
    {
        _ProcessShaderConnections(
            prim, *shaderInputIter, handle, currentTime, nodesBuilder,
            paramsBuilder, interfaceBuilder, layoutBuilder, connectionsBuilder,
            targetName, flatten);
    }

    std::vector<UsdShadeOutput> shaderOutputs = shaderSchema.GetOutputs();
    TF_FOR_ALL(shaderOutputIter, shaderOutputs)
    {
        _ProcessShaderConnections(
            prim, *shaderOutputIter, handle, currentTime, nodesBuilder,
            paramsBuilder, interfaceBuilder, layoutBuilder, connectionsBuilder,
            targetName, flatten);
    }
}

static void
_ReadLayoutAttrs(const UsdPrim& shadingNode, const std::string& handle,
                 FnKat::GroupBuilder& layoutBuilder)
{
    UsdUINodeGraphNodeAPI nodeApi(shadingNode);

    // Read displayColor.
    UsdAttribute displayColorAttr = nodeApi.GetDisplayColorAttr();
    if (displayColorAttr)
    {
        GfVec3f color;
        if (displayColorAttr.Get(&color))
        {
            float value[3] = { color[0], color[1], color[2] };
            layoutBuilder.set(handle + ".color",
                              FnKat::FloatAttribute(value, 3, 3));
            layoutBuilder.set(handle + ".nodeShapeAttributes.colorr",
                              FnKat::FloatAttribute(value[0]));
            layoutBuilder.set(handle + ".nodeShapeAttributes.colorg",
                              FnKat::FloatAttribute(value[1]));
            layoutBuilder.set(handle + ".nodeShapeAttributes.colorb",
                              FnKat::FloatAttribute(value[2]));
        }
    }

    // Read position.
    UsdAttribute posAttr = nodeApi.GetPosAttr();
    if (posAttr)
    {
        GfVec2f pos;
        if (posAttr.Get(&pos))
        {
            double value[2] = { pos[0], pos[1] };
            layoutBuilder.set(handle + ".position",
                              FnKat::DoubleAttribute(value, 2, 1));
        }
    }

    // Read expansion state.
    UsdAttribute expansionStateAttr = nodeApi.GetExpansionStateAttr();
    if (expansionStateAttr)
    {
        TfToken expansionState;
        if (expansionStateAttr.Get(&expansionState))
        {
            int value = -1;
            if (expansionState == UsdUITokens->closed)
            {
                value = 0;
            }
            else if (expansionState == UsdUITokens->minimized)
            {
                value = 1;
            }
            else if (expansionState == UsdUITokens->open)
            {
                value = 2;
            }

            if (value >= 0)
            {
                layoutBuilder.set(handle + ".viewState",
                                  FnKat::IntAttribute(value));
                layoutBuilder.set(handle + ".nodeShapeAttributes.viewState",
                                  FnKat::FloatAttribute(value));
            }
        }
    }

    layoutBuilder.set(handle + ".parent", FnKat::StringAttribute("USD"));
}

static void
_FillShaderIdToRenderTarget(std::map<std::string, std::string>& output)
{
    std::vector<std::string> pluginNames;
    FnPluginManager::PluginManager::getPluginNames("RendererInfoPlugin",
                                                   pluginNames, 2);

    for (const auto& pluginName : pluginNames)
    {
        FnPluginHandle plugin = FnPluginManager::PluginManager::getPlugin(
            pluginName, "RendererInfoPlugin", 2);
        if (!plugin)
        {
            FnLogError("Cannot find renderer info plugin '" << pluginName
                                                            << "'");
            continue;
        }

        const RendererInfoPluginSuite_v2* suite =
            reinterpret_cast<const RendererInfoPluginSuite_v2*>(
                FnPluginManager::PluginManager::getPluginSuite(plugin));
        if (!suite)
        {
            FnLogError("Error getting renderer info plugin API suite.");
            continue;
        }

        FnRendererInfo::FnRendererInfoPlugin* rendererInfoPlugin =
            new FnRendererInfo::FnRendererInfoPlugin(suite);
        const char* pluginCharFilepath =
            FnPluginManager::PluginManager::getPluginPath(plugin);
        const std::string pluginFilepath =
            (pluginCharFilepath ? pluginCharFilepath : "");
        const std::string pluginDir =
            pystring::os::path::dirname(pluginFilepath);
        const std::string rootPath = pluginDir + "/..";
        rendererInfoPlugin->setPluginPath(pluginDir);
        rendererInfoPlugin->setPluginRootPath(rootPath);
        rendererInfoPlugin->setKatanaPath(FnConfig::Config::get("KATANA_ROOT"));
        rendererInfoPlugin->setTmpPath(FnConfig::Config::get("KATANA_TMPDIR"));

        std::string rendererName;
        rendererInfoPlugin->getRegisteredRendererName(rendererName);

        std::vector<std::string> shaderNames;
        const std::vector<std::string> typeTags;
        rendererInfoPlugin->getRendererObjectNames(kFnRendererObjectTypeShader,
                                                   typeTags, shaderNames);
        for (const auto& shaderName : shaderNames)
        {
            output[shaderName] = rendererName;
        }
    }
}

std::string
_GetRenderTarget(const std::string& shaderId)
{
    // A static map from shader ID to renderer target name.
    static std::map<std::string, std::string> s_shaderIdToRenderTarget;

    // If the map has not been filled, do so now; take care to do this exactly
    // once from exactly one thread (since UsdIn is multithreaded).
    static std::once_flag s_onceFlag;
    std::call_once(
        s_onceFlag,
        [&]() { _FillShaderIdToRenderTarget(s_shaderIdToRenderTarget); });

    // Other threads can only get here once the lucky thread has filled the map.
    auto it = s_shaderIdToRenderTarget.find(shaderId);
    return (it == s_shaderIdToRenderTarget.end()) ? std::string() : it->second;
}

// NOTE: the Ris codepath doesn't use the interfaceBuilder
std::string
_CreateShadingNode(
        UsdPrim shadingNode,
        double currentTime,
        FnKat::GroupBuilder& nodesBuilder,
        FnKat::GroupBuilder& interfaceBuilder,
        FnKat::GroupBuilder& layoutBuilder,
        const std::string & targetName,
        bool flatten)
{
    std::string handle = PxrUsdKatanaUtils::GenerateShadingNodeHandle(shadingNode);
    if (handle.empty()) {
        return "";
    }

    // Check if we know about this node already
    FnKat::GroupAttribute curNodes = nodesBuilder.build(
            nodesBuilder.BuildAndRetain);
    if (curNodes.getChildByName(handle).isValid()) {
        // If so, just return and don't create anything
        return handle;
    }

    // Create an empty group at the handle to prevent infinite recursion
    nodesBuilder.set(handle, FnKat::GroupBuilder().build());

    FnKat::GroupBuilder shdNodeBuilder;
    bool validData = false;
    TfToken id;

    UsdShadeShader shaderSchema = UsdShadeShader(shadingNode);
    if (shaderSchema)
    {
        validData = true;
        SdfAssetPath fileAssetPath;

        // only use the fallback OSL test if the targetName is "prman" as
        // it will issue benign but confusing errors to the shell for
        // display shaders
        if (targetName == "prman")
        {
            shaderSchema.GetIdAttr().Get(&id, currentTime);
            std::string oslIdString = id.GetString();

            if (!pystring::endswith(oslIdString, ".oso"))
            {
                oslIdString = "osl:" + oslIdString;
            }

            FnKat::StringAttribute oslIdAttr = FnKat::StringAttribute(oslIdString);
            FnAttribute::GroupAttribute shaderInfoAttr =
                        FnGeolibServices::FnAttributeFunctionUtil::run(
                                "PRManGetShaderParameterInfo", oslIdAttr);
            if (shaderInfoAttr.isValid())
                shdNodeBuilder.set("type", oslIdAttr);
            else
                shdNodeBuilder.set(
                    "type", FnKat::StringAttribute(id.GetString()));
        }
        else
        {
            shaderSchema.GetIdAttr().Get(&id, currentTime);
            shdNodeBuilder.set(
                    "type", FnKat::StringAttribute(id.GetString()));
        }
    }

    // We gather shading parameters even if shaderSchema is invalid; we need to
    // get connection attributes for the enclosing network material.
    FnKat::GroupBuilder paramsBuilder;
    FnKat::GroupBuilder connectionsBuilder;

    _GatherShadingParameters(
        shaderSchema, handle, currentTime, nodesBuilder, paramsBuilder,
        interfaceBuilder, layoutBuilder, connectionsBuilder, targetName,
        flatten);

    FnKat::GroupAttribute paramsAttr = paramsBuilder.build();
    if (paramsAttr.getNumberOfChildren() > 0) {
        shdNodeBuilder.set("parameters", paramsAttr);
    }

    FnKat::GroupAttribute connectionsAttr = connectionsBuilder.build();
    if (connectionsAttr.getNumberOfChildren() > 0)
    {
        shdNodeBuilder.set("connections", connectionsAttr);

        // We also have to write connections to layout, but as a flattened attr.
        std::vector<std::string> connections;
        connections.reserve(connectionsAttr.getNumberOfChildren());
        for (int64_t i = 0; i < connectionsAttr.getNumberOfChildren(); ++i)
        {
            FnKat::StringAttribute c = connectionsAttr.getChildByIndex(i);
            if (c.isValid())
            {
                const std::string& name = connectionsAttr.getChildName(i);
                connections.push_back(name + ':' + c.getValue());
            }
        }

        FnKat::StringAttribute layoutConns(
            connections.data(), connections.size(), 1);
        layoutBuilder.set(handle + ".connections", layoutConns);
    }

    _ReadLayoutAttrs(shadingNode, handle, layoutBuilder);

    std::string target = targetName;
    if (validData)
    {
        const std::string result = _GetRenderTarget(id.GetString());
        if (!result.empty())
        {
            target = result;
        }

        if (flatten ||
            !PxrUsdKatana_IsPrimDefFromBaseMaterial(shadingNode))
        {
            shdNodeBuilder.set("name", FnKat::StringAttribute(handle));
            shdNodeBuilder.set("srcName", FnKat::StringAttribute(handle));
            shdNodeBuilder.set("target", FnKat::StringAttribute(target));
        }
    }

    FnKat::GroupAttribute shdNodeAttr = shdNodeBuilder.build();
    nodesBuilder.set(handle, shdNodeAttr);

    // Copy node attributes to layout so NME can create the shading network.
    if (shdNodeAttr.getNumberOfChildren() > 0)
    {
        FnKat::GroupBuilder gb;
        gb.set("name", shdNodeAttr.getChildByName("name"));
        gb.set("shaderType", shdNodeAttr.getChildByName("type"));
        gb.set("target", shdNodeAttr.getChildByName("target"));

        FnKat::GroupAttribute nodeSpecificAttrs = gb.build();
        if (nodeSpecificAttrs.getNumberOfChildren() > 0)
        {
            layoutBuilder.set(
                handle + ".nodeSpecificAttributes", nodeSpecificAttrs);
        }
    }

    // Set Katana node type.
    std::string nodeType;
    if (validData)
    {
        nodeType = target + "ShadingNode";
        nodeType[0] = toupper(nodeType[0]);
    }
    else
    {
        nodeType = "NetworkMaterial";
    }

    layoutBuilder.set(
        handle + ".katanaNodeType", FnKat::StringAttribute(nodeType));

    return handle;
}

// A specialization of _CreateShadingNode() for outputting layout attributes for
// the enclosing NetworkMaterial.
static std::string
_CreateEnclosingNetworkMaterialLayout(
    const UsdPrim& materialPrim,
    double currentTime,
    FnKat::GroupBuilder& nodesBuilder,
    FnKat::GroupBuilder& interfaceBuilder,
    FnKat::GroupBuilder& layoutBuilder,
    const std::string & targetName,
    bool flatten)
{
    std::string handle = _CreateShadingNode(
        materialPrim, currentTime, nodesBuilder, interfaceBuilder,
        layoutBuilder, targetName, flatten);

    // Remove from material.nodes, as there is no accompanying shading node.
    nodesBuilder.del(handle);

    // We must put the layout attributes are at material.layout.<primname>, else
    // NME will remove them and recreate new attributes.
    const std::string& expectedName = materialPrim.GetName().GetString();
    if (handle != expectedName)
    {
        FnKat::GroupAttribute layoutAttrs = layoutBuilder.build(
            FnKat::GroupBuilder::BuildAndRetain);
        layoutAttrs = layoutAttrs.getChildByName(handle);
        layoutBuilder.del(handle);
        layoutBuilder.set(expectedName, layoutAttrs);
    }

    return handle;
}

FnKat::Attribute
_GetMaterialAttr(
        const UsdShadeMaterial& materialSchema,
        double currentTime,
        const std::string& targetName,
        const bool prmanOutputTarget,
        bool flatten)
{
    UsdPrim materialPrim = materialSchema.GetPrim();

    // TODO: we need a hasA schema
    UsdRiMaterialAPI riMaterialAPI(materialPrim);
    UsdStageWeakPtr stage = materialPrim.GetStage();

    FnKat::GroupBuilder materialBuilder;
    materialBuilder.set("style", FnKat::StringAttribute("network"));
    FnKat::GroupBuilder nodesBuilder;
    FnKat::GroupBuilder interfaceBuilder;
    FnKat::GroupBuilder layoutBuilder;
    FnKat::GroupBuilder terminalsBuilder;

    /////////////////
    // RSL SECTION
    /////////////////

    // look for surface
    UsdShadeShader surfaceShader = riMaterialAPI.GetSurface(
            /*ignoreBaseMaterial*/ not flatten);
    if (surfaceShader.GetPrim()) {
        std::string handle = _CreateShadingNode(
            surfaceShader.GetPrim(), currentTime,
            nodesBuilder, interfaceBuilder, layoutBuilder, "prman", flatten);

        // If the source shader type is an RslShader, then publish it
        // as a prmanSurface terminal. If not, fallback to the
        // prmanBxdf terminal.
        UsdRiRslShader rslShader(surfaceShader.GetPrim());
        if (rslShader) {
            terminalsBuilder.set("prmanSurface",
                                 FnKat::StringAttribute(handle));
            terminalsBuilder.set("prmanSurfacePort",
                                 FnKat::StringAttribute("out"));
        } else {
            terminalsBuilder.set("prmanBxdf",
                                 FnKat::StringAttribute(handle));
            terminalsBuilder.set("prmanBxdfPort",
                                 FnKat::StringAttribute("out"));
        }
    }

    // look for displacement
    UsdShadeShader displacementShader = riMaterialAPI.GetDisplacement(
            /*ignoreBaseMaterial*/ not flatten);
    if (displacementShader.GetPrim()) {
        std::string handle = _CreateShadingNode(
            displacementShader.GetPrim(), currentTime,
            nodesBuilder, interfaceBuilder, layoutBuilder, "prman", flatten);
        terminalsBuilder.set("prmanDisplacement",
                             FnKat::StringAttribute(handle));
    }

    /////////////////
    // RIS SECTION
    /////////////////
    // this does not exclude the rsl part

    // XXX BEGIN This code is in support of Subgraph workflows
    //           and is currently necessary to match equivalent SGG behavior

    // Look for labeled patterns - TODO: replace with UsdShade::ShadingSubgraph
    std::vector<UsdProperty> properties =
        materialPrim.GetPropertiesInNamespace("patternTerminal");
    if (properties.size()) {
        TF_FOR_ALL(propIter, properties) {
            // if (propIter->Is<UsdRelationship>()) {
            UsdRelationship rel = propIter->As<UsdRelationship>();
            if (not rel) {
                continue;
            }

            SdfPathVector targetPaths;
            rel.GetForwardedTargets(&targetPaths);
            if (targetPaths.size() == 0) {
                continue;
            }
            if (targetPaths.size() > 1) {
                FnLogWarn(
                    "Multiple targets for one output port detected on look:" <<
                    materialPrim.GetPath());
            }

            const SdfPath targetPath = targetPaths[0];
            if (not targetPath.IsPropertyPath()) {
                FnLogWarn("Pattern wants a usd property path, not a prim: "
                    << targetPath.GetString());
                continue;
            }

            SdfPath nodePath = targetPath.GetPrimPath();

            if (UsdPrim patternPrim =
                    stage->GetPrimAtPath(nodePath)) {

                std::string propertyName = targetPath.GetName();
                std::string patternPort = propertyName.substr(
                    propertyName.find(':')+1);

                std::string terminalName = rel.GetName();
                terminalName = terminalName.substr(terminalName.find(':')+1);

                std::string handle = _CreateShadingNode(
                    patternPrim, currentTime, nodesBuilder,
                    interfaceBuilder, layoutBuilder, "prman", flatten);
                terminalsBuilder.set("prmanCustom_"+terminalName,
                    FnKat::StringAttribute(handle));
                terminalsBuilder.set("prmanCustom_"+terminalName+"Port",
                    FnKat::StringAttribute(patternPort));
            }
            else {
                FnLogWarn("Pattern does not exist at "
                            << targetPath.GetString());
            }
        }
    }
    // XXX END

    // with the current implementation of ris, there are
    // no patterns that are unbound or not connected directly
    // to bxdf's.

    // generate interface for materialPrim and also any "contiguous" scopes
    // that are we encounter.
    //
    // XXX: is this behavior unique to katana or do we stick this
    // into the schema?

    std::vector<UsdShadeOutput> materialOutputs = materialSchema.GetOutputs();

    for (auto& materialOutput : materialOutputs)
    {
        if (materialOutput.HasConnectedSource())
        {
            const TfToken materialOutTerminalName =
                materialOutput.GetBaseName();
            if (TfStringStartsWith(materialOutTerminalName.GetString(), "ri:"))
            {
                // Skip since we deal with prman shaders above.
                continue;
            }

            std::string katanaTerminalName = _GetKatanaTerminalName(
                materialOutTerminalName.GetString());
            if (katanaTerminalName.empty())
            {
                continue;
            }

            UsdShadeConnectableAPI materialOutSource;
            TfToken sourceName;
            UsdShadeAttributeType sourceType;
            materialOutput.GetConnectedSource(&materialOutSource, &sourceName,
                                              &sourceType);
            UsdShadeShader shader = UsdShadeShader(materialOutSource.GetPrim());
            const SdfPath& connectedShaderPath = materialOutSource.GetPath();
            const std::string katanaTerminalPortName =
                katanaTerminalName + "Port";
            terminalsBuilder.set(
                katanaTerminalName.c_str(),
                FnKat::StringAttribute(connectedShaderPath.GetName()));
            terminalsBuilder.set(
                katanaTerminalPortName.c_str(),
                FnKat::StringAttribute(sourceName.GetString()));
        }
    }

    _CreateEnclosingNetworkMaterialLayout(
        materialPrim, currentTime, nodesBuilder, interfaceBuilder,
        layoutBuilder, targetName, flatten);

    std::stack<UsdPrim> dfs;
    dfs.push(materialPrim);
    while (!dfs.empty()) {
        UsdPrim curr = dfs.top();
        dfs.pop();

        std::string paramPrefix;
        if (curr != materialPrim) {
            if (curr.IsA<UsdShadeShader>()) {
                // XXX: Because we're using a lookDerivesFrom
                // relationship instead of a USD composition construct,
                // we'll need to create every shading node instead of
                // relying on traversing the bxdf.
                // We can remove this once the "derives" usd composition
                // works, along with partial composition
                std::string handle = _CreateShadingNode(
                    curr, currentTime, nodesBuilder, interfaceBuilder,
                    layoutBuilder, targetName, flatten);
            }

            if (!curr.IsA<UsdGeomScope>()) {
                continue;
            }

            paramPrefix = PxrUsdKatanaUtils::GenerateShadingNodeHandle(curr);
        }

        _UnrollInterfaceFromPrim(curr,
                paramPrefix,
                materialBuilder,
                interfaceBuilder);

        TF_FOR_ALL(childIter, curr.GetChildren()) {
            dfs.push(*childIter);
        }
    }

    materialBuilder.set("nodes", nodesBuilder.build());
    materialBuilder.set("terminals", terminalsBuilder.build());
    materialBuilder.set("interface", interfaceBuilder.build());
    materialBuilder.set("layout", layoutBuilder.build());
    materialBuilder.set("info.name",
                        FnKat::StringAttribute(materialPrim.GetName()));
    materialBuilder.set("info.layoutVersion", FnKat::IntAttribute(2));

    FnKat::GroupBuilder statementsBuilder;
    PxrUsdKatanaReadPrimPrmanStatements(materialPrim, currentTime,
        statementsBuilder, prmanOutputTarget);
    // Gather prman statements
    FnKat::GroupAttribute statements = statementsBuilder.build();
    if (statements.getNumberOfChildren()) {
        if (prmanOutputTarget)
        {
            materialBuilder.set("underlayAttrs.prmanStatements", statements);
        }
        materialBuilder.set("usd", statements);
    }


    FnAttribute::GroupAttribute localMaterialAttr = materialBuilder.build();

    if (flatten) {
        // check for parent, and compose with it
        // XXX:
        // Eventually, this "derivesFrom" relationship will be
        // a "derives" composition in usd, in which case we'll have to
        // rewrite this to use partial usd composition
        //
        // Note that there are additional workarounds in using the
        // "derivesFrom"/BaseMaterial relationship in the non-op SGG that
        // would need to be replicated here if the USD Material AttributeFn
        // were to use the PxrUsdIn op instead, particularly with respect to
        // the tree structure that the non-op the SGG creates
        // See _ConvertUsdMAterialPathToKatLocation in
        // katanapkg/plugin/sgg/usd/utils.cpp

        if (materialSchema.HasBaseMaterial()) {
            SdfPath baseMaterialPath = materialSchema.GetBaseMaterialPath();
            if (UsdShadeMaterial baseMaterial = UsdShadeMaterial::Get(stage, baseMaterialPath)) {
                // Make a fake context to grab parent data, and recurse on that
                FnKat::GroupAttribute parentMaterial = _GetMaterialAttr(
                    baseMaterial, currentTime, targetName,
                    prmanOutputTarget, true);
                FnAttribute::GroupBuilder flatMaterialBuilder;
                flatMaterialBuilder.update(parentMaterial);
                flatMaterialBuilder.deepUpdate(localMaterialAttr);
                return flatMaterialBuilder.build();
            }
            else {
                FnLogError(TfStringPrintf("ERROR: Expected UsdShadeMaterial at %s\n",
                        baseMaterialPath.GetText()).c_str());
            }
        }
    }
    return localMaterialAttr;
}

/* static */
void
_UnrollInterfaceFromPrim(const UsdPrim& prim,
        const std::string& paramPrefix,
        FnKat::GroupBuilder& materialBuilder,
        FnKat::GroupBuilder& interfaceBuilder)
{
    UsdStageRefPtr stage = prim.GetStage();

    // TODO: Right now, the exporter doesn't always move thing into
    // the right spot.  for example, we have "Paint_Base_Color" on
    // /PaintedMetal_Material.Paint_Base_Color
    // Which makes it so we can't use the materialSchema.GetInterfaceInputs()
    // (because /PaintedMetal_Material.Paint_Base_Color doesn't have the
    // corresponding "ri" interfaceInput connection).
    //
    // that should really be on
    // /PaintedMetal_Material/Paint_.Base_Color  which does have that
    // connection.
    //
    UsdShadeMaterial materialSchema(prim);
    std::vector<UsdShadeInput> interfaceInputs =
        materialSchema.GetInterfaceInputs();
    UsdShadeNodeGraph::InterfaceInputConsumersMap interfaceInputConsumers =
        materialSchema.ComputeInterfaceInputConsumersMap(
            /*computeTransitiveMapping*/ true);

    TF_FOR_ALL(interfaceInputIter, interfaceInputs) {
        UsdShadeInput interfaceInput = *interfaceInputIter;

        // Skip invalid interface inputs.
        if (!interfaceInput.GetAttr()) {
            continue;
        }

        const TfToken& paramName = interfaceInput.GetBaseName();
        const std::string renamedParam = paramPrefix + paramName.GetString();

        // handle parameters with values
        VtValue attrVal;
        if (interfaceInput.GetAttr().Get(&attrVal) && !attrVal.IsEmpty()) {
            materialBuilder.set(
                    TfStringPrintf("parameters.%s", renamedParam.c_str()),
                    PxrUsdKatanaUtils::ConvertVtValueToKatAttr(attrVal));
        }

        if (interfaceInputConsumers.count(interfaceInput) == 0) {
            continue;
        }

        const std::vector<UsdShadeInput> &consumers =
            interfaceInputConsumers.at(interfaceInput);

        for (const UsdShadeInput &consumer : consumers) {
            UsdPrim consumerPrim = consumer.GetPrim();

            TfToken inputName = consumer.GetBaseName();

            std::string handle = PxrUsdKatanaUtils::GenerateShadingNodeHandle(
                consumerPrim);

            std::string srcKey = renamedParam + ".src";

            std::string srcVal = TfStringPrintf(
                    "%s.%s",
                    handle.c_str(),
                    inputName.GetText());

            interfaceBuilder.set(
                    srcKey.c_str(),
                    FnKat::StringAttribute(srcVal),
                    true);
        }

        // USD's group delimiter is :, whereas Katana's is .
        std::string page = TfStringReplace(
                interfaceInput.GetDisplayGroup(), ":", ".");
        if (!page.empty()) {
            std::string pageKey = renamedParam + ".hints.page";
            interfaceBuilder.set(pageKey, FnKat::StringAttribute(page), true);
        }

        std::string doc = interfaceInput.GetDocumentation();
        if (!doc.empty()) {
            std::string docKey = renamedParam + ".hints.help";

            doc = TfStringReplace(doc, "'", "\"");
            doc = TfStringReplace(doc, "\n", "\\n");

            interfaceBuilder.set(docKey, FnKat::StringAttribute(doc), true);
        }
    }
}


PXR_NAMESPACE_CLOSE_SCOPE

