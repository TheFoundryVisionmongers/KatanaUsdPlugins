#include "gtest/gtest.h"

#include <iostream>

#include "pxr/pxr.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usdLux/boundableLightBase.h"
#include "pxr/usd/usdLux/lightAPI.h"
#include "pxr/usd/usdShade/shaderDefParser.h"
#include "pxr/usd/usdShade/shaderDefUtils.h"

#include "usdKatana/attrMap.h"
#include "usdKatana/readLightFilter.h"
#include "usdKatana/usdInArgs.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

PXR_NAMESPACE_OPEN_SCOPE

class ReadLightFilterTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        _sdrRegistry = &SdrRegistry::GetInstance();
        const std::string assetPath = "test/shaders/shaderDefs.usda";
        UsdStageRefPtr stage = UsdStage::Open(assetPath);
        UsdShadeShader shaderDef = UsdShadeShader::Get(stage, SdfPath("/FnTestRectLight"));

        auto results = UsdShadeShaderDefUtils::GetNodeDiscoveryResults(
            shaderDef, stage->GetRootLayer()->GetRealPath());
        for (auto& result : results)
        {
            _sdrRegistry->AddDiscoveryResult(result);
        }
    }

    static SdrRegistry* _sdrRegistry;
};
SdrRegistry* ReadLightFilterTest::_sdrRegistry = nullptr;

namespace ReadLightFilterTests
{
TEST_F(ReadLightFilterTest, GetTestSchemaAsNode)
{
    auto node = _sdrRegistry->GetShaderNodeByName("FnTestRectLight");
    ASSERT_TRUE(static_cast<bool>(node));
}

TEST_F(ReadLightFilterTest, ReadType)
{
    UsdStageRefPtr stage = UsdStage::Open("test/lightfilter1.usda");
    UsdPrim lightFilterPrim = stage->GetPrimAtPath(SdfPath("/root/lgt/gaffer/UsdLightFilter"));
    ASSERT_TRUE(static_cast<bool>(lightFilterPrim));

    ArgsBuilder usdInArgsBuilder;
    usdInArgsBuilder.stage = stage;
    usdInArgsBuilder.rootLocation = "/root";
    usdInArgsBuilder.isolatePath = "";
    usdInArgsBuilder.sessionLocation = "";
    auto usdInArgs = usdInArgsBuilder.build();

    UsdKatanaUsdInPrivateData privateData(lightFilterPrim, usdInArgs);
    UsdKatanaAttrMap attrs;
    UsdKatanaReadLightFilter(lightFilterPrim, privateData, attrs);
    FnAttribute::GroupAttribute result = attrs.build();
    FnAttribute::StringAttribute typeAttr = result.getChildByName("type");
    ASSERT_TRUE(typeAttr.isValid());
    ASSERT_EQ(typeAttr.getValue("", true), "light filter");
}

}  // namespace ReadLightFilterTests
PXR_NAMESPACE_CLOSE_SCOPE
