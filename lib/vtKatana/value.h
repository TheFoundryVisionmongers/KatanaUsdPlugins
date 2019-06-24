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
#ifndef VTKATANA_VALUE_H
#define VTKATANA_VALUE_H

#include "pxr/pxr.h"
#include "vtKatana/traits.h"

#if defined(ARCH_OS_WINDOWS)
#include "api.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

/// Copy \p value to a katana attribute, minimizing intermediate copies
/// when possible, but always copying at least once.
template <typename T>
typename VtKatana_GetKatanaAttrType<T>::type VtKatanaCopy(const T& value);

#if defined(ARCH_OS_WINDOWS)
#define VTKATANA_EXPORT_VALUE_COPY(T)                                       \
    template VTKATANA_API typename VtKatana_GetKatanaAttrType<T>::type VtKatanaCopy<T>(  \
        const T& value);

// Integral types

VTKATANA_EXPORT_VALUE_COPY(bool)
VTKATANA_EXPORT_VALUE_COPY(char)
VTKATANA_EXPORT_VALUE_COPY(unsigned char)
VTKATANA_EXPORT_VALUE_COPY(short)
VTKATANA_EXPORT_VALUE_COPY(unsigned short)
VTKATANA_EXPORT_VALUE_COPY(int)
VTKATANA_EXPORT_VALUE_COPY(unsigned int)
VTKATANA_EXPORT_VALUE_COPY(uint64_t)
VTKATANA_EXPORT_VALUE_COPY(int64_t)
VTKATANA_EXPORT_VALUE_COPY(long)

// Vec Types
VTKATANA_EXPORT_VALUE_COPY(GfVec2i)
VTKATANA_EXPORT_VALUE_COPY(GfVec2f)
VTKATANA_EXPORT_VALUE_COPY(GfVec2h)
VTKATANA_EXPORT_VALUE_COPY(GfVec2d)
VTKATANA_EXPORT_VALUE_COPY(GfVec3i)
VTKATANA_EXPORT_VALUE_COPY(GfVec3f)
VTKATANA_EXPORT_VALUE_COPY(GfVec3h)
VTKATANA_EXPORT_VALUE_COPY(GfVec3d)
VTKATANA_EXPORT_VALUE_COPY(GfVec4i)
VTKATANA_EXPORT_VALUE_COPY(GfVec4f)
VTKATANA_EXPORT_VALUE_COPY(GfVec4h)
VTKATANA_EXPORT_VALUE_COPY(GfVec4d)

// Matrix Types
VTKATANA_EXPORT_VALUE_COPY(GfMatrix3f)
VTKATANA_EXPORT_VALUE_COPY(GfMatrix3d)
VTKATANA_EXPORT_VALUE_COPY(GfMatrix4f)
VTKATANA_EXPORT_VALUE_COPY(GfMatrix4d)

// Floating point types
VTKATANA_EXPORT_VALUE_COPY(float)
VTKATANA_EXPORT_VALUE_COPY(double)
VTKATANA_EXPORT_VALUE_COPY(GfHalf)

// String types
VTKATANA_EXPORT_VALUE_COPY(std::string)
VTKATANA_EXPORT_VALUE_COPY(SdfAssetPath)
VTKATANA_EXPORT_VALUE_COPY(SdfPath)
VTKATANA_EXPORT_VALUE_COPY(TfToken)

#undef VTKATANA_EXPORT_VALUE_COPY
#endif // defined(ARCH_OS_WINDOWS)

PXR_NAMESPACE_CLOSE_SCOPE

#endif
