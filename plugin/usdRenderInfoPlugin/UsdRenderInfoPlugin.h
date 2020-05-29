// Copyright (c) 2018 The Foundry Visionmongers Ltd. All Rights Reserved.

#ifndef KATANA_PLUGINS_UsdRenderInfoPlugin_H_
#define KATANA_PLUGINS_UsdRenderInfoPlugin_H_

#include <string>
#include <vector>

#include <FnAttribute/FnAttribute.h>
#include <FnRendererInfo/plugin/RendererInfoBase.h>

#include "pxr/pxr.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderProperty.h"

PXR_NAMESPACE_OPEN_SCOPE

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

private:
    void fillShaderTagsFromUsdShaderProperty(
        std::vector<std::string>& shaderTags,
        SdrShaderPropertyConstPtr shaderProperty) const;

    // Gets a shader name that does not end in a numerical value, such that
    // it does not cause issues when dropping multiple nodes down in an NMC
    // e.g `UsdPrimvarReader_float2` becomes `UsdPrimvarReader_float2_`
    std::string getSafeShaderName(const std::string& shaderName) const;

    // Reverse the operation done by getSafeShaderName, returns the name
    // which USD uses internally.
    std::string getShaderNamefromSafeName(
        const std::string& safeShaderName) const;

    SdrRegistry& m_sdrRegistry;

    static const char s_safeChar = '_';
};

PXR_NAMESPACE_CLOSE_SCOPE
#endif  // KATANA_PLUGINS_UsdRenderInfoPlugin_H_
