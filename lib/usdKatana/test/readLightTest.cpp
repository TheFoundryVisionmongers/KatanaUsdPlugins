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
#include "usdKatana/readLight.h"
#include "usdKatana/usdInArgs.h"
#include "usdKatana/usdInPrivateData.h"
#include "usdKatana/utils.h"

PXR_NAMESPACE_OPEN_SCOPE

class ReadLightTest : public ::testing::Test
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
SdrRegistry* ReadLightTest::_sdrRegistry = nullptr;

namespace ReadLightTests
{
TEST_F(ReadLightTest, GetTestSchemaAsNode)
{
    auto node = _sdrRegistry->GetShaderNodeByName("FnTestRectLight");
    ASSERT_TRUE(static_cast<bool>(node));
}

TEST_F(ReadLightTest, ReadTypedRectLight)
{
    UsdStageRefPtr stage = UsdStage::Open("test/light1.usda");
    UsdPrim lightPrim = stage->GetPrimAtPath(SdfPath("/root/lgt/gaffer/UsdRectLight"));
    ASSERT_TRUE(static_cast<bool>(lightPrim));

    ArgsBuilder usdInArgsBuilder;
    usdInArgsBuilder.stage = stage;
    usdInArgsBuilder.rootLocation = "/root";
    usdInArgsBuilder.isolatePath = "";
    usdInArgsBuilder.sessionLocation = "";
    auto usdInArgs = usdInArgsBuilder.build();

    UsdKatanaUsdInPrivateData privateData(lightPrim, usdInArgs);
    UsdKatanaAttrMap attrs;
    UsdKatanaReadLight(lightPrim, privateData, attrs);
    FnAttribute::GroupAttribute result = attrs.build();

    FnAttribute::StringAttribute typeAttr = result.getChildByName("type");
    ASSERT_TRUE(typeAttr.isValid());
    ASSERT_EQ(typeAttr.getValue("", true), "light");

    // Gather and check the general light attributes.
    FnAttribute::GroupAttribute materialAttrs = result.getChildByName("material");
    ASSERT_TRUE(materialAttrs.isValid());
    FnAttribute::StringAttribute lightShaderName = materialAttrs.getChildByName("usdLightShader");
    ASSERT_TRUE(lightShaderName.isValid());
    ASSERT_EQ(lightShaderName.getValue("", false), "UsdLuxRectLight");
    FnAttribute::GroupAttribute lightParamAttrs = materialAttrs.getChildByName("usdLightParams");
    ASSERT_TRUE(lightParamAttrs.isValid());

    // Check light intensity.
    FnAttribute::FloatAttribute intensityAttr = lightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(intensityAttr.isValid());
    ASSERT_EQ(intensityAttr.getValue(0.0, false), 40.0);

    // Check light color.
    FnAttribute::FloatAttribute colorAttr = lightParamAttrs.getChildByName("color");
    ASSERT_TRUE(colorAttr.isValid());
    auto colorValues = colorAttr.getNearestSample(0.0f);
    ASSERT_EQ(colorValues[0], 0.1f);
    ASSERT_EQ(colorValues[1], 0.2f);
    ASSERT_EQ(colorValues[2], 0.3f);

    // Check shadow enable.
    FnAttribute::IntAttribute shadowEnableAttr = lightParamAttrs.getChildByName("shadowEnable");
    ASSERT_TRUE(shadowEnableAttr.isValid());
    ASSERT_EQ(shadowEnableAttr.getValue(1, false), 0);

    // Check shadow falloff gamma.
    FnAttribute::FloatAttribute shadowFalloffGammaAttr =
        lightParamAttrs.getChildByName("shadowFalloffGamma");
    ASSERT_TRUE(shadowFalloffGammaAttr.isValid());
    ASSERT_EQ(shadowFalloffGammaAttr.getValue(0.0, true), 223.0f);

    // Check texture file path.
    FnAttribute::StringAttribute textureFileAttr = lightParamAttrs.getChildByName("textureFile");
    ASSERT_TRUE(textureFileAttr.isValid());
    ASSERT_EQ(textureFileAttr.getValue("", true), "C:/path/to/image/image_plane.tex");

    // Check width.
    FnAttribute::FloatAttribute widthAttr = lightParamAttrs.getChildByName("width");
    ASSERT_TRUE(widthAttr.isValid());
    ASSERT_EQ(widthAttr.getValue(0.0, true), 2.1f);
}

TEST_F(ReadLightTest, ReadTypelessMeshLight)
{
    UsdStageRefPtr stage = UsdStage::Open("test/light2.usda");
    UsdPrim lightPrim = stage->GetPrimAtPath(SdfPath("/root/lgt/gaffer/UsdCubeLight"));
    ASSERT_TRUE(static_cast<bool>(lightPrim));

    ArgsBuilder usdInArgsBuilder;
    usdInArgsBuilder.stage = stage;
    usdInArgsBuilder.rootLocation = "/root";
    usdInArgsBuilder.isolatePath = "";
    usdInArgsBuilder.sessionLocation = "";
    auto usdInArgs = usdInArgsBuilder.build();

    UsdKatanaUsdInPrivateData privateData(lightPrim, usdInArgs);
    UsdKatanaAttrMap attrs;
    UsdKatanaReadLight(lightPrim, privateData, attrs);
    FnAttribute::GroupAttribute result = attrs.build();

    // Gather and check the general light attributes.
    FnAttribute::GroupAttribute materialAttrs = result.getChildByName("material");
    ASSERT_TRUE(materialAttrs.isValid());
    FnAttribute::StringAttribute lightShaderName = materialAttrs.getChildByName("usdLightShader");
    ASSERT_TRUE(lightShaderName.isValid());
    ASSERT_EQ(lightShaderName.getValue("", false), "MeshLight");
    FnAttribute::GroupAttribute lightParamAttrs = materialAttrs.getChildByName("usdLightParams");
    ASSERT_TRUE(lightParamAttrs.isValid());

    // Check light intensity.
    FnAttribute::FloatAttribute intensityAttr = lightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(intensityAttr.isValid());
    ASSERT_EQ(intensityAttr.getValue(0.0, false), 40.0);
}

TEST_F(ReadLightTest, ReadMissingShader)
{
    UsdStageRefPtr stage = UsdStage::Open("test/light3.usda");
    UsdPrim lightPrim = stage->GetPrimAtPath(SdfPath("/root/lgt/gaffer/MixedContextLight"));
    ASSERT_TRUE(static_cast<bool>(lightPrim));

    ArgsBuilder usdInArgsBuilder;
    usdInArgsBuilder.stage = stage;
    usdInArgsBuilder.rootLocation = "/root";
    usdInArgsBuilder.isolatePath = "";
    usdInArgsBuilder.sessionLocation = "";
    auto usdInArgs = usdInArgsBuilder.build();

    UsdKatanaUsdInPrivateData privateData(lightPrim, usdInArgs);
    UsdKatanaAttrMap attrs;
    UsdKatanaReadLight(lightPrim, privateData, attrs);
    FnAttribute::GroupAttribute result = attrs.build();

    FnAttribute::GroupAttribute materialAttrs = result.getChildByName("material");
    ASSERT_TRUE(materialAttrs.isValid());
    // Get USD context attrs.
    FnAttribute::StringAttribute usdLightShaderName =
        materialAttrs.getChildByName("usdLightShader");
    ASSERT_TRUE(usdLightShaderName.isValid());
    ASSERT_EQ(usdLightShaderName.getValue("", false), "UsdLuxRectLight");
    FnAttribute::GroupAttribute usdLightParamAttrs = materialAttrs.getChildByName("usdLightParams");
    ASSERT_TRUE(usdLightParamAttrs.isValid());
    // Get Test context attrs.
    FnAttribute::StringAttribute testLightShaderName =
        materialAttrs.getChildByName("testLightShader");
    ASSERT_FALSE(testLightShaderName.isValid());

    // Check USD light shader color.
    FnAttribute::FloatAttribute colorAttr = usdLightParamAttrs.getChildByName("color");
    ASSERT_TRUE(colorAttr.isValid());
    auto colorValues = colorAttr.getNearestSample(0.0f);
    ASSERT_EQ(colorValues[0], 0.1f);
    ASSERT_EQ(colorValues[1], 0.2f);
    ASSERT_EQ(colorValues[2], 0.3f);

    // Check light intensity.
    FnAttribute::FloatAttribute intensityAttr = usdLightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(intensityAttr.isValid());
    ASSERT_EQ(intensityAttr.getValue(0.0, false), 11.0);
}

TEST_F(ReadLightTest, PerShaderAttributesTest)
{
    UsdStageRefPtr stage = UsdStage::Open("test/light3.usda");
    UsdPrim lightPrim = stage->GetPrimAtPath(SdfPath("/root/lgt/gaffer/MixedContextLight"));
    ASSERT_TRUE(static_cast<bool>(lightPrim));

    ArgsBuilder usdInArgsBuilder;
    usdInArgsBuilder.stage = stage;
    usdInArgsBuilder.rootLocation = "/root";
    usdInArgsBuilder.isolatePath = "";
    usdInArgsBuilder.sessionLocation = "";
    auto usdInArgs = usdInArgsBuilder.build();

    UsdKatanaUsdInPrivateData privateData(lightPrim, usdInArgs);
    UsdKatanaAttrMap attrs;
    UsdKatanaReadLight(lightPrim, privateData, attrs);
    FnAttribute::GroupAttribute result = attrs.build();

    FnAttribute::GroupAttribute materialAttrs = result.getChildByName("material");
    ASSERT_TRUE(materialAttrs.isValid());
    // Get USD context attrs.
    FnAttribute::StringAttribute usdLightShaderName =
        materialAttrs.getChildByName("usdLightShader");
    ASSERT_TRUE(usdLightShaderName.isValid());
    ASSERT_EQ(usdLightShaderName.getValue("", false), "UsdLuxRectLight");
    FnAttribute::GroupAttribute usdLightParamAttrs = materialAttrs.getChildByName("usdLightParams");
    ASSERT_TRUE(usdLightParamAttrs.isValid());

    // Get Test context attrs.
    FnAttribute::StringAttribute testLightShaderName =
        materialAttrs.getChildByName("fntestUsdaShader");
    ASSERT_TRUE(testLightShaderName.isValid());
    ASSERT_EQ(testLightShaderName.getValue("", false), "FnTestRectLight");
    FnAttribute::GroupAttribute testLightParamAttrs =
        materialAttrs.getChildByName("fntestUsdaParams");
    ASSERT_TRUE(testLightParamAttrs.isValid());

    // Check USD light shader color.
    FnAttribute::FloatAttribute usdColorAttr = usdLightParamAttrs.getChildByName("color");
    ASSERT_TRUE(usdColorAttr.isValid());
    auto usdColorValues = usdColorAttr.getNearestSample(0.0f);
    ASSERT_EQ(usdColorValues[0], 0.1f);
    ASSERT_EQ(usdColorValues[1], 0.2f);
    ASSERT_EQ(usdColorValues[2], 0.3f);

    // Check usd light intensity.
    FnAttribute::FloatAttribute usdIntensityAttr = usdLightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(usdIntensityAttr.isValid());
    ASSERT_EQ(usdIntensityAttr.getValue(0.0, false), 11.0);

    // Check usd light exposure.
    FnAttribute::FloatAttribute usdExposureAttr = usdLightParamAttrs.getChildByName("exposure");
    ASSERT_TRUE(usdExposureAttr.isValid());
    ASSERT_EQ(usdExposureAttr.getValue(0.0, false), 9.0);

    // Check fntest light intensity. This should be read from the namespaced value.
    FnAttribute::FloatAttribute testIntensityAttr = testLightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(testIntensityAttr.isValid());
    ASSERT_EQ(testIntensityAttr.getValue(0.0, false), 8.0);

    // Check test light shader color. This should be read from the existing non-namespaced value.
    FnAttribute::FloatAttribute testColorAttr = testLightParamAttrs.getChildByName("color");
    ASSERT_TRUE(testColorAttr.isValid());
    auto testColorValues = testColorAttr.getNearestSample(0.0f);
    ASSERT_EQ(testColorValues[0], 0.1f);
    ASSERT_EQ(testColorValues[1], 0.2f);
    ASSERT_EQ(testColorValues[2], 0.3f);

    // Check test light exposure. This should not be read as it is not part of the shader def.
    FnAttribute::FloatAttribute testExposureAttr = testLightParamAttrs.getChildByName("exposure");
    ASSERT_FALSE(testExposureAttr.isValid());
}

TEST_F(ReadLightTest, PerShaderAttributesMixedIdSourceTest)
{
    UsdStageRefPtr stage = UsdStage::Open("test/light4.usda");
    UsdPrim lightPrim = stage->GetPrimAtPath(SdfPath("/root/lgt/gaffer/MixedContextLight"));
    ASSERT_TRUE(static_cast<bool>(lightPrim));

    ArgsBuilder usdInArgsBuilder;
    usdInArgsBuilder.stage = stage;
    usdInArgsBuilder.rootLocation = "/root";
    usdInArgsBuilder.isolatePath = "";
    usdInArgsBuilder.sessionLocation = "";
    auto usdInArgs = usdInArgsBuilder.build();

    UsdKatanaUsdInPrivateData privateData(lightPrim, usdInArgs);
    UsdKatanaAttrMap attrs;
    UsdKatanaReadLight(lightPrim, privateData, attrs);
    FnAttribute::GroupAttribute result = attrs.build();

    FnAttribute::GroupAttribute materialAttrs = result.getChildByName("material");
    ASSERT_TRUE(materialAttrs.isValid());
    // Get USD context attrs.
    FnAttribute::StringAttribute usdLightShaderName =
        materialAttrs.getChildByName("usdLightShader");
    ASSERT_TRUE(usdLightShaderName.isValid());
    ASSERT_EQ(usdLightShaderName.getValue("", false), "UsdLuxRectLight");
    FnAttribute::GroupAttribute usdLightParamAttrs = materialAttrs.getChildByName("usdLightParams");
    ASSERT_TRUE(usdLightParamAttrs.isValid());

    // Get Test context attrs.
    FnAttribute::StringAttribute testLightShaderName =
        materialAttrs.getChildByName("fntestUsdaShader");
    ASSERT_TRUE(testLightShaderName.isValid());
    ASSERT_EQ(testLightShaderName.getValue("", false), "FnTestRectLight");
    FnAttribute::GroupAttribute testLightParamAttrs =
        materialAttrs.getChildByName("fntestUsdaParams");
    ASSERT_TRUE(testLightParamAttrs.isValid());

    // Check USD light shader color.
    FnAttribute::FloatAttribute usdColorAttr = usdLightParamAttrs.getChildByName("color");
    ASSERT_TRUE(usdColorAttr.isValid());
    auto usdColorValues = usdColorAttr.getNearestSample(0.0f);
    ASSERT_EQ(usdColorValues[0], 0.1f);
    ASSERT_EQ(usdColorValues[1], 0.2f);
    ASSERT_EQ(usdColorValues[2], 0.3f);

    // Check usd light intensity.
    FnAttribute::FloatAttribute usdIntensityAttr = usdLightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(usdIntensityAttr.isValid());
    ASSERT_EQ(usdIntensityAttr.getValue(0.0, false), 11.0);

    // Check usd light exposure.
    FnAttribute::FloatAttribute usdExposureAttr = usdLightParamAttrs.getChildByName("exposure");
    ASSERT_TRUE(usdExposureAttr.isValid());
    ASSERT_EQ(usdExposureAttr.getValue(0.0, false), 9.0);

    // Check fntest light intensity. This should be read from the namespaced value.
    FnAttribute::FloatAttribute testIntensityAttr = testLightParamAttrs.getChildByName("intensity");
    ASSERT_TRUE(testIntensityAttr.isValid());
    ASSERT_EQ(testIntensityAttr.getValue(0.0, false), 8.0);

    // Check test light shader color. This should be read from the existing non-namespaced value.
    FnAttribute::FloatAttribute testColorAttr = testLightParamAttrs.getChildByName("color");
    ASSERT_TRUE(testColorAttr.isValid());
    auto testColorValues = testColorAttr.getNearestSample(0.0f);
    ASSERT_EQ(testColorValues[0], 0.1f);
    ASSERT_EQ(testColorValues[1], 0.2f);
    ASSERT_EQ(testColorValues[2], 0.3f);

    // Check test light exposure. This should not be read as it is not part of the shader def.
    FnAttribute::FloatAttribute testExposureAttr = testLightParamAttrs.getChildByName("exposure");
    ASSERT_FALSE(testExposureAttr.isValid());
}

}  // namespace ReadLightTests

PXR_NAMESPACE_CLOSE_SCOPE
