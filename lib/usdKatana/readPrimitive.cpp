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

#include <functional>
#include <unordered_map>
#include <utility>

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/plane.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>

#include "usdKatana/attrMap.h"
#include "usdKatana/debugCodes.h"
#include "usdKatana/readGprim.h"
#include "usdKatana/readPrimitive.h"
#include "usdKatana/usdInPrivateData.h"

#include <FnConfig/FnConfig.h>
#include <FnGeolibServices/FnBuiltInOpArgsUtil.h>
#include <FnLogging/FnLogging.h>

#include "vtKatana/array.h"

PXR_NAMESPACE_OPEN_SCOPE

FnLogSetup("UsdKatanaReadPrimitive");

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(USDKATANA_PRIMITIVE_IMPORT,
                                "Diagnostics about UsdGeom Primitive import");
}

void ReadCapsule(const UsdPrim& prim, UsdKatanaAttrMap& attrs, const double currentTime)
{
    const UsdGeomCapsule capsule(prim);

    double radius = 1.0;
    double height = 2.0;
    std::string axis = "Z";
    if (VtValue radiusValue; capsule.GetRadiusAttr().Get(&radiusValue, currentTime))
        radius = radiusValue.Get<double>() * 2.0;
    if (VtValue heightValue; capsule.GetHeightAttr().Get(&heightValue, currentTime))
        height = heightValue.Get<double>();
    if (VtValue axisValue; capsule.GetAxisAttr().Get(&axisValue, currentTime))
        axis = axisValue.Get<TfToken>().GetString();

    const double rotationX[] = {axis == "Y" ? 90.0 : 0.0, 1.0, 0.0, 0.0};
    const double rotationY[] = {axis == "X" ? 90.0 : 0.0, 0.0, 1.0, 0.0};
    const double scale[] = {radius, radius, height};
    attrs.set("xform.primitiveImport.rotateX", FnAttribute::DoubleAttribute(rotationX, 4, 4));
    attrs.set("xform.primitiveImport.rotateY", FnAttribute::DoubleAttribute(rotationY, 4, 4));
    attrs.set("xform.primitiveImport.scale", FnAttribute::DoubleAttribute(scale, 3, 3));
}

void ReadCube(const UsdPrim& prim, UsdKatanaAttrMap& attrs, const double currentTime)
{
    UsdGeomCube cube(prim);

    if (VtValue sizeValue; cube.GetSizeAttr().Get(&sizeValue, currentTime))
    {
        const double size = sizeValue.Get<double>();
        const double scale[] = {size, size, size};
        attrs.set("xform.primitiveImport.scale", FnAttribute::DoubleAttribute(scale, 3, 3));
    }
}

void ReadSphere(const UsdPrim& prim, UsdKatanaAttrMap& attrs, const double currentTime)
{
    const UsdGeomSphere sphere(prim);

    if (VtValue radiusValue; sphere.GetRadiusAttr().Get(&radiusValue, currentTime))
    {
        const double radius = radiusValue.Get<double>();
        const double scale[] = {radius, radius, radius};
        attrs.set("xform.primitiveImport.scale", FnAttribute::DoubleAttribute(scale, 3, 3));
    }
}

void ReadCone(const UsdPrim& prim, UsdKatanaAttrMap& attrs, const double currentTime)
{
    const UsdGeomCone cone(prim);

    double radius = 1.0;
    double height = 2.0;
    std::string axis = "Z";
    if (VtValue radiusValue; cone.GetRadiusAttr().Get(&radiusValue, currentTime))
        radius = radiusValue.Get<double>();
    if (VtValue heightValue; cone.GetHeightAttr().Get(&heightValue, currentTime))
        height = heightValue.Get<double>();
    if (VtValue axisValue; cone.GetAxisAttr().Get(&axisValue, currentTime))
        axis = axisValue.Get<TfToken>().GetString();

    const double scale[] = {radius, radius, height / 2.0};
    const double rotationX[] = {axis == "Y" ? 0.0 : 90.0, 1.0, 0.0, 0.0};
    const double rotationY[] = {axis == "X" ? 90.0 : 0.0, 0.0, 1.0, 0.0};
    const double translate[] = {0.0, -1.0, 0.0};
    attrs.set("xform.primitiveImport.rotateY", FnAttribute::DoubleAttribute(rotationY, 4, 4));
    attrs.set("xform.primitiveImport.rotateX", FnAttribute::DoubleAttribute(rotationX, 4, 4));
    attrs.set("xform.primitiveImport.scale", FnAttribute::DoubleAttribute(scale, 3, 3));
    attrs.set("xform.primitiveImport.translate", FnAttribute::DoubleAttribute(translate, 3, 3));
}

void ReadCylinder(const UsdPrim& prim, UsdKatanaAttrMap& attrs, const double currentTime)
{
    const UsdGeomCylinder cylinder(prim);

    double radius = 1.0;
    double height = 2.0;
    std::string axis = "Z";
    if (VtValue radiusValue; cylinder.GetRadiusAttr().Get(&radiusValue, currentTime))
        radius = radiusValue.Get<double>();
    if (VtValue heightValue; cylinder.GetHeightAttr().Get(&heightValue, currentTime))
        height = heightValue.Get<double>();
    if (VtValue axisValue; cylinder.GetAxisAttr().Get(&axisValue, currentTime))
        axis = axisValue.Get<TfToken>().GetString();

    const double scale[] = {radius, radius, height / 2.0};
    const double rotationX[] = {axis == "Y" ? 0.0 : 90.0, 1.0, 0.0, 0.0};
    const double rotationY[] = {axis == "X" ? 90.0 : 0.0, 0.0, 1.0, 0.0};
    attrs.set("xform.primitiveImport.scale", FnAttribute::DoubleAttribute(scale, 3, 3));
    attrs.set("xform.primitiveImport.rotateY", FnAttribute::DoubleAttribute(rotationY, 4, 4));
    attrs.set("xform.primitiveImport.rotateX", FnAttribute::DoubleAttribute(rotationX, 4, 4));
}

void ReadPlane(const UsdPrim& prim, UsdKatanaAttrMap& attrs, const double currentTime)
{
    const UsdGeomPlane plane(prim);

    double length = 1.0;
    double width = 1.0;
    std::string axis = "Z";
    if (VtValue lengthValue; plane.GetLengthAttr().Get(&lengthValue, currentTime))
        length = lengthValue.Get<double>();
    if (VtValue widthValue; plane.GetWidthAttr().Get(&widthValue, currentTime))
        width = widthValue.Get<double>();
    if (VtValue axisValue; plane.GetAxisAttr().Get(&axisValue, currentTime))
        axis = axisValue.Get<TfToken>().GetString();

    const double rotationX[] = {axis == "X" || axis == "Z" ? 90.0 : 0.0, 1.0, 0.0, 0.0};
    const double rotationZ[] = {axis == "X" ? -90.0 : 0.0, 0.0, 0.0, 1.0};
    const double scale[] = {width, 1.0, length};
    attrs.set("xform.primitiveImport.rotateX", FnAttribute::DoubleAttribute(rotationX, 4, 4));
    attrs.set("xform.primitiveImport.rotateZ", FnAttribute::DoubleAttribute(rotationZ, 4, 4));
    attrs.set("xform.primitiveImport.scale", FnAttribute::DoubleAttribute(scale, 3, 3));
}

typedef std::pair<std::string, std::function<void(const UsdPrim&, UsdKatanaAttrMap&, const double)>>
    PrimitiveSourceReaderPair;
static std::unordered_map<TfToken, PrimitiveSourceReaderPair, TfToken::HashFunctor>
    s_typeToAttrsAndFuncMap({
        {TfToken("Capsule"), {"poly_capsule", ReadCapsule}},
        {TfToken("Cube"), {"cube", ReadCube}},
        {TfToken("Cone"), {"poly_cone", ReadCone}},
        {TfToken("Cylinder"), {"poly_cylinder", ReadCylinder}},
        {TfToken("Plane"), {"poly_plane", ReadPlane}},
        {TfToken("Sphere"), {"poly_sphere", ReadSphere}},
    });

void UsdKatanaReadPrimitive(const UsdPrim& prim,
                            const UsdKatanaUsdInPrivateData& data,
                            UsdKatanaAttrMap& attrs,
                            std::string& attrsFilePath)
{
    UsdKatanaReadGprim(UsdGeomGprim(prim), data, attrs);

    static std::string resourcesDir;
    if (resourcesDir.empty())
    {
        resourcesDir =
            FnConfig::Config::get("KATANA_INTERNAL_RESOURCES") + "/Geometry/PrimitiveCreate/";
    }

    if (const auto& itr = s_typeToAttrsAndFuncMap.find(prim.GetTypeName());
        itr != s_typeToAttrsAndFuncMap.end())
    {
        const auto& [attrsFileName, readerFunc] = itr->second;
        attrsFilePath = resourcesDir + attrsFileName + ".attrs";
        readerFunc(prim, attrs, data.GetCurrentTime());
    }
    else
    {
        FnLogWarn("Unsupported Primitive type '" << prim.GetTypeName() << "'");
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
