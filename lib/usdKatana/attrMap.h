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
#ifndef USDKATANA_ATTRMAP_H
#define USDKATANA_ATTRMAP_H

// pxr.h required before string to ensure windows related string definitions are made.
#include <string>

#include <pxr/pxr.h>
#include <pxr/usd/usd/attribute.h>

#include <FnAttribute/FnGroupBuilder.h>
#include <FnGeolib/op/FnGeolibOp.h>

#include "usdKatana/api.h"

#include <boost/thread/shared_mutex.hpp>

PXR_NAMESPACE_OPEN_SCOPE


/// \brief An object to store attributes.  The current implementation uses
/// a Foundry::Katana::GroupBuilder behind the scenes, but the dependence on
/// that thus far is somewhat minimal and not all of the behavior of
/// GroupBuilder has been exposed.
///
/// This class is here in case we need to have different behavior than the
/// GroupBuilder.
class UsdKatanaAttrMap
{
public:
    /// Configure this object to evaluate USD attributes at the given time.
    void SetUSDTimeCode(UsdTimeCode timeCode) {
        _usdTimeCode = timeCode;
    }

    /// Set the katana attribute \p path by evaluating the given
    /// USD attribute \p attr at the time configured in SetUSDTime().
    /// Returns this object by reference so these calls can be chained.
    USDKATANA_API UsdKatanaAttrMap& Set(const std::string& path, const UsdAttribute& attr);

    /// \brief set \p attr at \p path.
    USDKATANA_API void set(const std::string& path, const Foundry::Katana::Attribute& attr);

    /// \brief delete attribute at \p path
    USDKATANA_API void del(const std::string& path);

    /// \brief build a group attribute
    USDKATANA_API FnAttribute::GroupAttribute build();

    /// \brief sets attrs in \p attrs onto the \p interface.
    USDKATANA_API void toInterface(Foundry::Katana::GeolibCookInterface& interface);


    /// \brief returns true if a call to build has been made prior to any
    ///        subsequent calls to set or del.
    USDKATANA_API bool isBuilt();
    

    typedef boost::upgrade_mutex Mutex;
    /// \brief while no locking occurs internal to this class, calling code
    ///        may wish to manage read/write locks per-instance.
    Mutex & getInstanceMutex() { return m_mutex; }

private:
    Foundry::Katana::GroupBuilder _groupBuilder;

    // Cache the last call to _groupBuilder.build() so that instances can be
    // reused (as GroupBuilder clears itself by default)
    Foundry::Katana::GroupAttribute _lastBuilt;


    // Timecode to use when reading USD samples
    UsdTimeCode _usdTimeCode;
    
    // per-instance mutex available for external use.
    Mutex m_mutex;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // USDKATANA_ATTRMAP_H
