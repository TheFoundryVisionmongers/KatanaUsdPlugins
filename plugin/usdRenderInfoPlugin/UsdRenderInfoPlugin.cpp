// Copyright (c) 2018 The Foundry Visionmongers Ltd. All Rights Reserved.

#include "UsdRenderInfoPlugin.h"
#include "usdKatana/utils.h"

#include <string>
#include <vector>

#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/ndr/registry.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/usd/usdShade/connectableAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/utils.h"

#include "FnGeolibServices/FnArgsFile.h"

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

static inline constexpr auto hash(char const* s, uint64_t h = 0) -> uint64_t
{
    return !*s ? h : hash(s + 1, 1ULL * (h ^ *s) * 33ULL);
}

/// Hash a string.
static inline uint64_t hash(std::string const& str)
{
    return hash(str.c_str());
}

int GetParameterType(const FnAttribute::StringAttribute& attr)
{
    int type = kFnRendererObjectValueTypeUnknown;
    if (!attr.isValid())
        return type;

    const std::string typeValue = attr.getValue("", false);
    if (hash(typeValue) == hash("string"))
        type = kFnRendererObjectValueTypeString;
    else if (hash(typeValue) == hash("color"))
        type = kFnRendererObjectValueTypeColor3;
    else if (hash(typeValue) == hash("filename"))
        type = kFnRendererObjectValueTypeString;
    else if (hash(typeValue) == hash("boolean"))
        type = kFnRendererObjectValueTypeBoolean;
    else if (hash(typeValue) == hash("mapper"))
        type = kFnRendererObjectValueTypeFloat;
    else if (hash(typeValue )== hash("popup"))
        type = kFnRendererObjectValueTypeString;
    else if (hash(typeValue) == hash("vector2"))
        type = kFnRendererObjectValueTypeVector2;
    else if (hash(typeValue) == hash("vector3"))
        type = kFnRendererObjectValueTypeVector3;
    else if (hash(typeValue) == hash("vector4"))
        type = kFnRendererObjectValueTypeVector4;

    return type;
}
}  // namespace

PXR_NAMESPACE_USING_DIRECTIVE

const std::string GetParameterKey(const std::string& shader,
                                  const std::string& input)
{
    return shader + "." + input;
}

void ApplyCustomFloatHints(const std::string& shader,
                           const std::string& input,
                           FnAttribute::GroupBuilder& gb)
{
    const auto inputKey = GetParameterKey(shader, input);
    if (inputKey == "UsdPreviewSurface.ior")
        gb.set("slidermax", FnAttribute::FloatAttribute(5.0f));
}

void ApplyCustomStringHints(const std::string& shader,
                            const std::string& input,
                            FnAttribute::GroupBuilder& gb)
{
    const auto inputKey = GetParameterKey(shader, input);
    if (inputKey == "UsdUVTexture.wrapS" || inputKey == "UsdUVTexture.wrapT")
        gb.set("options",
               FnAttribute::StringAttribute(
                   {"black", "clamp", "mirror", "repeat", "useMetadata"}));
}

std::string GetWidgetTypeFromShaderInputProperty(
    const std::string& shaderName,
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
        {"int", "number"}};

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
        renderObjectTypes.push_back("light");
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
    if (type != kFnRendererObjectTypeShader)
    {
        return;
    }

    const LightEntriesMap lightEntries = getLightEntries();
    // A lambda to fill rendererObjectNames with lights
    auto fillWithLights = [&]() {
        for (const auto& luxLight : lightEntries)
        {
            rendererObjectNames.push_back(luxLight.first);
        }
    };

    std::vector<std::string> nodeNames = m_sdrRegistry.GetNodeNames();
    // A lambda to fill rendererObjectNames with surface shaders
    auto fillWithSurface = [&]() {
        for (const auto& name : nodeNames)
        {
            if (startsWith(name, "Usd"))
            {
                rendererObjectNames.push_back(name);
            }
        }
    };

    if (typeTags.empty())
    {
        fillWithLights();
        fillWithSurface();
        return;
    }

    for (const auto& typeTag : typeTags)
    {
        if (typeTag == "light")
        {
            fillWithLights();
        }
        else if (typeTag == "surface")
        {
            fillWithSurface();
        }
    }
}

void UsdRenderInfoPlugin::fillShaderInputNames(
    std::vector<std::string>& shaderInputNames,
    const std::string& shaderName) const
{
    SdrShaderNodeConstPtr shader =
        m_sdrRegistry.GetShaderNodeByName(shaderName);
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
    SdrShaderNodeConstPtr shader =
        m_sdrRegistry.GetShaderNodeByName(shaderName);
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
    SdrShaderNodeConstPtr shader =
        m_sdrRegistry.GetShaderNodeByName(shaderName);
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
    SdrShaderNodeConstPtr shader =
        m_sdrRegistry.GetShaderNodeByName(shaderName);
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
    if (type == kFnRendererObjectTypeShader)
    {
        std::set<std::string> typeTags;
        typeTags.insert("Shader");
        FnKat::Attribute containerHintsAttr;
        configureBasicRenderObjectInfo(
            rendererObjectInfo, type,
            std::vector<std::string>(typeTags.begin(), typeTags.end()), name,
            name, kFnRendererObjectValueTypeUnknown, containerHintsAttr);

        SdrShaderNodeConstPtr shader = m_sdrRegistry.GetShaderNodeByName(name);
        if (shader)
        {
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
                FnKat::Attribute defaultAttr =
                    PxrUsdKatanaUtils::ConvertVtValueToKatAttr(defaultValue);
                EnumPairVector enumValues;
                FnAttribute::GroupBuilder hintsGroupBuilder;

                const std::string widgetType =
                    GetWidgetTypeFromShaderInputProperty(name, shaderInput);
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
                ApplyCustomFloatHints(name, inputName, hintsGroupBuilder);
                ApplyCustomStringHints(name, inputName, hintsGroupBuilder);

                addRenderObjectParam(rendererObjectInfo, std::string(inputName),
                                     kFnRendererObjectValueTypeUnknown, 0,
                                     defaultAttr, hintsGroupBuilder.build(),
                                     enumValues);
            }
            return true;
        }
        else
        {
            LightEntriesMap lights = getLightEntries();
            const std::string filePath = lights[name].filePath;
            return parseARGS(filePath, name, rendererObjectInfo);
        }
        return false;
    }
    return false;
}

bool UsdRenderInfoPlugin::parseARGS(const std::string& location,
                                    const std::string& name,
                                    FnAttribute::GroupBuilder& gb) const
{
    std::string argsPath = TfAbsPath(location + "/" + name + ".args");
    if (!TfPathExists(argsPath))
        return false;

    FnAttribute::GroupAttribute result =
        FnGeolibServices::FnArgsFile::parseArgsFile(argsPath);
    if (!result.isValid())
        return false;

    FnAttribute::GroupAttribute paramsGroup = result.getChildByName("params");
    if (!paramsGroup.isValid())
        return false;

    // Loop through all params listed in ARGS file.
    for (int64_t i = 0; i < paramsGroup.getNumberOfChildren(); ++i)
    {
        FnAttribute::Attribute defaultAttr;
        std::string paramName = paramsGroup.getChildName(i);
        FnAttribute::GroupAttribute attr = paramsGroup.getChildByIndex(i);
        FnAttribute::GroupAttribute hintsAttr = attr.getChildByName("hints");
        if (hintsAttr.isValid())
        {
            int type = GetParameterType(hintsAttr.getChildByName("type"));
            if (type == kFnRendererObjectValueTypeUnknown)
            {
                // No type attribute in the args file determine by widget.
                FnAttribute::StringAttribute widgetAttr;
                widgetAttr = hintsAttr.getChildByName("widget");
                type = GetParameterType(widgetAttr);
            }

            defaultAttr = hintsAttr.getChildByName("default");
            if (!defaultAttr.isValid())
                defaultAttr = FnAttribute::StringAttribute("");

            std::string defaultValue =
                ((FnAttribute::StringAttribute)defaultAttr).getValue("", false);

            std::vector<std::string> tokens = TfStringTokenize(defaultValue);
            float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (size_t index = 0; index < tokens.size(); ++index)
            {
                double d = TfStringToDouble(tokens[index]);
                values[index] = static_cast<float>(d);
            }

            int64_t arraySize =
                (type != kFnRendererObjectValueTypeUnknown) ? tokens.size() : 1;

            if (kFnRendererObjectValueTypeString != type)
            {
                defaultAttr = FnAttribute::FloatAttribute(&values[0], arraySize,
                                                          arraySize);
            }

            EnumPairVector enums;
            addRenderObjectParam(gb, paramName, type, tokens.size(),
                                 defaultAttr, hintsAttr, enums);
        }
    }
    return true;
}

LightEntriesMap UsdRenderInfoPlugin::getLightEntries() const
{
    LightEntriesMap lights;
    const char* const katanaRootCStr = getenv("KATANA_ROOT");
    if (katanaRootCStr == nullptr)
    {
        std::cerr << "Couldn't get KATANA_ROOT environment variable."
                  << std::endl;
        return lights;
    }
    const std::string path = std::string(katanaRootCStr) +
                             "/plugins/Resources/Core/Shaders/USD/Light";
    std::string error;
    std::vector<std::string> filenames;

    if (TfReadDir(path, nullptr, &filenames, nullptr, &error))
    {
        for (auto& filename : filenames)
        {
            LightEntry light;
            light.filePath = path;
            const std::string name = filename.substr(0, filename.rfind("."));
            lights[name] = std::move(light);
        }
    }
    else
    {
        std::cerr << error << ": " << path << std::endl;
    }

    return lights;
}
