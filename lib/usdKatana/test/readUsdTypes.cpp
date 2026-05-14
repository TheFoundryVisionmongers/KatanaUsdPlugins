// Copyright (c) 2026 The Foundry Visionmongers Ltd.  All rights reserved.
#include "gtest/gtest.h"

#include <iostream>

#include "pxr/pxr.h"

#include "FnAttribute/FnAttribute.h"
#include "usdKatana/attrMap.h"
#include "usdKatana/readMesh.h"
#include "usdKatana/usdInArgs.h"
#include "usdKatana/usdInPrivateData.h"

PXR_NAMESPACE_OPEN_SCOPE

class ReadUsdTypeTest : public ::testing::Test
{
private:
    static UsdStageRefPtr _stage;

protected:
    static UsdKatanaUsdInArgsRefPtr _usdInArgs;
    static FnAttribute::GroupAttribute _attributes;

    static void SetUpTestSuite()
    {
        // Setup Stage
        _stage = UsdStage::Open("test/types.usda");
        UsdPrim meshPrim = _stage->GetPrimAtPath(SdfPath("/mesh"));
        ASSERT_TRUE(static_cast<bool>(meshPrim));

        // Setup UsdIn args
        ArgsBuilder usdInArgsBuilder;
        usdInArgsBuilder.stage = _stage;
        usdInArgsBuilder.rootLocation = "/";
        usdInArgsBuilder.isolatePath = "";
        usdInArgsBuilder.sessionLocation = "";
        _usdInArgs = usdInArgsBuilder.build();

        // Read and create Attributes
        UsdKatanaAttrMap attrs;
        UsdKatanaUsdInPrivateData privateData(meshPrim, _usdInArgs);

        UsdKatanaReadMesh(UsdGeomMesh(meshPrim), privateData, attrs);
        _attributes = attrs.build();
    }
};

UsdStageRefPtr ReadUsdTypeTest::_stage;
UsdKatanaUsdInArgsRefPtr ReadUsdTypeTest::_usdInArgs;
FnAttribute::GroupAttribute ReadUsdTypeTest::_attributes;

namespace ReadUsdTypeTests
{
TEST_F(ReadUsdTypeTest, ReadHalfType)
{
    const std::string attributePath = "geometry.arbitrary.halfConstant";
    const FnAttribute::GroupAttribute& halfConstantAttr = _attributes.getChildByName(attributePath);
    ASSERT_TRUE(halfConstantAttr.isValid());

    const FnAttribute::StringAttribute& usdTypeAttr =
        halfConstantAttr.getChildByName("usd.usdType");
    const FnAttribute::IntAttribute& elementAttr = halfConstantAttr.getChildByName("elementSize");
    const FnAttribute::FloatAttribute& valueAttr = halfConstantAttr.getChildByName("value");
    ASSERT_TRUE(usdTypeAttr.isValid());
    ASSERT_TRUE(elementAttr.isValid());
    ASSERT_TRUE(valueAttr.isValid());

    ASSERT_EQ(usdTypeAttr.getValue(), "half");
    ASSERT_EQ(elementAttr.getValue(), 1);

    // Clamp for float precision error handling.
    const float clampedValue = std::round(valueAttr.getValue() * 100) / 100;
    ASSERT_EQ(clampedValue, 0.35f);
}

TEST_F(ReadUsdTypeTest, ReadHalfVec2Type)
{
    const std::string attributePath = "geometry.arbitrary.halfVec2";
    const FnAttribute::GroupAttribute& halfVecAttr = _attributes.getChildByName(attributePath);
    ASSERT_TRUE(halfVecAttr.isValid());

    const FnAttribute::StringAttribute& usdTypeAttr = halfVecAttr.getChildByName("usd.usdType");
    const FnAttribute::IntAttribute& elementAttr = halfVecAttr.getChildByName("elementSize");
    const FnAttribute::FloatAttribute& valueAttr = halfVecAttr.getChildByName("value");
    ASSERT_TRUE(usdTypeAttr.isValid());
    ASSERT_TRUE(elementAttr.isValid());
    ASSERT_TRUE(valueAttr.isValid());

    ASSERT_EQ(usdTypeAttr.getValue(), "half2");
    ASSERT_EQ(elementAttr.getValue(), 2);

    const std::array<float, 2> expectedValues = {0.138f, 0.076f};
    for (int idx = 0; idx < valueAttr.getNumberOfValues(); idx++)
    {
        const auto values = valueAttr.getValuesAs<GfVec2f, 2>();
        const float clampedValue = std::round(values[idx] * 1000) / 1000;
        ASSERT_EQ(clampedValue, expectedValues[idx]);
    }
}

TEST_F(ReadUsdTypeTest, ReadHalfVec3Type)
{
    const std::string attributePath = "geometry.arbitrary.halfVec3";
    const FnAttribute::GroupAttribute& halfVecAttr = _attributes.getChildByName(attributePath);
    ASSERT_TRUE(halfVecAttr.isValid());

    const FnAttribute::StringAttribute& usdTypeAttr = halfVecAttr.getChildByName("usd.usdType");
    const FnAttribute::IntAttribute& elementAttr = halfVecAttr.getChildByName("elementSize");
    const FnAttribute::FloatAttribute& valueAttr = halfVecAttr.getChildByName("value");
    ASSERT_TRUE(usdTypeAttr.isValid());
    ASSERT_TRUE(elementAttr.isValid());
    ASSERT_TRUE(valueAttr.isValid());

    ASSERT_EQ(usdTypeAttr.getValue(), "half3");
    ASSERT_EQ(elementAttr.getValue(), 3);

    const std::array<float, 3> expectedValues = {0.138f, 0.076f, 0.062f};
    for (int idx = 0; idx < valueAttr.getNumberOfValues(); idx++)
    {
        const auto values = valueAttr.getValuesAs<GfVec3f, 3>();
        const float clampedValue = std::round(values[idx] * 1000) / 1000;
        ASSERT_EQ(clampedValue, expectedValues[idx]);
    }
}

TEST_F(ReadUsdTypeTest, ReadHalfVec4Type)
{
    const std::string attributePath = "geometry.arbitrary.halfVec4";
    const FnAttribute::GroupAttribute& halfVecAttr = _attributes.getChildByName(attributePath);
    ASSERT_TRUE(halfVecAttr.isValid());

    const FnAttribute::StringAttribute& usdTypeAttr = halfVecAttr.getChildByName("usd.usdType");
    const FnAttribute::IntAttribute& elementAttr = halfVecAttr.getChildByName("elementSize");
    const FnAttribute::FloatAttribute& valueAttr = halfVecAttr.getChildByName("value");
    ASSERT_TRUE(usdTypeAttr.isValid());
    ASSERT_TRUE(elementAttr.isValid());
    ASSERT_TRUE(valueAttr.isValid());

    ASSERT_EQ(usdTypeAttr.getValue(), "half4");
    ASSERT_EQ(elementAttr.getValue(), 4);

    const std::array<float, 4> expectedValues = {0.138f, 0.076f, 0.062f, 0.057f};
    for (int idx = 0; idx < valueAttr.getNumberOfValues(); idx++)
    {
        const auto values = valueAttr.getValuesAs<GfVec4f, 4>();
        const float clampedValue = std::round(values[idx] * 1000) / 1000;
        ASSERT_EQ(clampedValue, expectedValues[idx]);
    }
}

TEST_F(ReadUsdTypeTest, ReadHalfArrayType)
{
    const std::string attributePath = "geometry.arbitrary.halfArray";
    const FnAttribute::GroupAttribute& halfArrayAttr = _attributes.getChildByName(attributePath);
    ASSERT_TRUE(halfArrayAttr.isValid());

    const FnAttribute::StringAttribute& usdTypeAttr = halfArrayAttr.getChildByName("usd.usdType");
    const FnAttribute::FloatAttribute& valueAttr = halfArrayAttr.getChildByName("value");
    ASSERT_TRUE(usdTypeAttr.isValid());
    ASSERT_TRUE(valueAttr.isValid());

    const int expectedNumValues = 8;
    ASSERT_EQ(usdTypeAttr.getValue(), "half[]");
    ASSERT_EQ(valueAttr.getNumberOfValues(), expectedNumValues);

    const std::vector expectedValues = std::vector<float>{1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    const std::vector floatArr = valueAttr.getValuesAs<std::vector<float>, expectedNumValues>();
    ASSERT_EQ(floatArr, expectedValues);
}
}  // namespace ReadUsdTypeTests

PXR_NAMESPACE_CLOSE_SCOPE
