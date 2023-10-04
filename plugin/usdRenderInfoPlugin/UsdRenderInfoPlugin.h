// Copyright (c) 2020 The Foundry Visionmongers Ltd.
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

#ifndef KATANA_PLUGINS_UsdRenderInfoPlugin_H_
#define KATANA_PLUGINS_UsdRenderInfoPlugin_H_

#include <string>
#include <vector>

#include <FnAttribute/FnAttribute.h>
#include <FnRendererInfo/plugin/RendererInfoBase.h>

#include <pxr/pxr.h>
#include <pxr/usd/ndr/registry.h>
#include <pxr/usd/sdr/registry.h>
#include <pxr/usd/sdr/shaderProperty.h>

PXR_NAMESPACE_OPEN_SCOPE

struct LightEntry
{
    std::string filePath;
};

using LightEntriesMap = std::unordered_map<std::string, LightEntry>;
/**
 * \brief This plug-in registers and defines the <b>usd</b>
 * render info plug-in.
 *
 */
class UsdRenderInfoPlugin : public FnKat::RendererInfo::RendererInfoBase
{
public:
    /** \brief Constructor. */
    UsdRenderInfoPlugin();

    /** \brief Destructor. */
    ~UsdRenderInfoPlugin() override {}

    static FnKat::RendererInfo::RendererInfoBase* create();

    static void flush() {}

    /* RendererInfoBase Methods */

    void configureBatchRenderMethod(
        FnKat::RendererInfo::DiskRenderMethod& batchRenderMethod) const override
    {
    }

    void fillRenderMethods(std::vector<FnKat::RendererInfo::RenderMethod*>&
                               renderMethods) const override
    {
    }

    void fillRendererObjectTypes(std::vector<std::string>& renderObjectTypes,
                                 const std::string& type) const override;

    void fillRendererShaderTypeTags(
        std::vector<std::string>& shaderTypeTags,
        const std::string& shaderType) const override;

    void fillRendererObjectNames(
        std::vector<std::string>& rendererObjectNames,
        const std::string& type,
        const std::vector<std::string>& typeTags) const override;

    std::string getRegisteredRendererName() const override;

    std::string getRegisteredRendererVersion() const override;

    bool buildRendererObjectInfo(
        FnAttribute::GroupBuilder& rendererObjectInfo,
        const std::string& name,
        const std::string& type,
        const FnAttribute::GroupAttribute inputAttr) const override;

    void fillShaderInputNames(std::vector<std::string>& shaderInputNames,
                              const std::string& shaderName) const;

    void fillShaderInputTags(std::vector<std::string>& shaderInputTags,
                             const std::string& shaderName,
                             const std::string& inputName) const;

    void fillShaderOutputNames(std::vector<std::string>& shaderOutputNames,
                               const std::string& shaderName) const;

    void fillShaderOutputTags(std::vector<std::string>& shaderOutputTags,
                              const std::string& shaderName,
                              const std::string& outputName) const;
    bool isNodeTypeSupported(const std::string& nodeType) const;

private:
    void fillShaderTagsFromUsdShaderProperty(
        std::vector<std::string>& shaderTags,
        SdrShaderPropertyConstPtr shaderProperty) const;

    bool parseARGS(const std::string& location,
                   const std::string& name,
                   FnAttribute::GroupBuilder& gb) const;

    LightEntriesMap getLightEntries() const;

    SdrRegistry& m_sdrRegistry;
};

PXR_NAMESPACE_CLOSE_SCOPE
#endif  // KATANA_PLUGINS_UsdRenderInfoPlugin_H_
