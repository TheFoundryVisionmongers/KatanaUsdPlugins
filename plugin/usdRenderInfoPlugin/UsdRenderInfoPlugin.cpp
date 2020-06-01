// Copyright (c) 2018 The Foundry Visionmongers Ltd. All Rights Reserved.

#include "UsdRenderInfoPlugin.h"
#include "usdKatana/utils.h"

#include <string>
#include <vector>

#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/usd/usdShade/connectableAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/utils.h"

namespace
{
bool startsWith(const std::string& str, const std::string& startsWithStr)
{
    const size_t startsWithLength = startsWithStr.length();
    const size_t strLength = str.length();
    if (startsWithLength > strLength)
    {
        return false;
    }
    else if (startsWithLength == strLength)
    {
        return startsWithStr == str;
    }
    else
    {
        return str.compare(0, startsWithLength, startsWithStr) == 0;
    }
}
}  // namespace

PXR_NAMESPACE_USING_DIRECTIVE

const std::string GetParameterKey(const std::string& shader,
                                  const std::string& input)
{
    return shader + "." + input;
}

void ApplyCustomFloatHints(
    std::string shader, std::string input, FnAttribute::GroupBuilder& gb)
{
    const auto inputKey = GetParameterKey(shader, input);
    if (inputKey == "UsdPreviewSurface.ior")
        gb.set("slidermax", FnAttribute::FloatAttribute(5.0f));
}

void ApplyCustomStringHints(
    std::string shader, std::string input, FnAttribute::GroupBuilder& gb)
{
    const auto inputKey = GetParameterKey(shader, input);
    if (inputKey == "UsdUVTexture.wrapS" || inputKey == "UsdUVTexture.wrapT")
        gb.set("options", FnAttribute::StringAttribute(
            {"black", "clamp", "mirror", "repeat", "useMetadata"}));
}

std::string GetWidgetTypeFromShaderInputProperty(
    std::string shaderName,
    SdrShaderPropertyConstPtr shaderInput)
{
    if (!shaderInput)
    {
        return std::string();
    }

    static const std::map<std::string, std::string> widgetTypes = {
        {"std::string", "string"},
        {"SdfAssetPath", "assetIdInput"},
        {"float", "number"},
        {"int", "number"}
    };

    const auto inputName = shaderInput->GetImplementationName();
    const auto key = GetParameterKey(shaderName, inputName);

    // check for a custom widget definition for this particular input
    if (key == "UsdPreviewSurface.useSpecularWorkflow")
        {
            return "checkBox";
        }
    else if (key == "UsdUVTexture.wrapS" || key == "UsdUVTexture.wrapT")
    {
        return "popup";
    }

    // color needs to be handled specifically
    if (shaderInput->GetType().GetString() == "color")
    {
        return "color";
    }

    SdfTypeIndicator sdfTypePair = shaderInput->GetTypeAsSdfType();
    const std::string shaderInputType = sdfTypePair.first.GetCPPTypeName();
    auto element = widgetTypes.find(shaderInputType);
    if (element != widgetTypes.end())
    {
        return element->second;
    }

    return std::string();
}

UsdRenderInfoPlugin::UsdRenderInfoPlugin()
    : m_sdrRegistry(SdrRegistry::GetInstance())
{
}

FnKat::RendererInfo::RendererInfoBase* UsdRenderInfoPlugin::create()
{
    return new UsdRenderInfoPlugin();
}

std::string UsdRenderInfoPlugin::getRegisteredRendererName() const
{
    return "usd";
}

std::string UsdRenderInfoPlugin::getRegisteredRendererVersion() const
{
    return "1.0";
}

bool UsdRenderInfoPlugin::isNodeTypeSupported(const std::string& nodeType) const
{
    if (nodeType == "ShadingNode")
    {
        return true;
    }
    return false;
}

void UsdRenderInfoPlugin::fillRendererObjectTypes(
    std::vector<std::string>& renderObjectTypes,
    const std::string& type) const
{
    if (type == kFnRendererObjectTypeShader)
    {
        renderObjectTypes.push_back("displacement");
        renderObjectTypes.push_back("surface");
    }
}

void UsdRenderInfoPlugin::fillRendererShaderTypeTags(
    std::vector<std::string>& shaderTypeTags,
    const std::string& shaderType) const
{
    // For both displacement and surface we want the terminal tag to be
    // the same, such that we can only plug surface into surface terminals
    // and displacement into displacement.
    shaderTypeTags.push_back(shaderType);
}

void UsdRenderInfoPlugin::fillRendererObjectNames(
    std::vector<std::string>& rendererObjectNames,
    const std::string& type,
    const std::vector<std::string>& typeTags) const
{
    if (type == kFnRendererObjectTypeShader)
    {
        std::vector<std::string> nodeNames = m_sdrRegistry.GetNodeNames();
        for (const auto& name : nodeNames)
        {
            if (startsWith(name, "Usd"))
            {
                std::string shaderName = getSafeShaderName(name);
                rendererObjectNames.push_back(shaderName);
            }
        }
    }
}
void UsdRenderInfoPlugin::fillShaderInputNames(
    std::vector<std::string>& shaderInputNames,
    const std::string& shaderName) const
{
    const std::string name = getShaderNamefromSafeName(shaderName);
    SdrShaderNodeConstPtr shader = m_sdrRegistry.GetShaderNodeByName(name);
    if (!shader)
    {
        return;
    }
    NdrTokenVec shaderInputs = shader->GetInputNames();
    shaderInputNames.reserve(shaderInputs.size());
    for (const auto& token : shaderInputs)
    {
        shaderInputNames.push_back(token.GetString());
    }
}

void UsdRenderInfoPlugin::fillShaderInputTags(
    std::vector<std::string>& shaderInputTags,
    const std::string& shaderName,
    const std::string& inputName) const
{
    const std::string name = getShaderNamefromSafeName(shaderName);
    SdrShaderNodeConstPtr shader = m_sdrRegistry.GetShaderNodeByName(name);
    if (!shader)
    {
        return;
    }

    SdrShaderPropertyConstPtr shaderProperty =
        shader->GetShaderInput(TfToken(inputName));
    fillShaderTagsFromUsdShaderProperty(shaderInputTags, shaderProperty);
}

void UsdRenderInfoPlugin::fillShaderTagsFromUsdShaderProperty(
    std::vector<std::string>& shaderTags,
    SdrShaderPropertyConstPtr shaderProperty) const
{
    if (!shaderProperty)
    {
        return;
    }
    const bool isOutput = shaderProperty->IsOutput();
    const std::string shaderType = shaderProperty->GetType().GetString();

    // From the Docs: Two scenarios can result: an exact mapping from property
    // type to Sdf type, and an inexact mapping. In the first scenario, the
    // first element in the pair will be the cleanly-mapped Sdf type, and the
    // second element, a TfToken, will be empty. In the second scenario, the
    // Sdf type will be set to Token to indicate an unclean mapping, and the
    // second element will be set to the original type returned by GetType().
    // From USD code: (So we know what an SdfTypeIndicator is in the future!)
    // typedef std::pair<SdfValueTypeName, TfToken> SdfTypeIndicator;
    SdfTypeIndicator sdfTypePair = shaderProperty->GetTypeAsSdfType();
    std::string sdfType;
    if (sdfTypePair.second.IsEmpty())
    {
        // Scenario 1
        sdfType = sdfTypePair.first.GetType().GetTypeName();
    }
    else
    {
        // Scenario 2
        // In Scenario 2 there is no mapping, which if this is a terminal
        // we would like to use the name of the terminal as the tag
        // such that we cant connect surface to displacement for example.
        sdfType = sdfTypePair.second.GetString();
        if (sdfType == "terminal")
        {
            sdfType = shaderProperty->GetName();
        }
    }
    
    if (sdfType != shaderType)
    {
        if (isOutput)
        {
            shaderTags.push_back(std::move(shaderType));
            if (sdfType != shaderType)
            {
                shaderTags.push_back(std::move(sdfType));
            }
        }
        else
        {
            // an output tag must match ALL input tag expressions, therefore 
            // we cannot add these as multiple entries, we must build a single
            // expression.
            shaderTags.push_back(shaderType + " or " + sdfType);
        }
    }
    else
    {
        shaderTags.push_back(shaderType);
    }
}

void UsdRenderInfoPlugin::fillShaderOutputNames(
    std::vector<std::string>& shaderOutputNames,
    const std::string& shaderName) const
{
    const std::string name = getShaderNamefromSafeName(shaderName);
    SdrShaderNodeConstPtr shader = m_sdrRegistry.GetShaderNodeByName(name);
    if (!shader)
    {
        return;
    }
    NdrTokenVec shaderOutputs = shader->GetOutputNames();
    shaderOutputNames.reserve(shaderOutputs.size());
    for (const auto& token : shaderOutputs)
    {
        shaderOutputNames.push_back(token.GetString());
    }
}

void UsdRenderInfoPlugin::fillShaderOutputTags(
    std::vector<std::string>& shaderOutputTags,
    const std::string& shaderName,
    const std::string& outputName) const
{
    const std::string name = getShaderNamefromSafeName(shaderName);
    SdrShaderNodeConstPtr shader = m_sdrRegistry.GetShaderNodeByName(name);
    if (!shader)
    {
        return;
    }

    SdrShaderPropertyConstPtr shaderProperty =
        shader->GetShaderOutput(TfToken(outputName));

    // Some special logic for r, g, b, rgb, and rgba outputs which should
    // be able to plug into colors. Ensure if the outputName is rgb or rgba,
    // color is the first tag for the output type. This determines the color of
    // the port!
    if (outputName == "rgb" || outputName == "rgba")
    {
        shaderOutputTags.push_back("color");
    }
    fillShaderTagsFromUsdShaderProperty(shaderOutputTags, shaderProperty);

    // Some special logic for r, g, b, rgb, and rgba outputs which should
    // be able to plug into colors.
    if (outputName == "r" || outputName == "g" || outputName == "b")
    {
        shaderOutputTags.push_back("color");
    }
}

bool UsdRenderInfoPlugin::buildRendererObjectInfo(
    FnAttribute::GroupBuilder& rendererObjectInfo,
    const std::string& name,
    const std::string& type,
    const FnAttribute::GroupAttribute inputAttr) const
{
    std::string shaderName = getShaderNamefromSafeName(name);
    if (type == kFnRendererObjectTypeShader)
    {
        std::set<std::string> typeTags;
        typeTags.insert("Shader");
        FnKat::Attribute containerHintsAttr;
        configureBasicRenderObjectInfo(
            rendererObjectInfo, type,
            std::vector<std::string>(typeTags.begin(), typeTags.end()),
            shaderName, shaderName, kFnRendererObjectValueTypeUnknown,
            containerHintsAttr);

        SdrShaderNodeConstPtr shader =
            m_sdrRegistry.GetShaderNodeByName(shaderName);
        if (!shader)
        {
            return false;
        }
        const NdrTokenVec inputNames = shader->GetInputNames();
        for (const TfToken& inputName : inputNames)
        {
            SdrShaderPropertyConstPtr shaderInput =
                shader->GetShaderInput(inputName);
            if (!shaderInput)
            {
                return false;
            }
            const VtValue& defaultValue = shaderInput->GetDefaultValue();
            auto defaultAttr =
                PxrUsdKatanaUtils::ConvertVtValueToKatAttr(defaultValue);
            EnumPairVector enumValues;
            FnAttribute::GroupBuilder hintsGroupBuilder;

            const std::string shaderName = shader->GetImplementationName();
            const std::string widgetType = GetWidgetTypeFromShaderInputProperty(
                shaderName, shaderInput);
            if (!widgetType.empty())
            {
                hintsGroupBuilder.set(
                    "widget", FnAttribute::StringAttribute(widgetType));
                if (widgetType == "number")  // candidate for a slider
                {
                    hintsGroupBuilder.set(
                        "slider", FnAttribute::FloatAttribute(1.0f));
                    hintsGroupBuilder.set(
                        "min", FnAttribute::FloatAttribute(0.0f));
                    hintsGroupBuilder.set(
                        "max", FnAttribute::FloatAttribute(1.0f));
                    hintsGroupBuilder.set(
                        "slidermin", FnAttribute::FloatAttribute(0.0f));
                    hintsGroupBuilder.set(
                        "slidermax", FnAttribute::FloatAttribute(1.0f));
                }
            }

            // add any addtional custom hints
            ApplyCustomFloatHints(shaderName, inputName, hintsGroupBuilder);
            ApplyCustomStringHints(shaderName, inputName, hintsGroupBuilder);

            addRenderObjectParam(rendererObjectInfo, std::string(inputName),
                                 kFnRendererObjectValueTypeUnknown, 0,
                                 defaultAttr, hintsGroupBuilder.build(),
                                 enumValues);
        }
        return true;
    }
    return false;
}

std::string UsdRenderInfoPlugin::getSafeShaderName(
    const std::string& shaderName) const
{
    if (isdigit(shaderName.back()))
    {
        return shaderName + s_safeChar;
    }
    else
    {
        return shaderName;
    }
}

std::string UsdRenderInfoPlugin::getShaderNamefromSafeName(
    const std::string& safeShaderName) const
{
    std::string shaderName = safeShaderName;
    if (safeShaderName.back() == s_safeChar)
    {
        shaderName.pop_back();
    }
    return shaderName;
}
