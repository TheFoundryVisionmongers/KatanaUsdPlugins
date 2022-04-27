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
#ifndef USDKATANA_READPOINTS_H
#define USDKATANA_READPOINTS_H

#include "pxr/pxr.h"

PXR_NAMESPACE_OPEN_SCOPE

#include "usdKatana/api.h"

class UsdKatanaAttrMap;
class UsdKatanaUsdInPrivateData;
class UsdGeomPoints;

/// \brief read \p points into \p attrs.
USDKATANA_API void UsdKatanaReadPoints(const UsdGeomPoints& points,
                                       const UsdKatanaUsdInPrivateData& data,
                                       UsdKatanaAttrMap& attrs);

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // USDKATANA_READPOINTS_H
