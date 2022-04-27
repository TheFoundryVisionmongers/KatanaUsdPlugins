// These files began life as part of the main USD distribution
// https://github.com/PixarAnimationStudios/USD.
// In 2019, Foundry and Pixar agreed Foundry should maintain and curate
// these plug-ins, and they moved to
// https://github.com/TheFoundryVisionmongers/KatanaUsdPlugins
// under the same Modified Apache 2.0 license, as shown below.
//
// Copyright 2018 Pixar
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
#ifndef VTKATANA_ARRAY_H
#define VTKATANA_ARRAY_H

#include "pxr/pxr.h"
#include <FnAttribute/FnAttribute.h>

#include "pxr/base/vt/array.h"
#include "vtKatana/traits.h"

#if defined(ARCH_OS_WINDOWS)
#include "vtKatana/api.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

/// Maps a VtArray to a Katana attribute, minimizing intermediate copies.
///
/// The number of intermediate copies required to construct an attribute
/// is determined by the type traits internal to this library. As a general,
/// rule of thumb, if the precision of the source array type matches
/// the destination type, you can assume that no intermediate copies are
/// required. For example, a Vec3fArray shouldn't require intermediate
/// copies to construct a FloatAttribute, but a BoolArray requires
/// constructing an intermediate Int copy to construct an IntAttribute.
///
/// If VTKATANA_ENABLE_ZERO_COPY_ARRAYS is enabled, MapOrCopy is allowed to
/// utilize Katana's ZeroCopy feature to allow the data to be owned by a
/// VtArray
///
/// \note Because Katana hashes every attribute, zero copy data from
/// crate files will need to be read as soon as the attribute is created.
/// There's no way to cleverly stack crate and katana's zero copy features
/// to avoid or defer an attribute being copied into memory.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaMapOrCopy(
    const VtArray<T>& value);

/// Maps a series of \p times and \p values to a Katana attribute, minimizing
/// intermediate copies.
///
/// The number of intermediate copies required to construct an attribute
/// is determined by the type traits internal to this library. As a general,
/// rule of thumb, if the precision of the source array type matches
/// the destination type, you can assume that no intermediate copies are
/// required.  For example, a Vec3fArray shouldn't require intermediate
/// copies to construct a FloatAttribute, but a BoolArray requires
/// constructing an intermediate Int copy to construct an IntAttribute.
///
/// If VTKATANA_ENABLE_ZERO_COPY_ARRAYS is enabled, MapOrCopy is allowed to
/// utilize Katana's ZeroCopy feature to allow the data to be owned by the
/// VtArray.
///
/// \warn \p times MUST be sorted.
///
/// \note Because Katana hashes every attribute, zero copy data from
/// crate files will need to be read as soon as the attribute is created.
/// There's no way to cleverly stack crate and katana's zero copy features
/// to avoid or defer an attribute being copied into memory.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaMapOrCopy(
    const std::vector<float>& times,
    const typename std::vector<VtArray<T>>& values);

/// Create a map containing VtArrays of all motion samples contained
/// by \p attribute.
///
/// \sa VtKatanaMapOrCopy(const typename VtKatana_GetKatanaAttrType<T>::type&,
/// float)
template <typename T>
std::map<float, VtArray<T>> VtKatanaMapOrCopy(
    const typename VtKatana_GetKatanaAttrType<T>::type& attribute);

/// Maps \p timeToValueMap to a Katana attribute, minimizing
/// intermediate copies.
///
/// Internally, the map will be flattened into two vectors, so
/// \ref VtKatanaMapOrCopy(const std::vector<float>&,const std::vector<VtArray<T>>&)
/// is preferable if you already have sorted vectors.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaMapOrCopy(
    const typename std::map<float, VtArray<T>>& timeToValueMap);

/// Copy a VtArray to a Katana attribute, minimizing intermediate copies, but
/// disallowing any Zero Copy features the type might support.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaCopy(
    const VtArray<T>& value);

/// Copy a series of VtArray time samples to a Katana attribute, minimizing
/// intermediate copies, but disallowing any Zero Copy features the type
/// might support.
///
/// \warn \p times MUST be sorted.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaCopy(
    const std::vector<float>& times,
    const typename std::vector<VtArray<T>>& values);

/// Copy \p timeToValueMap to a Katana attribute, minimizing
/// intermediate copies, but disallowing any Zero Copy features the type
/// might support.
///
/// Internally, the map will be flattened into two vectors, so
/// \ref VtKatanaCopy(const std::vector<float>&,const std::vector<VtArray<T>>&)
/// is preferable if you already have sorted vectors.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaCopy(
    const typename std::map<float, VtArray<T>>& timeToValueMap);

/// Create a VtArray from the \p attribute array nearest to \sample
///
/// The number of intermediate copies required to construct an attribute
/// is determined by the type traits internal to this library. As a general,
/// rule of thumb, if the precision of the source array type matches
/// the destination type, you can assume that no intermediate copies are
/// required.  For example, a FloatAttribute shouldn't require intermediate
/// copies to construct a Vec3fArray, but an IntAttribute requires
/// constructing an intermediate copy to construct an BoolArray.
///
/// If the VTKATANA_ENABLE_ZERO_COPY_ARRAYS env settng is enabled,
/// this returns a VtArray with an attribute holder pointing to the
/// originating attribute.  For vec and matrix types, the attribute
/// must match the dimensionality of the of the Element to be succesfully
/// constructed.
///
/// \note A reference to the attribute is retained until the array is
/// uniquified by calling any non-const method on the array. Since an
/// attribute stores multiple time samples, it is technically possible
/// for you to hold onto more data than you intended. However, the
/// number of time samples in general is small, so this shouldn't be
/// an issue, but if this is of concern, use VtKatanaCopy instead.
template <typename T>
VtArray<T> VtKatanaMapOrCopy(
        const typename VtKatana_GetKatanaAttrType<T>::type& attr,
        float sample = 0.0f);

/// Copy a single sample from a Katana attribute to a VtArray, minizing
/// intermediate copies, but disallowing any Zero Copy features the type
/// might support.
template <typename T>
VtArray<T> VtKatanaCopy(
    const typename VtKatana_GetKatanaAttrType<T>::type& attr,
    float sample = 0.0f);


/// Copy ALL time samples from a Katana attribute to a map of VtArrays,
/// minimizing intermediate copies, but disallowing any Zero Copy features
/// the type might support.
template <typename T>
std::map<float, VtArray<T>> VtKatanaCopy(
    const typename VtKatana_GetKatanaAttrType<T>::type& attr);


#if defined(ARCH_OS_WINDOWS)
#define VTKATANA_EXPORT_MAP_AND_COPY(T)                                    \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type     \
        VtKatanaMapOrCopy<T>(                                              \
            const VtArray<T>& value);                                      \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type     \
        VtKatanaMapOrCopy<T>(                                              \
            const std::vector<float>& times,                               \
            const typename std::vector<VtArray<T>>& values);               \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type     \
        VtKatanaMapOrCopy<T>(                                              \
            const typename std::map<float, VtArray<T>>&);                  \
    template VTKATANA_API VtArray<T> VtKatanaMapOrCopy<T>(                 \
        const typename VtKatana_GetKatanaAttrType<T>::type&, float);       \
    template VTKATANA_API typename std::map<float, VtArray<T>>             \
        VtKatanaMapOrCopy<T>(                                              \
            const typename VtKatana_GetKatanaAttrType<T>::type&);          \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type     \
        VtKatanaCopy<T>(                                                   \
            const VtArray<T>& value);                                      \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type     \
        VtKatanaCopy<T>(                                                   \
            const std::vector<float>& times,                               \
            const typename std::vector<VtArray<T>>& values);               \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type     \
        VtKatanaCopy<T>(                                                   \
            const typename std::map<float, VtArray<T>>&);                  \
    template VTKATANA_API VtArray<T> VtKatanaCopy<T>(                      \
        const typename VtKatana_GetKatanaAttrType<T>::type&, float);       \
    template VTKATANA_API typename std::map<float, VtArray<T>>             \
        VtKatanaCopy<T>(const typename                                     \
            VtKatana_GetKatanaAttrType<T>::type&);                         \

// Defines for copy
// Integral Types
VTKATANA_EXPORT_MAP_AND_COPY(bool)
VTKATANA_EXPORT_MAP_AND_COPY(char)
VTKATANA_EXPORT_MAP_AND_COPY(unsigned char)
VTKATANA_EXPORT_MAP_AND_COPY(short)
VTKATANA_EXPORT_MAP_AND_COPY(unsigned short)
VTKATANA_EXPORT_MAP_AND_COPY(int)
VTKATANA_EXPORT_MAP_AND_COPY(unsigned int)
VTKATANA_EXPORT_MAP_AND_COPY(uint64_t)
VTKATANA_EXPORT_MAP_AND_COPY(int64_t)
VTKATANA_EXPORT_MAP_AND_COPY(long)

// Floating point types
VTKATANA_EXPORT_MAP_AND_COPY(float)
VTKATANA_EXPORT_MAP_AND_COPY(double)
VTKATANA_EXPORT_MAP_AND_COPY(GfHalf)

// Vec Types
VTKATANA_EXPORT_MAP_AND_COPY(GfVec2i)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec2f)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec2h)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec2d)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec3i)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec3f)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec3h)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec3d)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec4i)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec4f)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec4h)
VTKATANA_EXPORT_MAP_AND_COPY(GfVec4d)

// Matrix Types
VTKATANA_EXPORT_MAP_AND_COPY(GfMatrix3f)
VTKATANA_EXPORT_MAP_AND_COPY(GfMatrix3d)
VTKATANA_EXPORT_MAP_AND_COPY(GfMatrix4f)
VTKATANA_EXPORT_MAP_AND_COPY(GfMatrix4d)

// String types
VTKATANA_EXPORT_MAP_AND_COPY(std::string)
VTKATANA_EXPORT_MAP_AND_COPY(SdfAssetPath)
VTKATANA_EXPORT_MAP_AND_COPY(SdfPath)
VTKATANA_EXPORT_MAP_AND_COPY(TfToken)

#undef VTKATANA_EXPORT_MAP_AND_COPY
#endif // defined(ARCH_OS_WINDOWS)

PXR_NAMESPACE_CLOSE_SCOPE

#endif
