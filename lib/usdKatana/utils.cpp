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

#include "pxr/base/arch/demangle.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/base/work/loops.h"
#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/ar/resolverScopedCache.h"
#include "pxr/usd/kind/registry.h"
#include "pxr/usd/pcp/mapExpression.h"
#include "pxr/usd/sdr/registry.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/collectionAPI.h"
#include "pxr/usd/usd/modelAPI.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/relationship.h"
#include "pxr/usd/usdGeom/boundable.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdLux/lightAPI.h"
#include "pxr/usd/usdLux/lightFilter.h"
#include "pxr/usd/usdLux/listAPI.h"
#include "pxr/usd/usdRi/statementsAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/blendShapeQuery.h"
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/skeletonQuery.h"
#include "pxr/usd/usdSkel/skinningQuery.h"
#include "pxr/usd/usdUI/sceneGraphPrimAPI.h"
#include "pxr/usd/usdUtils/pipeline.h"

#include "vtKatana/array.h"
#include "vtKatana/value.h"

#include "usdKatana/utils.h"
#include "usdKatana/blindDataObject.h"
#include "usdKatana/lookAPI.h"
#include "usdKatana/baseMaterialHelpers.h"

#include <FnLogging/FnLogging.h>

FnLogSetup("UsdKatanaUtils");

#include "boost/filesystem.hpp"
#include "boost/regex.hpp"

#include <cmath>
#include <map>
#include <sstream>
#include <unordered_map>

PXR_NAMESPACE_OPEN_SCOPE

#if defined(ARCH_OS_WINDOWS)
TF_DEFINE_ENV_SETTING(
    USD_KATANA_LOOK_TOKENS,
    "Looks;looks;materials",
    "Defines which prim names will allow for any child Material prims which have sibling materials "
    "to become child materials where a Specializes composition arc exists between them.");
#elif defined(ARCH_OS_LINUX)
TF_DEFINE_ENV_SETTING(
    USD_KATANA_LOOK_TOKENS,
    "Looks:looks:materials",
    "Defines which prim names will allow for any child Material prims which have sibling materials "
    "to become child materials where a Specializes composition arc exists between them.");
#endif

namespace
{
void ApplyBlendShapeAnimation(const UsdSkelSkinningQuery& skinningQuery,
                              const UsdSkelSkeletonQuery& skelQuery,
                              const double time,
                              VtVec3fArray& points)
{
    const UsdSkelBlendShapeQuery blendShapeQuery =
        UsdSkelBindingAPI(skinningQuery.GetPrim());
    if (!blendShapeQuery.IsValid())
    {
        return;
    }

    if (const UsdSkelAnimMapperRefPtr blendShapeMapper =
            skinningQuery.GetBlendShapeMapper())
    {
        VtFloatArray blendShapeWeights;
        const auto animQuery = skelQuery.GetAnimQuery();
        if (!animQuery.IsValid())
        {
            return;
        }
        animQuery.ComputeBlendShapeWeights(&blendShapeWeights, time);

        VtFloatArray weightsForPrim;
        if (blendShapeMapper->Remap(blendShapeWeights, &weightsForPrim))
        {
            VtFloatArray subShapeWeights;
            VtUIntArray blendShapeIndices, subShapeIndices;
            if (blendShapeQuery.ComputeSubShapeWeights(
                    weightsForPrim, &subShapeWeights, &blendShapeIndices,
                    &subShapeIndices))
            {
                const std::vector<VtIntArray> blendShapePointIndices =
                    blendShapeQuery.ComputeBlendShapePointIndices();
                const std::vector<VtVec3fArray> subShapePointOffsets =
                    blendShapeQuery.ComputeSubShapePointOffsets();

                blendShapeQuery.ComputeDeformedPoints(
                    subShapeWeights, blendShapeIndices, subShapeIndices,
                    blendShapePointIndices, subShapePointOffsets, points);
            }
        }
    }
};

void ApplyJointAnimation(const UsdSkelSkinningQuery& skinningQuery,
                         const UsdSkelSkeletonQuery& skelQuery,
                         const double time,
                         VtVec3fArray& points)
{
    // Get the skinning transform from the skeleton.
    VtMatrix4dArray skinningXforms;
    skelQuery.ComputeSkinningTransforms(&skinningXforms, time);
    // Get the prim's points first and then skin them.
    skinningQuery.ComputeSkinnedPoints(skinningXforms, &points, time);

    // Apply transforms to get the points in mesh prim space
    // instead of skeleton space.
    UsdGeomXformCache xformCache(time);
    const UsdPrim& skelPrim = skelQuery.GetPrim();
    const GfMatrix4d skelLocalToWorld =
        xformCache.GetLocalToWorldTransform(skelPrim);
    const GfMatrix4d primWorldToLocal =
        xformCache.GetLocalToWorldTransform(skinningQuery.GetPrim())
            .GetInverse();
    const GfMatrix4d skelToPrimLocal = skelLocalToWorld * primWorldToLocal;
    WorkParallelForN(
        points.size(),
        [&](size_t start, size_t end) {
            for (size_t i = start; i < end; ++i)
            {
                points[i] = skelToPrimLocal.Transform(points[i]);
            }
        },
        /*grainSize*/ 1000);
};
}  // namespace

static const std::string _ResolveAssetPath(const SdfAssetPath& assetPath)
{
    if (!assetPath.GetResolvedPath().empty())
        return assetPath.GetResolvedPath();

    const std::string& rawPath = assetPath.GetAssetPath();
    size_t udimIdx = rawPath.rfind("<UDIM>");
    if (udimIdx != std::string::npos)
    {
        // assetPath points to a UDIM set.  We find the first tile, with <UDIM>
        // replaced by an ID 1xxx, resolve that path, and return the resolved
        // path with 1xxx re-replaced again with <UDIM>.
        boost::filesystem::path boostPath(rawPath);
        boost::filesystem::path dirPath = boostPath.parent_path();
        if (boost::filesystem::exists(dirPath))
        {
            boost::filesystem::path filterPath(rawPath);
            std::string filter = filterPath.filename().string();
            size_t filterSize = filter.size();
            filter.replace(udimIdx - dirPath.string().size() - 1, 6,
                           "1\\d\\d\\d");

            const boost::regex regexFilter(filter);

            boost::filesystem::directory_iterator beginIt{dirPath};
            boost::filesystem::directory_iterator endIt;
            for (auto it = beginIt; it != endIt; ++it)
            {
                if (!boost::filesystem::is_regular_file(it->status()))
                    continue;

                boost::smatch what;
                const std::string path = it->path().string();
                const std::string filename = it->path().filename().string();
                if ((filename.size() == (filterSize - 2)) &&
                    boost::regex_match(filename, what, regexFilter))
                {
                    ArResolverScopedCache resolverCache;
                    ArResolver& resolver = ArGetResolver();
                    std::string resolvedPath = resolver.Resolve(path);
                    if (resolvedPath.size() > (udimIdx + 4))
                    {
                        return resolvedPath.replace(udimIdx, 4, "<UDIM>");
                    }
                }
            }
        }

        // TP 485194: As of 21.05, HdStorm will attempt to bind missing textures, causing a crash
        // when it tries to dereference the texture to get its GLuint handle.  If we couldn't
        // resolve the path, return an empty string; unfortunately this means we don't show the
        // original path in Katana attributes.
        TF_WARN("No resolved path for UDIM texture @%s@", rawPath.c_str());
        return std::string();
    }

    // There's no resolved path and it's not a UDIM path.
    if (!rawPath.empty())
    {
        TF_WARN("No resolved path for @%s@", rawPath.c_str());
    }

    return rawPath;
}

double UsdKatanaUtils::ReverseTimeSample(double sample)
{
    // Only multiply when the sample is not 0 to avoid writing
    // out a motion block containing -0.
    return (sample == 0.0) ? sample : sample * -1;
}

void UsdKatanaUtils::ConvertNumVertsToStartVerts(const std::vector<int>& numVertsVec,
                                                 std::vector<int>* startVertsVec)
{
    startVertsVec->resize( numVertsVec.size()+1 );
    int index = 0;
    for (size_t i=0; i<=numVertsVec.size(); ++i) {
        (*startVertsVec)[i] = index;
        if (i < numVertsVec.size()) {
            index += numVertsVec[i];
        }
    }
}

void UsdKatanaUtils::ConvertArrayToVector(const VtVec3fArray& a, std::vector<float>* r)
{
    r->resize(a.size()*3);
    size_t i=0;
    TF_FOR_ALL(vec, a) {
        (*r)[i++] = (*vec)[0];
        (*r)[i++] = (*vec)[1];
        (*r)[i++] = (*vec)[2];
    }
    TF_VERIFY(i == r->size());
}

FnKat::Attribute UsdKatanaUtils::ConvertVtValueToKatAttr(const VtValue& val, bool asShaderParam)
{
    if (val.IsHolding<bool>()) {
        return FnKat::IntAttribute(int(val.UncheckedGet<bool>()));
    }
    if (val.IsHolding<int>()) {
        return FnKat::IntAttribute(val.UncheckedGet<int>());
    }
    if (val.IsHolding<float>()) {
        return FnKat::FloatAttribute(val.UncheckedGet<float>());
    }
    if (val.IsHolding<double>()) {
        return FnKat::DoubleAttribute(val.UncheckedGet<double>());
    }
    if (val.IsHolding<std::string>()) {
        if (val.UncheckedGet<std::string>() == "_NO_VALUE_") {
            return FnKat::NullAttribute();
        }
        else {
            return FnKat::StringAttribute(val.UncheckedGet<std::string>());
        }
    }
    if (val.IsHolding<SdfAssetPath>()) {
        const SdfAssetPath& assetPath(val.UncheckedGet<SdfAssetPath>());
        return FnKat::StringAttribute(_ResolveAssetPath(assetPath));
    }
    if (val.IsHolding<TfToken>()) {
        const TfToken &myVal = val.UncheckedGet<TfToken>();
        return FnKat::StringAttribute(myVal.GetString());
    }

    // Compound types require special handling.  Because they do not
    // correspond 1:1 to Fn attribute types, we must describe the
    // type as a separate attribute.
    FnKat::Attribute typeAttr;
    FnKat::Attribute valueAttr;

    if (val.IsHolding<VtArray<std::string> >()) {
        const auto& array = val.UncheckedGet<VtArray<std::string> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("string [%zu]", array.size()));
    }

    else if (val.IsHolding<VtArray<TfToken> >()) {
        const auto& array = val.UncheckedGet<VtArray<TfToken> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("string [%zu]", array.size()));
    }

    else if (val.IsHolding<VtArray<int> >()) {
        const VtArray<int> array = val.UncheckedGet<VtArray<int> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("int [%zu]", array.size()));
    }

    else if (val.IsHolding<VtArray<unsigned> >()) {
        // Lossy translation of array<unsigned> to array<int>
        // No warning is printed as they obscure more important warnings
        const VtArray<unsigned> array = val.Get<VtArray<unsigned> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("unsigned [%zu]", array.size()));
    }

    else if (val.IsHolding<VtArray<long> >()) {
        // Lossy translation of array<long> to array<int>
        // No warning is printed as they obscure more important warnings
        const VtArray<long> array = val.Get<VtArray<long> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("long [%zu]", array.size()));
    }

    else if (val.IsHolding<VtArray<float> >()) {
        const VtArray<float> array = val.UncheckedGet<VtArray<float> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("float [%zu]", array.size()));
    }
    else if (val.IsHolding<VtArray<double> >()) {
        const VtArray<double> array = val.UncheckedGet<VtArray<double> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("double [%zu]", array.size()));
    }

    // XXX: Should matrices also be brought in as doubles?
    // What implications does this have? xform.matrix is handled explicitly as
    // a double, and apparently we don't use GfMatrix4f.
    // Shader parameter floats might expect a float matrix?
    if (val.IsHolding<VtArray<GfMatrix4d> >()) {
        const VtArray<GfMatrix4d> rawVal = val.UncheckedGet<VtArray<GfMatrix4d> >();
        std::vector<float> vec;
        TF_FOR_ALL(mat, rawVal) {
             for (int i=0; i < 4; ++i) {
                 for (int j=0; j < 4; ++j) {
                     vec.push_back( static_cast<float>((*mat)[i][j]) );
                 }
             }
         }
         FnKat::FloatBuilder builder(/* tupleSize = */ 16);
         builder.set(vec);
         valueAttr = builder.build();
         typeAttr = FnKat::StringAttribute(
             TfStringPrintf("matrix [%zu]", rawVal.size()));
    }

    // GfVec2f
    else if (val.IsHolding<GfVec2f>()) {
        const GfVec2f rawVal = val.UncheckedGet<GfVec2f>();
        valueAttr = VtKatanaCopy(rawVal);
        typeAttr = FnKat::StringAttribute("float [2]");
    }

    // GfVec2d
    else if (val.IsHolding<GfVec2d>()) {
        const GfVec2d rawVal = val.UncheckedGet<GfVec2d>();
        valueAttr = VtKatanaCopy(rawVal);
        typeAttr = FnKat::StringAttribute("double [2]");
    }

    // GfVec3f
    else if (val.IsHolding<GfVec3f>()) {
        const GfVec3f rawVal = val.UncheckedGet<GfVec3f>();
        valueAttr = VtKatanaCopy(rawVal);
        typeAttr = FnKat::StringAttribute("float [3]");
    }

    // GfVec3d
    else if (val.IsHolding<GfVec3d>()) {
        const GfVec3d rawVal = val.UncheckedGet<GfVec3d>();
        valueAttr = VtKatanaCopy(rawVal);
        typeAttr = FnKat::StringAttribute("double [3]");
    }

    // GfVec4f
    else if (val.IsHolding<GfVec4f>()) {
        const GfVec4f rawVal = val.UncheckedGet<GfVec4f>();
        valueAttr = VtKatanaCopy(rawVal);
        typeAttr = FnKat::StringAttribute("float [4]");
    }

    // GfVec4d
    else if (val.IsHolding<GfVec4d>()) {
        const GfVec4d rawVal = val.UncheckedGet<GfVec4d>();
        valueAttr = VtKatanaCopy(rawVal);
        typeAttr = FnKat::StringAttribute("double [4]");
    }

    // GfMatrix4d
    // XXX: Should matrices also be brought in as doubles?
    // What implications does this have? xform.matrix is handled explicitly as
    // a double, and apparently we don't use GfMatrix4f.
    // Shader parameter floats might expect a float matrix?
    else if (val.IsHolding<GfMatrix4d>()) {
        const GfMatrix4d rawVal = val.UncheckedGet<GfMatrix4d>();
        FnKat::FloatBuilder builder(/* tupleSize = */ 16);
        std::vector<float> vec;
        vec.resize(16);
        for (int i=0; i < 4; ++i) {
            for (int j=0; j < 4; ++j) {
                vec[i*4+j] = static_cast<float>(rawVal[i][j]);
            }
        }
        builder.set(vec);
        typeAttr = FnKat::StringAttribute("matrix [1]");
        valueAttr = builder.build();
    }

    // TODO: support complex types such as primvars
    // VtArray<GfVec4f>
    else if (val.IsHolding<VtArray<GfVec4f> >()) {
        const VtArray<GfVec4f> array = val.UncheckedGet<VtArray<GfVec4f> >();
        valueAttr = VtKatanaMapOrCopy(array);
        // NOTE: needs typeAttr set?
    }

    // VtArray<GfVec3f>
    else if (val.IsHolding<VtArray<GfVec3f> >()) {
        const VtArray<GfVec3f> array = val.UncheckedGet<VtArray<GfVec3f> >();
        valueAttr = VtKatanaMapOrCopy(array);
        // NOTE: needs typeAttr set?
    }

    // VtArray<GfVec2f>
    else if (val.IsHolding<VtArray<GfVec2f> >()) {
        const VtArray<GfVec2f> array = val.UncheckedGet<VtArray<GfVec2f> >();
        valueAttr = VtKatanaMapOrCopy(array);
        // NOTE: needs typeAttr set?
    }

    // VtArray<GfVec4d>
    else if (val.IsHolding<VtArray<GfVec4d> >()) {
        const VtArray<GfVec4d> array = val.UncheckedGet<VtArray<GfVec4d> >();
	valueAttr = VtKatanaMapOrCopy(array);
        // NOTE: needs typeAttr set?
    }

    // VtArray<GfVec3d>
    else if (val.IsHolding<VtArray<GfVec3d> >()) {
        const VtArray<GfVec3d> array = val.UncheckedGet<VtArray<GfVec3d> >();
        valueAttr = VtKatanaMapOrCopy(array);
        // NOTE: needs typeAttr set?
    }

    // VtArray<GfVec2d>
    else if (val.IsHolding<VtArray<GfVec2d> >()) {
        const VtArray<GfVec2d> array = val.UncheckedGet<VtArray<GfVec2d> >();
        valueAttr = VtKatanaMapOrCopy(array);
        // NOTE: needs typeAttr set?
    }

    // VtArray<SdfAssetPath>
    else if (val.IsHolding<VtArray<SdfAssetPath> >()) {
        // This will replicate the previous behavior:
        // if (asShaderParam) return valueAttr; asShaderParam = false;
        const VtArray<SdfAssetPath> &array = val.UncheckedGet<VtArray<SdfAssetPath> >();
        valueAttr = VtKatanaMapOrCopy(array);
        typeAttr = FnKat::StringAttribute(
            TfStringPrintf("string [%zu]", array.size()));
    }

    // If being used as a shader param, the type will be provided elsewhere,
    // so simply return the value attribute as-is.
    if (asShaderParam) {
        return valueAttr;
    }
    // Otherwise, return the type & value in a group.
    if (typeAttr.isValid() && valueAttr.isValid()) {
        FnKat::GroupBuilder groupBuilder;
        groupBuilder.set("type", typeAttr);
        groupBuilder.set("value", valueAttr);
        return groupBuilder.build();
    }
    return FnKat::Attribute();
}

FnKat::Attribute UsdKatanaUtils::ConvertRelTargetsToKatAttr(const UsdRelationship& rel,
                                                            bool asShaderParam)
{
    SdfPathVector targets;
    rel.GetForwardedTargets(&targets);
    FnKat::Attribute valueAttr;
    std::vector<std::string> vec;
    TF_FOR_ALL(targetItr, targets) {
        UsdPrim targetPrim =
            rel.GetPrim().GetStage()->GetPrimAtPath(*targetItr);
        if (targetPrim) {
            if (targetPrim.IsA<UsdShadeShader>()){
                vec.push_back(UsdKatanaUtils::GenerateShadingNodeHandle(targetPrim));
            }
            else {
                vec.push_back(targetItr->GetString());
            }
        }
        else if (targetItr->IsPropertyPath()) {
            if (UsdPrim owningPrim =
                rel.GetPrim().GetStage()->GetPrimAtPath(
                    targetItr->GetPrimPath())) {
                const TfTokenVector &propNames = owningPrim.GetPropertyNames();
                if (std::count(propNames.begin(),
                               propNames.end(),
                               targetItr->GetNameToken())) {
                    vec.push_back(targetItr->GetString());
                }
            }
        }

    }
    FnKat::StringBuilder builder(/* tupleSize = */ 1);
    builder.set(vec);
    valueAttr = builder.build();

    // If being used as a shader param, the type will be provided elsewhere,
    // so simply return the value attribute as-is.
    if (asShaderParam) {
        return valueAttr;
    }

    // Otherwise, return the type & value in a group.
    FnKat::Attribute typeAttr = FnKat::StringAttribute(
        TfStringPrintf("string [%zu]", targets.size()));

    if (typeAttr.isValid() && valueAttr.isValid()) {
        FnKat::GroupBuilder groupBuilder;
        groupBuilder.set("type", typeAttr);
        groupBuilder.set("value", valueAttr);
        return groupBuilder.build();
    }
    return FnKat::Attribute();
}



static bool
_KTypeAndSizeFromUsdVec2(TfToken const &roleName,
                         const char *typeStr,
                         FnKat::Attribute *inputTypeAttr,
                         FnKat::Attribute *elementSizeAttr)
{
    if (roleName == SdfValueRoleNames->Point) {
        *inputTypeAttr = FnKat::StringAttribute("point2");
    } else if (roleName == SdfValueRoleNames->Vector) {
        *inputTypeAttr = FnKat::StringAttribute("vector2");
    } else if (roleName == SdfValueRoleNames->Normal) {
        *inputTypeAttr = FnKat::StringAttribute("normal2");
    } else if (roleName == SdfValueRoleNames->TextureCoordinate ||
               roleName.IsEmpty()) {
        *inputTypeAttr = FnKat::StringAttribute(typeStr);
        *elementSizeAttr = FnKat::IntAttribute(2);
    } else {
        return false;
    }
    return true;
}

static bool
_KTypeAndSizeFromUsdVec3(TfToken const &roleName,
                         const char *typeStr,
                         FnKat::Attribute *inputTypeAttr,
                         FnKat::Attribute *elementSizeAttr)
{
    if (roleName == SdfValueRoleNames->Point) {
        *inputTypeAttr = FnKat::StringAttribute("point3");
    } else if (roleName == SdfValueRoleNames->Vector) {
        *inputTypeAttr = FnKat::StringAttribute("vector3");
    } else if (roleName == SdfValueRoleNames->Normal) {
        *inputTypeAttr = FnKat::StringAttribute("normal3");
    } else if (roleName == SdfValueRoleNames->Color) {
        *inputTypeAttr = FnKat::StringAttribute("color3");
    } else if (roleName == SdfValueRoleNames->TextureCoordinate ||
               roleName.IsEmpty()) {
        // Deserves explanation: there is no type in prman
        // (or apparently, katana) that represents
        // "a 3-vector with no additional behavior/meaning.
        // P-refs fall into this category.  In our pipeline,
        // we have chosen to represent this as float[3] to
        // renderers.
        *inputTypeAttr = FnKat::StringAttribute(typeStr);
        *elementSizeAttr = FnKat::IntAttribute(3);
    } else {
        return false;
    }
    return true;
}

static bool
_KTypeAndSizeFromUsdVec4(TfToken const &roleName,
                         const char *typeStr,
                         FnKat::Attribute *inputTypeAttr,
                         FnKat::Attribute *elementSizeAttr)
{
    if (roleName == SdfValueRoleNames->Point) {
        *inputTypeAttr = FnKat::StringAttribute("point4");
    } else if (roleName == SdfValueRoleNames->Vector) {
        *inputTypeAttr = FnKat::StringAttribute("vector4");
    } else if (roleName == SdfValueRoleNames->Normal) {
        *inputTypeAttr = FnKat::StringAttribute("normal4");
    } else if (roleName == SdfValueRoleNames->Color) {
        *inputTypeAttr = FnKat::StringAttribute("color4");
    } else if (roleName.IsEmpty()) {
        // We are mimicking the behavior of
        // _KTypeAndSizeFromUsdVec3 here.
        *inputTypeAttr = FnKat::StringAttribute(typeStr);
        *elementSizeAttr = FnKat::IntAttribute(4);
    } else {
        return false;
    }
    return true;
}

static bool
_KTypeAndSizeFromUsdVec2(TfToken const &roleName,
                         FnKat::Attribute *inputTypeAttr,
                         FnKat::Attribute *elementSizeAttr)
{
    if (roleName.IsEmpty()) {
        // Deserves explanation: there is no type in prman
        // (or apparently, katana) that represents
        // "a 2-vector with no additional behavior/meaning.
        // UVs fall into this category.  In our pipeline,
        // we have chosen to represent this as float[2] to
        // renderers.
        *inputTypeAttr = FnKat::StringAttribute("float");
        *elementSizeAttr = FnKat::IntAttribute(2);
    } else {
        return false;
    }
    return true;
}

void UsdKatanaUtils::ConvertVtValueToKatCustomGeomAttr(const VtValue& val,
                                                       int elementSize,
                                                       const TfToken& roleName,
                                                       FnKat::Attribute* valueAttr,
                                                       FnKat::Attribute* inputTypeAttr,
                                                       FnKat::Attribute* elementSizeAttr)
{
    // The following encoding is taken from Katana's
    // "LOCATIONS AND ATTRIBUTES" doc, which says this about
    // the "geometry.arbitrary.xxx" attributes:
    //
    // > Note: Katana currently supports the following types: float,
    // > double, int, string, color3, color4, normal2, normal3, vector2,
    // > vector3, vector4, point2, point3, point4, matrix9, matrix16.
    // > Depending on the renderer's capabilities, all these nodes might
    // > not be supported.

    // Usd half and half3 are converted to katana float and float3

    // TODO:
    // half4, color4, vector4, point4, matrix9

    if (val.IsHolding<float>()) {
        *valueAttr =  FnKat::FloatAttribute(val.Get<float>());
        *inputTypeAttr = FnKat::StringAttribute("float");
        *elementSizeAttr = FnKat::IntAttribute(elementSize);
        // Leave elementSize empty.
        return;
    }
    if (val.IsHolding<double>()) {
        // XXX(USD) Kat says it supports double here -- should we preserve
        // double-ness?
        *valueAttr =
            FnKat::DoubleAttribute(val.Get<double>());
        *inputTypeAttr = FnKat::StringAttribute("double");
        // Leave elementSize empty.
        return;
    }
    if (val.IsHolding<int>()) {
        *valueAttr = FnKat::IntAttribute(val.Get<int>());
        *inputTypeAttr = FnKat::StringAttribute("int");
        // Leave elementSize empty.
        return;
    }
    if (val.IsHolding<std::string>()) {
        // TODO: support NO_VALUE here?
        // *valueAttr = FnKat::NullAttribute();
        // *inputTypeAttr = FnKat::NullAttribute();
        *valueAttr = FnKat::StringAttribute(val.Get<std::string>());
        *inputTypeAttr = FnKat::StringAttribute("string");
        // Leave elementSize empty.
        return;
    }
    if (val.IsHolding<GfVec2f>()) {
        if (_KTypeAndSizeFromUsdVec2(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const GfVec2f rawVal = val.Get<GfVec2f>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<GfVec2d>()) {
        if (_KTypeAndSizeFromUsdVec2(roleName, "double",
                                     inputTypeAttr, elementSizeAttr)){
            const GfVec2d rawVal = val.Get<GfVec2d>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<GfVec3f>()) {
        if (_KTypeAndSizeFromUsdVec3(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const GfVec3f rawVal = val.Get<GfVec3f>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<GfVec4f>()) {
        if (_KTypeAndSizeFromUsdVec4(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const GfVec4f rawVal = val.Get<GfVec4f>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<GfVec2f>()) {
        if (_KTypeAndSizeFromUsdVec2(roleName, inputTypeAttr, elementSizeAttr)){
            const GfVec2f rawVal = val.Get<GfVec2f>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<GfVec3d>()) {
        if (_KTypeAndSizeFromUsdVec3(roleName, "double",
                                     inputTypeAttr, elementSizeAttr)){
            const GfVec3d rawVal = val.Get<GfVec3d>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<GfVec4d>()) {
        if (_KTypeAndSizeFromUsdVec4(roleName, "double",
                                     inputTypeAttr, elementSizeAttr)){
            const GfVec4d rawVal = val.Get<GfVec4d>();
            *valueAttr = VtKatanaCopy(rawVal);
        }
        return;
    }
    // XXX: Should matrices also be brought in as doubles?
    // What implications does this have? xform.matrix is handled explicitly as
    // a double, and apparently we don't use GfMatrix4f.
    // Shader parameter floats might expect a float matrix?
    if (val.IsHolding<GfMatrix4d>()) {
        const GfMatrix4d rawVal = val.Get<GfMatrix4d>();
        FnKat::FloatBuilder builder(/* tupleSize = */ 16);
        std::vector<float> vec;
        vec.resize(16);
        for (int i=0; i < 4; ++i) {
            for (int j=0; j < 4; ++j) {
                vec[i*4+j] = static_cast<float>(rawVal[i][j]);
            }
        }
        builder.set(vec);
        *valueAttr = builder.build();
        *inputTypeAttr = FnKat::StringAttribute("matrix16");
        // Leave elementSize empty.
        return;
    }

    if (val.IsHolding<VtArray<GfHalf> >()) {
        if (_KTypeAndSizeFromUsdVec3(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfHalf> rawVal = val.Get<VtArray<GfHalf> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }

    if (val.IsHolding<VtFloatArray>()) {
        const VtFloatArray rawVal = val.Get<VtFloatArray>();
        *valueAttr = VtKatanaMapOrCopy(rawVal);
        *inputTypeAttr = FnKat::StringAttribute("float");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    if (val.IsHolding<VtDoubleArray>()) {
        const VtDoubleArray rawVal = val.Get<VtDoubleArray>();
        *valueAttr = VtKatanaMapOrCopy(rawVal);
        *inputTypeAttr = FnKat::StringAttribute("double");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    // XXX: Should matrices also be brought in as doubles?
    // What implications does this have? xform.matrix is handled explicitly as
    // a double, and apparently we don't use GfMatrix4f.
    // Shader parameter floats might expect a float matrix?
    if (val.IsHolding<VtArray<GfMatrix4d> >()) {
        const VtArray<GfMatrix4d> rawVal = val.Get<VtArray<GfMatrix4d> >();
        std::vector<float> vec;
        TF_FOR_ALL(mat, rawVal) {
            for (int i=0; i < 4; ++i) {
                for (int j=0; j < 4; ++j) {
                    vec.push_back( static_cast<float>((*mat)[i][j]) );
                }
            }
        }
        FnKat::FloatBuilder builder(/* tupleSize = */ 16);
        builder.set(vec);
        *valueAttr = builder.build();
        *inputTypeAttr = FnKat::StringAttribute("matrix16");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec2f> >()) {
        if (_KTypeAndSizeFromUsdVec2(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec2f> rawVal = val.Get<VtArray<GfVec2f> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec2d> >()) {
        if (_KTypeAndSizeFromUsdVec2(roleName, "double",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec2d> rawVal = val.Get<VtArray<GfVec2d> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec3h> >()) {
        if (_KTypeAndSizeFromUsdVec3(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec3h> rawVal = val.Get<VtArray<GfVec3h> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec3f> >()) {
        if (_KTypeAndSizeFromUsdVec3(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec3f> rawVal = val.Get<VtArray<GfVec3f> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec3d> >()) {
        if (_KTypeAndSizeFromUsdVec3(roleName, "double",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec3d> rawVal = val.Get<VtArray<GfVec3d> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec4f> >()) {
        if (_KTypeAndSizeFromUsdVec4(roleName, "float",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec4f> rawVal = val.Get<VtArray<GfVec4f> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<GfVec4d> >()) {
        if (_KTypeAndSizeFromUsdVec4(roleName, "double",
                                     inputTypeAttr, elementSizeAttr)){
            const VtArray<GfVec4d> rawVal = val.Get<VtArray<GfVec4d> >();
            *valueAttr = VtKatanaMapOrCopy(rawVal);
        }
        return;
    }
    if (val.IsHolding<VtArray<int> >()) {
        const VtArray<int> rawVal = val.Get<VtArray<int> >();
        *valueAttr = VtKatanaMapOrCopy(rawVal);
        *inputTypeAttr = FnKat::StringAttribute("int");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    if (val.IsHolding<VtArray<unsigned> >()) {
        // Lossy translation of array<unsigned> to array<int>
        // No warning is printed as they obscure more important warnings
        const VtArray<unsigned> rawVal = val.Get<VtArray<unsigned> >();
        *valueAttr = VtKatanaMapOrCopy(rawVal);
        *inputTypeAttr = FnKat::StringAttribute("unsigned");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    if (val.IsHolding<VtArray<long> >()) {
        // Lossy translation of array<long> to array<int>
        // No warning is printed as they obscure more important warnings
        const VtArray<long> rawVal = val.Get<VtArray<long> >();
        *valueAttr = VtKatanaMapOrCopy(rawVal);
        *inputTypeAttr = FnKat::StringAttribute("long");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    if (val.IsHolding<VtArray<std::string> >()) {
        const VtArray<std::string> rawVal = val.Get<VtArray<std::string> >();
        *valueAttr = VtKatanaMapOrCopy(rawVal);
        *inputTypeAttr = FnKat::StringAttribute("string");
        if (elementSize > 1) {
            *elementSizeAttr = FnKat::IntAttribute(elementSize);
        }
        return;
    }
    if (val.IsHolding<TfToken>())
    {
        *valueAttr = FnKat::StringAttribute(val.Get<TfToken>().GetString());
        *inputTypeAttr = FnKat::StringAttribute("string");
        return;
    }

    TF_WARN("Unsupported primvar value type: %s",
            ArchGetDemangled(val.GetTypeid()).c_str());
}

std::string UsdKatanaUtils::GenerateShadingNodeHandle(const UsdPrim& shadingNode)
{
    std::string name;
    for (UsdPrim curr = shadingNode;
            curr && (
                curr == shadingNode ||
                curr.IsA<UsdGeomScope>());
            curr = curr.GetParent()) {
        name = curr.GetName().GetString() + name;
    }

    return name;
}

void
_FindCameraPaths_Traversal( const UsdPrim &prim, SdfPathVector *result )
{
    // Recursively traverse model hierarchy for camera prims.
    // Note 1: this requires that either prim types be lofted above
    //         payloads for all model references, or that models be loaded.
    // Note 2: Obviously, we will not find cameras embedded within models.
    //         We have made this restriction consciously to reduce the
    //         latency of camera-enumeration

    // If set, this allows for better traversal for global attributes (camera list and light lists)
    // by utilizing USD Prim children filters to check for prims in the model hierarchy only,
    // rather than the default Prim child traversal.
    static const bool traverseModelHierarchyOnly =
        TfGetenvBool("KATANA_USD_GLOBALS_TRAVERSE_MODEL_HIERARCHY", true);
    auto flags = UsdPrimDefaultPredicate;
    if (traverseModelHierarchyOnly)
    {
        flags = flags && UsdPrimIsModel;
    }
    TF_FOR_ALL(child, prim.GetFilteredChildren(flags)) {
        if (child->IsA<UsdGeomCamera>()) {
            result->push_back(child->GetPath());
        }
        _FindCameraPaths_Traversal(*child, result);
    }
}

SdfPathVector UsdKatanaUtils::FindCameraPaths(const UsdStageRefPtr& stage)
{
    SdfPathVector result;
    _FindCameraPaths_Traversal( stage->GetPseudoRoot(), &result );
    return result;
}

// This works like UsdLuxListAPI::ComputeLightList() except it tries to
// maintain the order discovered during traversal.
static void
_Traverse(const UsdPrim &prim,
          UsdLuxListAPI::ComputeMode mode,
          std::set<SdfPath, SdfPath::FastLessThan> &seen,
          SdfPathVector *lights)
{
    if (!prim)
        return;

    // If requested, check lightList cache.
    if (mode == UsdLuxListAPI::ComputeModeConsultModelHierarchyCache &&
        prim.GetPath().IsPrimPath() /* no cache on pseudoRoot */) {
        UsdLuxListAPI listAPI(prim);
        TfToken cacheBehavior;
        if (listAPI.GetLightListCacheBehaviorAttr().Get(&cacheBehavior)) {
            if (cacheBehavior == UsdLuxTokens->consumeAndContinue ||
                cacheBehavior == UsdLuxTokens->consumeAndHalt) {
                // Check stored lightList.
                UsdRelationship rel = listAPI.GetLightListRel();
                SdfPathVector targets;
                rel.GetForwardedTargets(&targets);
                for (const auto& target: targets) {
                    if (seen.insert(target).second) {
                        lights->push_back(target);
                    }
                }
                if (cacheBehavior == UsdLuxTokens->consumeAndHalt) {
                    return;
                }
            }
        }
    }
    // Accumulate discovered prims.
    if (prim.HasAPI<UsdLuxLightAPI>() || prim.IsA<UsdLuxLightFilter>() || prim.GetTypeName() == "Light") {
        if (seen.insert(prim.GetPath()).second) {
            lights->push_back(prim.GetPath());
        }
    }
    // Traverse descendants.
    auto flags = UsdPrimIsActive && !UsdPrimIsAbstract && UsdPrimIsDefined;

    // If set, this allows for better traversal for global attributes (camera list and light lists)
    // by utilizing USD Prim children filters to check for prims in the model hierarchy only,
    // rather than the default Prim child traversal.
    static const bool traverseModelHierarchyOnly =
        TfGetenvBool("KATANA_USD_GLOBALS_TRAVERSE_MODEL_HIERARCHY", true);
    if (traverseModelHierarchyOnly && mode == UsdLuxListAPI::ComputeModeConsultModelHierarchyCache)
    {
        // When consulting the cache we only traverse model hierarchy.
        flags = flags && UsdPrimIsModel;
    }
    else
    {
        flags = flags && UsdPrimIsLoaded;
    }
    for (const UsdPrim &child:
             prim.GetFilteredChildren(UsdTraverseInstanceProxies(flags))) {
        _Traverse(child, mode, seen, lights);
    }
}

SdfPathVector UsdKatanaUtils::FindLightPaths(const UsdStageRefPtr& stage)
{
/* XXX -- ComputeLightList() doesn't try to maintain an order.  That
          should be okay for lights but it does cause differences in
          the Katana lightList and generated RIB.  These differences
          should have no effect on a render but they do make it more
          difficult to compare RIB before and after a round-trip
          through USD so, for now, we maintain the order found during
          traversal.  If we switch to using this code then this method
          should return an SdfPathSet and _Traverse() is unnecessary.
    SdfPathSet allLights;
    for (const auto &child: stage->GetPseudoRoot().GetChildren()) {
        SdfPathSet lights = UsdLuxListAPI(child).ComputeLightList(
            UsdLuxListAPI::ComputeModeConsultModelHierarchyCache);
        allLights.insert(lights.begin(), lights.end());
    }
    return allLights;
*/
    SdfPathVector result;
    std::set<SdfPath, SdfPath::FastLessThan> seen;
    for (const auto &child: stage->GetPseudoRoot().GetChildren()) {
        _Traverse(child, UsdLuxListAPI::ComputeModeConsultModelHierarchyCache,
                  seen, &result);
    }
    return result;
}

std::string UsdKatanaUtils::ConvertUsdPathToKatLocation(const SdfPath& path,
                                                        const std::string& isolatePathString,
                                                        const std::string& rootPathString,
                                                        const std::string& sessionPathString,
                                                        bool allowOutsideIsolation)
{
    if (!TF_VERIFY(path.IsAbsolutePath())) {
        return std::string();
    }

    // Convert to the corresponding katana location by stripping
    // off the leading rootPath and prepending rootLocation.
    //
    // absolute path: starts with '/'
    std::string pathString = path.GetString();
    if (!isolatePathString.empty()) {
        if (pathString.size() == 1 && pathString[0] == '/')
        {
            return rootPathString;
        }
        if (pathString.rfind(isolatePathString, 0) == 0) {
            pathString = pathString.substr(isolatePathString.size());
        } else {
            // no good guess about the katana target location:
            //   isolatePath is not a prefix of the prim being cooked
            if (allowOutsideIsolation) {
                // So we are returning the path using the session location
                // For materials.
                if (sessionPathString.empty() && pathString.empty()) {
                    return "/";
                }
                return sessionPathString + pathString;
            } else {
                FnLogWarn(
                    "UsdIn: Failed to compute katana path for"
                    " usd path: "
                    << path << " with given isolatePath: " << isolatePathString);
                return std::string();
            }
        }
    }

    // The rootPath is expected to be an absolute path or empty string.
    //
    // minimum expected path is '/'
    if (rootPathString.empty() && pathString.empty()) {
        return "/";
    }

    std::string resultKatanaLocation = rootPathString;
    resultKatanaLocation += pathString;

    return resultKatanaLocation;
}

std::string UsdKatanaUtils::ConvertUsdPathToKatLocation(const SdfPath& path,
                                                        const UsdKatanaUsdInArgsRefPtr& usdInArgs,
                                                        bool allowOutsideIsolation)
{
    return ConvertUsdPathToKatLocation(path, usdInArgs->GetIsolatePath(),
                                       usdInArgs->GetRootLocationPath(),
                                       usdInArgs->GetSessionLocationPath(),
                                       allowOutsideIsolation);
}

std::string UsdKatanaUtils::ConvertUsdPathToKatLocation(const SdfPath& path,
                                                        const UsdKatanaUsdInPrivateData& data,
                                                        bool allowOutsideIsolation)
{
    if (!TF_VERIFY(path.IsAbsolutePath())) {
        return std::string();
    }

    // If the current prim is in a prototype for the sake of processing
    // an instance, replace the prototype path by the instance path before
    // converting to a katana location.
    SdfPath nonPrototypePath = path;
    if (data.GetUsdPrim().IsInPrototype() && !data.GetInstancePath().IsEmpty())
    {
        nonPrototypePath =
            nonPrototypePath.ReplacePrefix(data.GetPrototypePath(), data.GetInstancePath());
    }

    return ConvertUsdPathToKatLocation(
        nonPrototypePath, data.GetUsdInArgs(), allowOutsideIsolation);
}

std::string UsdKatanaUtils::_GetDisplayName(const UsdPrim& prim)
{
    std::string primName = prim.GetName();
    UsdUISceneGraphPrimAPI sgp(prim);
    UsdAttribute displayNameAttr = sgp.GetDisplayNameAttr();
    if (displayNameAttr.IsValid() && !UsdKatana_IsAttrValFromBaseMaterial(displayNameAttr) &&
        !UsdKatana_IsAttrValFromDirectReference(displayNameAttr))
    {
        // override prim name
        TfToken displayNameToken;
        if (displayNameAttr.Get(&displayNameToken)) {
            primName = displayNameToken.GetString();
        }
        else {
            displayNameAttr.Get(&primName);
        }
    }
    else
    {
        UsdAttribute primNameAttr = UsdKatanaLookAPI(prim).GetPrimNameAttr();
        if (primNameAttr.IsValid() && !UsdKatana_IsAttrValFromBaseMaterial(primNameAttr) &&
            !UsdKatana_IsAttrValFromDirectReference(primNameAttr))
        {
            primNameAttr.Get(&primName);
        }
    }
    return primName;
}

std::string UsdKatanaUtils::_GetDisplayGroup(const UsdPrim& prim, const SdfPath& path)
{
    std::string displayGroup;
    UsdUISceneGraphPrimAPI sgp(prim);

    UsdAttribute displayGroupAttr = sgp.GetDisplayGroupAttr();
    if (displayGroupAttr.IsValid() && !UsdKatana_IsAttrValFromBaseMaterial(displayGroupAttr) &&
        !UsdKatana_IsAttrValFromDirectReference(displayGroupAttr))
    {
        TfToken displayGroupToken;
        if (displayGroupAttr.Get(&displayGroupToken)) {
            displayGroup = displayGroupToken.GetString();
        }
        else {
            displayGroupAttr.Get(&displayGroup);
        }
        displayGroup = TfStringReplace(displayGroup, ":", "/");
    }

    if (displayGroup.empty())
    {
        // calculate from basematerial
        SdfPath parentPath;

        UsdShadeMaterial materialSchema = UsdShadeMaterial(prim);
        if (materialSchema.HasBaseMaterial()) {
            // This base material is defined as a derivesFrom relationship
            parentPath = materialSchema.GetBaseMaterialPath();
        }

        UsdPrim parentPrim =
            prim.GetStage()->GetPrimAtPath(parentPath);

        // Asset sanity check. It is possible the derivesFrom relationship
        // for a Look exists but references a non-existent location. If so,
        // simply return the base path.
        if (!parentPrim) {
            return "";
        }

        if (parentPrim.IsInPrototype())
        {
            // If the prim is inside a prototype, then attempt to translate the
            // parentPath to the corresponding uninstanced path, assuming that
            // the given forwarded path and parentPath belong to the same prototype
            const SdfPath primPath = prim.GetPath();
            std::pair<SdfPath, SdfPath> prefixPair =
                primPath.RemoveCommonSuffix(path);
            const SdfPath& prototypePath = prefixPair.first;
            const SdfPath& instancePath = prefixPair.second;

            // XXX: Assuming that the base look (parent) path belongs to the
            // same prototype! If it belongs to a different prototype, we don't have
            //  the context needed to resolve it.
            if (parentPath.HasPrefix(prototypePath))
            {
                parentPath = instancePath.AppendPath(
                    parentPath.ReplacePrefix(prototypePath, SdfPath::ReflexiveRelativePath()));
            }
            else
            {
                FnLogWarn("Error converting UsdMaterial path <" <<
                    path.GetString() <<
                    "> to katana location: could not map parent path <" <<
                    parentPath.GetString() << "> to uninstanced location.");
                return "";
            }
        }
        // displayGroup coming from the parent includes the materialGroup
        std::string parentDisplayName = _GetDisplayName(parentPrim);
        std::string parentDisplayGroup = _GetDisplayGroup(
            parentPrim,
            parentPath);

        if (parentDisplayGroup.empty()) {
            displayGroup = parentDisplayName;
        }
        else {
            displayGroup = parentDisplayGroup;
            displayGroup += '/';
            displayGroup += parentDisplayName;
        }
    }

    return displayGroup;
}

std::string UsdKatanaUtils::ConvertUsdMaterialPathToKatLocation(
    const SdfPath& path,
    const UsdKatanaUsdInPrivateData& data)
{
    std::string returnValue = "/" + path.GetName();

    // calculate the material group. It can be either "/" or an absolute
    // path (no trailing '/')
    std::string materialGroupKatanaPath =
        ConvertUsdPathToKatLocation(path.GetParentPath(), data, true);

    UsdPrim prim =
        UsdUtilsGetPrimAtPathWithForwarding(
            data.GetUsdInArgs()->GetStage(), path);

    // LooksDerivedStructure is legacy
    bool isLibrary = (materialGroupKatanaPath == "/" ||
        materialGroupKatanaPath == "/LooksDerivedStructure");

    if (isLibrary) {
        // materials are at the root: we are in a library
        if (!prim.IsValid()) {
            // failed
            return returnValue;
        }
    }
    else {
        // the parent of this material is a material group
        // apply prim name only if
        returnValue = materialGroupKatanaPath;
        if (returnValue != "/") {
            returnValue += '/';
        }
        returnValue += path.GetName();

        if (!prim.IsValid()) {
            return returnValue;
        }
    }

    returnValue = materialGroupKatanaPath;
    if (returnValue != "/") {
        returnValue += '/';
    }

    std::string displayGroup = _GetDisplayGroup(prim, path);
    if (!displayGroup.empty()) {
        returnValue += displayGroup;
        returnValue += '/';
    }

    std::string primName = _GetDisplayName(prim);
    returnValue += primName;
    return returnValue;
}

bool UsdKatanaUtils::ModelGroupIsAssembly(const UsdPrim& prim)
{
    if (!(prim.IsGroup() && prim.GetParent()) || prim.IsInPrototype())
        return false;

    // XXX with bug/102670, this test will be trivial: prim.IsAssembly()
    TfToken kind;

    if (!UsdModelAPI(prim).GetKind(&kind)){
        TF_WARN("Expected to find authored kind on prim <%s>",
                prim.GetPath().GetText());
        return false;
    }

    return KindRegistry::IsA(kind, KindTokens->assembly) ||
           UsdKatanaUtils::ModelGroupNeedsProxy(prim);
}

FnKat::GroupAttribute UsdKatanaUtils::GetViewerProxyAttr(double currentTime,
                                                         const std::string& fileName,
                                                         const std::string& referencePath,
                                                         const std::string& rootLocation,
                                                         FnAttribute::GroupAttribute sessionAttr,
                                                         const std::string& ignoreLayerRegex)
{
    FnKat::GroupBuilder proxiesBuilder;

    proxiesBuilder.set("viewer.load.opType",
        FnKat::StringAttribute("StaticSceneCreate"));

    proxiesBuilder.set("viewer.load.opArgs.a.type",
        FnKat::StringAttribute("usd"));

    proxiesBuilder.set("viewer.load.opArgs.a.currentTime",
        FnKat::DoubleAttribute(currentTime));

    proxiesBuilder.set("viewer.load.opArgs.a.fileName",
        FnKat::StringAttribute(fileName));

    proxiesBuilder.set("viewer.load.opArgs.a.forcePopulateUsdStage",
        FnKat::FloatAttribute(1));

    // XXX: Once everyone has switched to the op, change referencePath
    // to isolatePath here and in the USD VMP (2/25/2016).
    proxiesBuilder.set("viewer.load.opArgs.a.referencePath",
        FnKat::StringAttribute(referencePath));

    proxiesBuilder.set("viewer.load.opArgs.a.rootLocation",
        FnKat::StringAttribute(rootLocation));

    proxiesBuilder.set("viewer.load.opArgs.a.session", sessionAttr);

    proxiesBuilder.set("viewer.load.opArgs.a.ignoreLayerRegex",
            FnKat::StringAttribute(ignoreLayerRegex));

    return proxiesBuilder.build();
}

FnKat::GroupAttribute UsdKatanaUtils::GetViewerProxyAttr(const UsdKatanaUsdInPrivateData& data)
{
    return GetViewerProxyAttr(
            data.GetCurrentTime(),
            data.GetUsdInArgs()->GetFileName(),
            data.GetUsdPrim().GetPath().GetString(),
            data.GetUsdInArgs()->GetRootLocationPath(),
            data.GetUsdInArgs()->GetSessionAttr(),
            data.GetUsdInArgs()->GetIgnoreLayerRegex());
}

bool UsdKatanaUtils::PrimIsSubcomponent(const UsdPrim& prim)
{
    // trying to make this early exit for leaf geometry.
    // unfortunately there's no good IsXXX() method to test
    // for subcomponents -- they aren't Models or Groups --
    // but they do have Payloads.
    if (!(prim.HasAuthoredPayloads() && prim.GetParent()))
        return false;

    // XXX(spiff) with bug/102670, this test will be trivial: prim.IsAssembly()
    TfToken kind;

    if (!UsdModelAPI(prim).GetKind(&kind)){
        TF_WARN("Expected to find authored kind on prim <%s>",
                prim.GetPath().GetText());
        return false;
    }

    return KindRegistry::IsA(kind, KindTokens->subcomponent);
}

bool UsdKatanaUtils::ModelGroupNeedsProxy(const UsdPrim& prim)
{
    // No proxy if group-to-assembly promotion is explicitly suppressed.
    bool suppressProxy;
    if (UsdKatanaBlindDataObject(prim)
            .GetSuppressGroupToAssemblyPromotionAttr()
            .Get(&suppressProxy) && suppressProxy) {
        return false;
    }

    // Check to see if all children are not group models, if so, we'll make
    // this an assembly as a load/proxy optimization.
    TF_FOR_ALL(childIt, prim.GetChildren()) {
        if (childIt->IsGroup())
            return false;
    }

    return true;
}

bool UsdKatanaUtils::IsModelAssemblyOrComponent(const UsdPrim& prim)
{
    if (!prim || !prim.IsModel() || prim.IsInPrototype()) {
        return false;
    }

    {
        // handle cameras as they are not "assembly" or "component" to katana.
        if (prim.IsA<UsdGeomCamera>()) {
            return false;
        }

        // XXX: A prim whose kind *equals* "group" should never be
        // considered an assembly or component
        // http://bugzilla.pixar.com/show_bug.cgi?id=106971#c1
        TfToken kind;
        if (!UsdModelAPI(prim).GetKind(&kind)){
            TF_WARN("Expected to find authored kind on prim <%s>",
                    prim.GetPath().GetText());
            return false;
        }
        if (kind == KindTokens->group) {
            return false;
        }
    }

    // XXX: We'll be able to implement all of this in a
    // much more clear way in the future.  for now, just check if it has this
    // authored metadata.
    // XXX: coming with bug/102670
    if (prim.HasAuthoredMetadata(TfToken("references"))) {
        return true;
    }

    return false;
}

static const std::unordered_map<std::string, std::string> s_rendererToContextName{{"prman", "ri"},
                                                                                  {"nsi", "dl"}};
static const std::unordered_map<std::string, std::string> s_contextNameToRenderer{{"ri", "prman"},
                                                                                  {"dl", "nsi"}};

void UsdKatanaUtils::ShaderToAttrsBySdr(const UsdPrim& prim,
                                        const std::string& shaderName,
                                        const UsdTimeCode& currentTimeCode,
                                        FnAttribute::GroupBuilder& attrs)
{
    std::vector<std::string> idSplit = TfStringSplit(shaderName, ":");
    if (idSplit.size() != 2)
    {
        return;
    }

    std::string shaderPrefix = idSplit[0];
    const std::string& shaderId = idSplit[1];

    SdrShaderNodeConstPtr sdrNode = GetShaderNodeFromShaderId(shaderName);

    if (!sdrNode)
    {
        FnLogWarn("No Sdr shader found for " << shaderId);
        return;
    }

    UsdKatanaAttrMap shaderBuilder;
    shaderBuilder.SetUSDTimeCode(currentTimeCode);
    const std::string& shaderContext = sdrNode->GetContext().GetString();

    for (const auto& inputNameToken : sdrNode->GetInputNames())
    {
        // This block is for building up a vector of potential attribute names
        // (potentialUsdAttributeNames) inside the usd prim being read. Katana supports having
        // multiple light shaders with differing values for the same attribute on the same
        // location. In USD, the  `inputs:color` attribute would set the color for any applied
        // renderer light schemas but we allow these attributes to be namespaced, so
        // `inputs:ri:light:color` would set the color just for a prman light inside Katana
        // at the light location, while leaving the basic USD Lux light color to be set by
        // `inputs:color`.
        std::vector<std::string> potentialUsdAttributeNames;
        const std::string& inputName = inputNameToken.GetString();
        potentialUsdAttributeNames.reserve(4);

        const auto& rendererNameMappingIt = s_rendererToContextName.find(shaderPrefix);
        shaderPrefix = rendererNameMappingIt != s_rendererToContextName.end()
                           ? rendererNameMappingIt->second
                           : shaderPrefix;

        // Build a common renderer-specific namespace prefix for the attribute.
        std::string entryPrefix = shaderPrefix + ":";
        if (!shaderContext.empty())
        {
            entryPrefix += shaderContext + ":";
        }

        // Here, for a prman light shader we would expect `entryPrefix` to be `ri:light:`.
        // If this prefix is not already applied as a potential attribute name, add it first
        // as this is the attribute we want to prioritise for reading the imported value.
        if (inputName.rfind(entryPrefix, 0) != 0)
        {
            potentialUsdAttributeNames.insert(potentialUsdAttributeNames.begin(),
                                              entryPrefix + inputName);
        }
        // If this prefix is not already applied as a potential attribute name, including,
        // the "inputs:" prefix, add it.
        if (inputName.rfind("inputs:" + entryPrefix, 0) != 0)
        {
            // We do not want to add the whole namespace prefix along with the "inputs:"
            // prefix if it is already a part of the inputName already.
            if (inputName.rfind(entryPrefix, 0) != 0)
            {
                potentialUsdAttributeNames.emplace_back("inputs:" + entryPrefix + inputName);
            }
            else
            {
                potentialUsdAttributeNames.emplace_back("inputs:" + inputName);
            }
        }
        // The last attributes we would want to import from are the basic non-namespaced
        // versions.
        potentialUsdAttributeNames.emplace_back("inputs:" + inputName);
        potentialUsdAttributeNames.emplace_back(inputName);

        std::string usdAttributeName;
        for (const std::string& potentialUsdAttributeName : potentialUsdAttributeNames)
        {
            if (prim.HasAttribute(TfToken(potentialUsdAttributeName)))
            {
                usdAttributeName = potentialUsdAttributeName;
                break;
            }
        }

        if (!usdAttributeName.empty())
        {
            // Use implementation name instead of input name for Katana attributes
            // for cases like color vs lightColor
            const SdrShaderProperty* input = sdrNode->GetShaderInput(inputNameToken);
            if (!input)
            {
                continue;
            }

            shaderBuilder.Set(input->GetImplementationName(),
                              prim.GetAttribute(TfToken(usdAttributeName)));
        }
    }

    auto rendererItr = s_contextNameToRenderer.find(shaderPrefix);
    shaderPrefix =
        rendererItr != s_contextNameToRenderer.end() ? rendererItr->second : shaderPrefix;
    const std::string shaderContextCased = TfStringCapitalize(shaderContext);
    attrs.set(shaderPrefix + shaderContextCased + "Shader", FnKat::StringAttribute(shaderId));
    attrs.set(shaderPrefix + shaderContextCased + "Params", shaderBuilder.build());
}

std::unordered_set<std::string> UsdKatanaUtils::GetShaderIds(const UsdPrim& prim,
                                                             const UsdTimeCode& currentTimeCode)
{
    UsdKatanaKatanaLightAPI katanaLightAPI(prim);
    std::unordered_set<std::string> shaderIds;

    // Gather light shader ids from the LightAPI shaderId attribute.
    for (auto& attr : prim.GetAttributes())
    {
        if (TfStringEndsWith(attr.GetName(), "light:shaderId") ||
            TfStringEndsWith(attr.GetName(), "lightFilter:shaderId"))
        {
            VtValue shaderIdVal;
            attr.Get(&shaderIdVal);
            if (shaderIdVal.IsHolding<TfToken>())
            {
                std::vector<std::string> attrNameSplit = TfStringSplit(attr.GetName(), ":");
                // If the size is greater than 2, it's a namespaced shader id,
                // e.g. ri:light:shaderId.
                if (attrNameSplit.size() > 2)
                {
                    std::string shaderId = attrNameSplit[0];
                    shaderId.append(":" + shaderIdVal.UncheckedGet<TfToken>().GetString());
                    shaderIds.insert(shaderId);
                }
                else
                {
                    std::string shaderId = shaderIdVal.UncheckedGet<TfToken>().GetString();
                    if (shaderId.empty())
                    {
                        continue;
                    }
                    shaderId.insert(0, "usd:");
                    shaderIds.insert(shaderId);
                }
            }
        }
    }

    // Gather light shader ids from the KatanaLightAPI shaderId attribute.
    VtValue lightShaderIdsVal;
    katanaLightAPI.GetIdAttr().Get(&lightShaderIdsVal, currentTimeCode);
    if (!lightShaderIdsVal.IsEmpty() && lightShaderIdsVal.IsHolding<VtArray<std::string>>())
    {
        for (const std::string& shaderId : lightShaderIdsVal.UncheckedGet<VtArray<std::string>>())
        {
            shaderIds.insert(shaderId);
        }
    }

    return shaderIds;
}

SdrShaderNodeConstPtr UsdKatanaUtils::GetShaderNodeFromShaderId(const std::string& shaderName)
{
    SdrRegistry& sdrRegistry = SdrRegistry::GetInstance();
    std::vector<std::string> idSplit = TfStringSplit(shaderName, ":");
    if (idSplit.size() != 2)
    {
        return nullptr;
    }

    const std::string& shaderId = idSplit[1];

    SdrShaderNodeConstPtr sdrNode = sdrRegistry.GetShaderNodeByIdentifier(TfToken(shaderId));
    if (!sdrNode)
    {
        sdrNode =
            sdrRegistry.GetShaderNodeByName(TfToken(shaderId), {}, NdrVersionFilterAllVersions);
    }
    if (!sdrNode)
    {
        FnLogWarn("No Sdr shader found for " << shaderId);
        return nullptr;
    }

    return sdrNode;
}

bool UsdKatanaUtils::IsAttributeVarying(const UsdAttribute& attr, double currentTime)
{
    // XXX: Copied from UsdImagingDelegate::_TrackVariability.
    // XXX: This logic is highly sensitive to the underlying quantization of
    //      time. Also, the epsilon value (.000001) may become zero for large
    //      time values.
    double lower, upper, queryTime;
    bool hasSamples;
    queryTime = currentTime + 0.000001;
    // TODO: migrate this logic into UsdAttribute.
    if (attr.GetBracketingTimeSamples(queryTime, &lower, &upper, &hasSamples)
        && hasSamples)
    {
        // The potential results are:
        //    * Requested time was between two time samples
        //    * Requested time was out of the range of time samples (lesser)
        //    * Requested time was out of the range of time samples (greater)
        //    * There was a time sample exactly at the requested time or
        //      there was exactly one time sample.
        // The following logic determines which of these states we are in.

        // Between samples?
        if (lower != upper) {
            return true;
        }

        // Out of range (lower) or exactly on a time sample?
        attr.GetBracketingTimeSamples(lower+.000001,
                                      &lower, &upper, &hasSamples);
        if (lower != upper) {
            return true;
        }

        // Out of range (greater)?
        attr.GetBracketingTimeSamples(lower-.000001,
                                      &lower, &upper, &hasSamples);
        if (lower != upper) {
            return true;
        }
        // Really only one time sample --> not varying for our purposes
    }
    return false;
}

std::string UsdKatanaUtils::GetModelInstanceName(const UsdPrim& prim)
{
    if (!prim) {
        return std::string();
    }

    if (prim.GetPath() == SdfPath::AbsoluteRootPath()) {
        return std::string();
    }

    if (UsdAttribute attr =
        UsdRiStatementsAPI(prim).GetRiAttribute(TfToken("ModelInstance"))) {
        std::string modelInstanceName;
        if (attr.Get(&modelInstanceName)) {
            return modelInstanceName;
        }
    }

    if (UsdKatanaUtils::IsModelAssemblyOrComponent(prim))
    {
        return prim.GetName();
    }

    // Recurse to namespace parents so we can find the enclosing model
    // instance.  (Note that on the katana side, the modelInstanceName
    // attribute inherits.)
    //
    // XXX tools OM is working on a much more clear future way to handle
    // this, but until then we recurse upwards.
    return UsdKatanaUtils::GetModelInstanceName(prim.GetParent());
}

std::string UsdKatanaUtils::GetAssetName(const UsdPrim& prim)
{
    bool isPseudoRoot = prim.GetPath() == SdfPath::AbsoluteRootPath();

    if (isPseudoRoot)
        return std::string();

    UsdModelAPI model(prim);
    std::string assetName;
    if (model.GetAssetName(&assetName)) {
        if (!assetName.empty())
            return assetName;
    }

    return prim.GetName();
}

bool UsdKatanaUtils::IsBoundable(const UsdPrim& prim)
{
    if (!prim)
        return false;

    if (prim.IsModel() && ((!prim.IsGroup()) || UsdKatanaUtils::ModelGroupIsAssembly(prim)))
        return true;

    if (UsdKatanaUtils::PrimIsSubcomponent(prim))
        return true;

    return prim.IsA<UsdGeomBoundable>();
}

FnKat::DoubleAttribute UsdKatanaUtils::ConvertBoundsToAttribute(
    const std::vector<GfBBox3d>& bounds,
    const std::vector<double>& motionSampleTimes,
    const bool isMotionBackward,
    bool* hasInfiniteBounds)
{
    FnKat::DoubleBuilder boundBuilder(6);

    // There must be one bboxCache per motion sample, for efficiency purposes.
    if (!TF_VERIFY(bounds.size() == motionSampleTimes.size())) {
        return FnKat::DoubleAttribute();
    }

    for (size_t i = 0; i < motionSampleTimes.size(); i++) {
        const GfBBox3d& bbox = bounds[i];

        double relSampleTime = motionSampleTimes[i];

        const GfRange3d range = bbox.ComputeAlignedBox();
        const GfVec3d& min = range.GetMin();
        const GfVec3d& max = range.GetMax();

        // Don't return empty bboxes, Katana/PRMan will not behave well.
        if (range.IsEmpty()) {
            // FnLogWarn(TfStringPrintf(
            //     "Failed to compute bound for <%s>",
            //      prim.GetPath().GetText()));
            return FnKat::DoubleAttribute();
        }

        if ( std::isinf(min[0]) || std::isinf(min[1]) || std::isinf(min[2]) ||
            std::isinf(max[0]) || std::isinf(max[1]) || std::isinf(max[2]) ) {
            *hasInfiniteBounds = true;
        }

        std::vector<double>& boundData = boundBuilder.get(
            isMotionBackward ? UsdKatanaUtils::ReverseTimeSample(relSampleTime) : relSampleTime);

        boundData.push_back( min[0] );
        boundData.push_back( max[0] );
        boundData.push_back( min[1] );
        boundData.push_back( max[1] );
        boundData.push_back( min[2] );
        boundData.push_back( max[2] );
    }

    return boundBuilder.build();
}

namespace
{
    typedef std::map<std::string, std::string> StringMap;
    // A container that respects insertion order is needed; since the set is
    // not expected to grow large, std::vector is used. USD appears to not be
    // deterministic when generating the /__Prototype prims given the same
    // stage. I.e a prim with a prototype could point to /__Prototype_1
    // or /__Prototype_2 when reloading. This would cause issues as the order
    // of the set is used to create the instance sources, and if that can
    // change ordering because the comparison of /__Prototype_x changes, it
    // changes the resultant hierarchy.
    typedef std::vector<std::string> StringVec;
    typedef std::map<std::string, StringVec> StringVecMap;

    void _walkForPrototypes(const UsdPrim& prim,
                            StringMap& prototypeToKey,
                            StringVecMap& keyToPrototypes)
    {
        if (prim.IsInstance())
        {
            const UsdPrim prototype = prim.GetPrototype();

            if (prototype.IsValid())
            {
                std::string prototypePath = prototype.GetPath().GetString();

                if (prototypeToKey.find(prototypePath) == prototypeToKey.end())
                {
                    std::string assetName;
                    UsdModelAPI(prim).GetAssetName(&assetName);
                    if (assetName.empty())
                    {
                        assetName = "prototype";
                    }

                    std::ostringstream buffer;
                    buffer << assetName << "/variants";

                    UsdVariantSets variantSets = prim.GetVariantSets();

                    std::vector<std::string> names;
                    variantSets.GetNames(&names);
                    TF_FOR_ALL(I, names)
                    {
                        const std::string & variantName = (*I);
                        std::string variantValue =
                                variantSets.GetVariantSet(
                                        variantName).GetVariantSelection();
                        buffer << "__" << variantName << "_" << variantValue;
                    }

                    std::string key = buffer.str();
                    prototypeToKey[prototypePath] = key;
                    if (std::find(keyToPrototypes[key].begin(),
                                  keyToPrototypes[key].end(),
                                  prototypePath) == keyToPrototypes[key].end())
                    {
                        keyToPrototypes[key].push_back(prototypePath);
                    }
                    // TODO, Warn when there are multiple prototypes with the
                    //      same key.

                    _walkForPrototypes(prototype, prototypeToKey, keyToPrototypes);
                }
            }
        }


        TF_FOR_ALL(childIter, prim.GetFilteredChildren(
                UsdPrimIsDefined && UsdPrimIsActive && !UsdPrimIsAbstract))
        {
            const UsdPrim& child = *childIter;
            _walkForPrototypes(child, prototypeToKey, keyToPrototypes);
        }
    }
}

FnKat::GroupAttribute UsdKatanaUtils::BuildInstancePrototypeMapping(const UsdStageRefPtr& stage,
                                                                    const SdfPath& rootPath)
{
    StringMap prototypeToKey;
    StringVecMap keyToPrototypes;
    _walkForPrototypes(stage->GetPrimAtPath(rootPath), prototypeToKey, keyToPrototypes);

    FnKat::GroupBuilder gb;
    TF_FOR_ALL(I, keyToPrototypes)
    {
        const std::string & key = (*I).first;
        const StringVec& prototypes = (*I).second;

        size_t i = 0;

        TF_FOR_ALL(J, prototypes)
        {
            const std::string& prototype = (*J);

            std::ostringstream buffer;

            buffer << key << "/m" << i;
            gb.set(FnKat::DelimiterEncode(prototype), FnKat::StringAttribute(buffer.str()));

            ++i;
        }
    }


    return gb.build();
}

FnKat::Attribute UsdKatanaUtils::ApplySkinningToPoints(const UsdGeomPointBased& points,
                                                       const UsdKatanaUsdInPrivateData& data)
{
    static const int tupleSize = 3;
    FnKat::FloatAttribute skinnedPointsAttr;
    FnKat::DataBuilder<FnKat::FloatAttribute> attrBuilder(tupleSize);

    // Flag to check if we discovered the topology is varying, in
    // which case we only output the sample at the curent frame.
    bool varyingTopology = false;

    const double currentTime = data.GetCurrentTime();

    const bool isMotionBackward = data.IsMotionBackward();

    UsdSkelCache skelCache;
    const UsdPrim prim{points.GetPrim()};
    UsdSkelRoot skelRoot = UsdSkelRoot::Find(prim);
    if (!skelRoot)
    {
        return skinnedPointsAttr;
    }
    skelCache.Populate(skelRoot, UsdTraverseInstanceProxies());

    // Get skinning query
    const UsdSkelSkinningQuery skinningQuery = skelCache.GetSkinningQuery(prim);
    if (!skinningQuery)
    {
        return skinnedPointsAttr;
    }

    // Get skeleton query
    const UsdSkelSkeleton skel = UsdSkelBindingAPI(prim).GetInheritedSkeleton();
    const UsdSkelSkeletonQuery skelQuery = skelCache.GetSkelQuery(skel);
    if (!skelQuery)
    {
        return skinnedPointsAttr;
    }

    // Get motion samples from UsdSkel animation query
    const UsdSkelAnimQuery skelAnimQuery = skelQuery.GetAnimQuery();
    std::vector<double> blendShapeMotionSamples, jointXformMotionSamples;

    const std::vector<double> matchingMotionSamples = data.GetSkelMotionSampleTimes(
        skelAnimQuery, blendShapeMotionSamples, jointXformMotionSamples);

    // No guarantee that the GetSkelMotionSamples will populate the
    // blendShapeMotionSamples or jointXformMotionSamples.
    // Ensure we at least look at the current frame.
    if (blendShapeMotionSamples.empty())
    {
        blendShapeMotionSamples.push_back(currentTime);
    }
    if (jointXformMotionSamples.empty())
    {
        jointXformMotionSamples.push_back(currentTime);
    }

    std::map<float, VtArray<GfVec3f>> timeToSampleMap;
    // Prioritise JointTransform samples. Could prioritise either
    for (double relSampleTime : matchingMotionSamples)
    {
        double time = currentTime + relSampleTime;
        VtVec3fArray skinnedPoints;
        points.GetPointsAttr().Get(&skinnedPoints, time);
        // Retrieve the base points again!

        if (std::count(blendShapeMotionSamples.begin(), blendShapeMotionSamples.end(), time))
        {
            PXR_INTERNAL_NS::ApplyBlendShapeAnimation(skinningQuery, skelQuery, time,
                                                      skinnedPoints);
        }
        if (std::count(jointXformMotionSamples.begin(), jointXformMotionSamples.end(), time))
        {
            PXR_INTERNAL_NS::ApplyJointAnimation(skinningQuery, skelQuery, time, skinnedPoints);
        }

        if (!timeToSampleMap.empty())
        {
            if (timeToSampleMap.begin()->second.size() != skinnedPoints.size())
            {
                timeToSampleMap.clear();
                varyingTopology = true;
                break;
            }
        }
        float correctedSampleTime =
            isMotionBackward ? UsdKatanaUtils::ReverseTimeSample(relSampleTime) : relSampleTime;
        timeToSampleMap.insert({correctedSampleTime, skinnedPoints});
    }
    if (varyingTopology)
    {
        VtVec3fArray skinnedPoints;
        points.GetPointsAttr().Get(&skinnedPoints, currentTime);
        FnKat::DataBuilder<FnKat::FloatAttribute> defaultBuilder(tupleSize);

        PXR_INTERNAL_NS::ApplyBlendShapeAnimation(skinningQuery, skelQuery, currentTime,
                                                  skinnedPoints);
        PXR_INTERNAL_NS::ApplyJointAnimation(skinningQuery, skelQuery, currentTime, skinnedPoints);
        // Package the points in an attribute.
        if (!skinnedPoints.empty())
        {
            std::vector<float>& attrVec = attrBuilder.get(0);
            UsdKatanaUtils::ConvertArrayToVector(skinnedPoints, &attrVec);
        }
        return defaultBuilder.build();
    }
    return VtKatanaMapOrCopy<GfVec3f>(timeToSampleMap);
}

TfTokenVector UsdKatanaUtils::GetLookTokens()
{
#if defined(ARCH_OS_WINDOWS)
    static const std::string s_lookTokenSeparator = ";";
#elif defined(ARCH_OS_LINUX)
    static const std::string s_lookTokenSeparator = ":";
#endif

    static const TfTokenVector s_lookTokens{
        []()
        {
            TfTokenVector lookTokens;
            const std::string lookTokensStr = TfGetEnvSetting(USD_KATANA_LOOK_TOKENS);
            const std::vector<std::string> lookTokensSplit =
                TfStringSplit(lookTokensStr, s_lookTokenSeparator);
            for (const std::string& token : lookTokensSplit)
            {
                lookTokens.push_back(TfToken(token));
            }
            return lookTokens;
        }()};

    return s_lookTokens;
}

namespace {

// DataBuilder<>::update() is broken so we roll our own.  Note that we
// clear the builder first, unlike the update() member function.
template <typename B, typename A>
void update(B& builder, const A& attr)
{
    // Start clean and set the tuple size.
    builder = B(attr.getTupleSize());

    // Copy the data.  We make a local copy because a StringAttribute
    // returns const char* but the builder wants std::string.
    for (int64_t i = 0, n = attr.getNumberOfTimeSamples(); i != n; ++i) {
        const auto time = attr.getSampleTime(i);
        const auto& src = attr.getNearestSample(time);
        std::vector<typename A::value_type> dst(src.begin(), src.end());
        builder.set(dst, time);
    }
}

}

//
// UsdKatanaUtilsLightListAccess
//

UsdKatanaUtilsLightListAccess::UsdKatanaUtilsLightListAccess(
    FnKat::GeolibCookInterface& interface,
    const UsdKatanaUsdInArgsRefPtr& usdInArgs)
    : _interface(interface), _usdInArgs(usdInArgs)
{
    // Get the lightList attribute.
    FnKat::GroupAttribute lightList = _interface.getAttr("lightList");
    if (lightList.isValid()) {
        _lightListBuilder.deepUpdate(lightList);
    }
}

UsdKatanaUtilsLightListAccess::~UsdKatanaUtilsLightListAccess()
{
    // Do nothing
}

void UsdKatanaUtilsLightListAccess::SetPath(const SdfPath& lightPath)
{
    _lightPath = lightPath;
    if (_lightPath.IsAbsolutePath()) {
        _key = TfStringReplace(GetLocation().substr(1), "/", "_") + '.';
    }
    else {
        _key.clear();
    }
}

UsdPrim UsdKatanaUtilsLightListAccess::GetPrim() const
{
    return _usdInArgs->GetStage()->GetPrimAtPath(_lightPath);
}

std::string UsdKatanaUtilsLightListAccess::GetLocation() const
{
    return UsdKatanaUtils::ConvertUsdPathToKatLocation(_lightPath, _usdInArgs);
}

std::string UsdKatanaUtilsLightListAccess::GetLocation(const SdfPath& path) const
{
    return UsdKatanaUtils::ConvertUsdPathToKatLocation(path, _usdInArgs);
}

void UsdKatanaUtilsLightListAccess::_Set(const std::string& name, const VtValue& value)
{
    if (TF_VERIFY(!_key.empty(), "Light path not set or not absolute")) {
        FnKat::Attribute attr = UsdKatanaUtils::ConvertVtValueToKatAttr(value);
        if (TF_VERIFY(attr.isValid(),
                      "Failed to convert value for %s", name.c_str())) {
            _lightListBuilder.set(_key + name, attr);
        }
    }
}

void UsdKatanaUtilsLightListAccess::_Set(const std::string& name, const FnKat::Attribute& attr)
{
    if (TF_VERIFY(!_key.empty(), "Light path not set or not absolute")) {
        _lightListBuilder.set(_key + name, attr);
    }
}

bool UsdKatanaUtilsLightListAccess::SetLinks(const UsdCollectionAPI& collectionAPI,
                                             const std::string& linkName)
{
    bool isLinked = false;
    FnKat::GroupBuilder onBuilder, offBuilder;

    // See if the prim has special blind data for round-tripping CEL
    // expressions.
    UsdPrim prim = collectionAPI.GetPrim();
    UsdAttribute off =
        prim.GetAttribute(TfToken("katana:CEL:lightLink:" + linkName + ":off"));
    UsdAttribute on =
        prim.GetAttribute(TfToken("katana:CEL:lightLink:" + linkName + ":on"));
    if (off.IsValid() || on.IsValid()) {
        // We have CEL info.  Use it as-is.
        VtArray<std::string> patterns;
        if (off.IsValid() && off.Get(&patterns)) {
            for (const auto& pattern: patterns) {
                const FnKat::StringAttribute patternAttr(pattern);
                offBuilder.set(patternAttr.getHash().str(), patternAttr);
            }
        }
        if (on.IsValid() && on.Get(&patterns)) {
            for (const auto& pattern: patterns) {
                const FnKat::StringAttribute patternAttr(pattern);
                onBuilder.set(patternAttr.getHash().str(), patternAttr);
            }
        }

        // We can't know without evaluating if we link the prim's path
        // so assume that we do.
        isLinked = true;
    }
    else {
        UsdCollectionAPI::MembershipQuery query =
            collectionAPI.ComputeMembershipQuery();
        UsdCollectionAPI::MembershipQuery::PathExpansionRuleMap linkMap =
            query.GetAsPathExpansionRuleMap();
        for (const auto &entry: linkMap) {
            if (entry.first == SdfPath::AbsoluteRootPath()) {
                // Skip property paths
                continue;
            }
            // By convention, entries are "link.TYPE.{on,off}.HASH" where
            // HASH is getHash() of the CEL and TYPE is the type of linking
            // (light, shadow, etc). In this case we can just hash the
            // string attribute form of the location.
            const std::string location =
                UsdKatanaUtils::ConvertUsdPathToKatLocation(entry.first, _usdInArgs);
            const FnKat::StringAttribute locAttr(location);
            const std::string linkHash = locAttr.getHash().str();
            const bool on = (entry.second != UsdTokens->exclude);
            (on ? onBuilder : offBuilder).set(linkHash, locAttr);
            isLinked = true;
        }
    }

    // Set off and then on attributes, in order, to ensure
    // stable override semantics when katana applies these.
    // (This matches what the Gaffer node does.)
    FnKat::GroupAttribute offAttr = offBuilder.build();
    if (offAttr.getNumberOfChildren()) {
        _Set("link."+linkName+".off", offAttr);
    }
    FnKat::GroupAttribute onAttr = onBuilder.build();
    if (onAttr.getNumberOfChildren()) {
        _Set("link."+linkName+".on", onAttr);
    }

    return isLinked;
}

void UsdKatanaUtilsLightListAccess::AddToCustomStringList(const std::string& tag,
                                                          const std::string& value)
{
    // Append the value.
    if (_customStringLists.find(tag) == _customStringLists.end()) {
        // This is the first value.  First copy any existing attribute.
        auto& builder = _customStringLists[tag];
        FnKat::StringAttribute attr = _interface.getAttr(tag);
        if (attr.isValid()) {
            update(builder, attr);
        }

        // Then append the value.
        builder.push_back(value);
    }
    else {
        // We've already seen this tag.  Just append the value.
        _customStringLists[tag].push_back(value);
    }
}

void UsdKatanaUtilsLightListAccess::Build()
{
    FnKat::GroupAttribute lightListAttr = _lightListBuilder.build();
    if (lightListAttr.getNumberOfChildren() > 0) {
        _interface.setAttr("lightList", lightListAttr);
    }

    // Add custom string lists.
    for (auto& value: _customStringLists) {
        auto attr = value.second.build();
        if (attr.getNumberOfValues() > 0) {
            _interface.setAttr(value.first, attr);
        }
    }
    _customStringLists.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE

