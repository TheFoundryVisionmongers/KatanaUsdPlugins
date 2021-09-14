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
#ifndef USDKATANA_GENERATED_LIGHTAPI_H
#define USDKATANA_GENERATED_LIGHTAPI_H

/// \file usdKatana/lightAPI.h

#include "pxr/pxr.h"
#include "usdKatana/api.h"
#include "pxr/usd/usd/apiSchemaBase.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "usdKatana/tokens.h"

#include "pxr/base/vt/value.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/matrix4d.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

class SdfAssetPath;

// -------------------------------------------------------------------------- //
// LIGHTAPI                                                                   //
// -------------------------------------------------------------------------- //

/// \class UsdKatanaLightAPI
///
/// Katana-specific entensions of UsdLuxLight
///
class UsdKatanaLightAPI : public UsdAPISchemaBase
{
public:
    /// Compile time constant representing what kind of schema this class is.
    ///
    /// \sa UsdSchemaKind
    static const UsdSchemaKind schemaKind = UsdSchemaKind::SingleApplyAPI;

    /// \deprecated
    /// Same as schemaKind, provided to maintain temporary backward
    /// compatibility with older generated schemas.
    static const UsdSchemaKind schemaType = UsdSchemaKind::SingleApplyAPI;

    /// Construct a UsdKatanaLightAPI on UsdPrim \p prim .
    /// Equivalent to UsdKatanaLightAPI::Get(prim.GetStage(), prim.GetPath())
    /// for a \em valid \p prim, but will not immediately throw an error for
    /// an invalid \p prim
    explicit UsdKatanaLightAPI(const UsdPrim& prim=UsdPrim())
        : UsdAPISchemaBase(prim)
    {
    }

    /// Construct a UsdKatanaLightAPI on the prim held by \p schemaObj .
    /// Should be preferred over UsdKatanaLightAPI(schemaObj.GetPrim()),
    /// as it preserves SchemaBase state.
    explicit UsdKatanaLightAPI(const UsdSchemaBase& schemaObj)
        : UsdAPISchemaBase(schemaObj)
    {
    }

    /// Destructor.
    USDKATANA_API
    virtual ~UsdKatanaLightAPI();

    /// Return a vector of names of all pre-declared attributes for this schema
    /// class and all its ancestor classes.  Does not include attributes that
    /// may be authored by custom/extended methods of the schemas involved.
    USDKATANA_API
    static const TfTokenVector &
    GetSchemaAttributeNames(bool includeInherited=true);

    /// Return a UsdKatanaLightAPI holding the prim adhering to this
    /// schema at \p path on \p stage.  If no prim exists at \p path on
    /// \p stage, or if the prim at that path does not adhere to this schema,
    /// return an invalid schema object.  This is shorthand for the following:
    ///
    /// \code
    /// UsdKatanaLightAPI(stage->GetPrimAtPath(path));
    /// \endcode
    ///
    USDKATANA_API
    static UsdKatanaLightAPI
    Get(const UsdStagePtr &stage, const SdfPath &path);


    /// Applies this <b>single-apply</b> API schema to the given \p prim.
    /// This information is stored by adding "LightAPI" to the
    /// token-valued, listOp metadata \em apiSchemas on the prim.
    ///
    /// \return A valid UsdKatanaLightAPI object is returned upon success.
    /// An invalid (or empty) UsdKatanaLightAPI object is returned upon
    /// failure. See \ref UsdPrim::ApplyAPI() for conditions
    /// resulting in failure.
    ///
    /// \sa UsdPrim::GetAppliedSchemas()
    /// \sa UsdPrim::HasAPI()
    /// \sa UsdPrim::ApplyAPI()
    /// \sa UsdPrim::RemoveAPI()
    ///
    USDKATANA_API
    static UsdKatanaLightAPI
    Apply(const UsdPrim &prim);

protected:
    /// Returns the kind of schema this class belongs to.
    ///
    /// \sa UsdSchemaKind
    USDKATANA_API
    UsdSchemaKind _GetSchemaKind() const override;

    /// \deprecated
    /// Same as _GetSchemaKind, provided to maintain temporary backward
    /// compatibility with older generated schemas.
    USDKATANA_API
    UsdSchemaKind _GetSchemaType() const override;

private:
    // needs to invoke _GetStaticTfType.
    friend class UsdSchemaRegistry;
    USDKATANA_API
    static const TfType &_GetStaticTfType();

    static bool _IsTypedSchema();

    // override SchemaBase virtuals.
    USDKATANA_API
    const TfType &_GetTfType() const override;

public:
    // --------------------------------------------------------------------- //
    // ID
    // --------------------------------------------------------------------- //
    /// Defines the light shader name used by Katana when
    /// creating the light location. This allows renderer-specific
    /// implementations of lights to be correctly created.
    ///
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform string[] katana:id` |
    /// | C++ Type | VtArray<std::string> |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->StringArray |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    USDKATANA_API
    UsdAttribute GetIdAttr() const;

    /// See GetIdAttr(), and also
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    USDKATANA_API
    UsdAttribute CreateIdAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // CENTEROFINTEREST
    // --------------------------------------------------------------------- //
    /// Center of interest holds a distance extending from the light's
    /// origin in the direction it is facing. This defines a point at which the
    /// light can be pivoted around or translated towards or away from using
    /// Katana's lighting tools.
    ///
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform double geometry:centerOfInterest = 20` |
    /// | C++ Type | double |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Double |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    USDKATANA_API
    UsdAttribute GetCenterOfInterestAttr() const;

    /// See GetCenterOfInterestAttr(), and also
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    USDKATANA_API
    UsdAttribute CreateCenterOfInterestAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // ===================================================================== //
    // Feel free to add custom code below this line, it will be preserved by
    // the code generator.
    //
    // Just remember to:
    //  - Close the class declaration with };
    //  - Close the namespace with PXR_NAMESPACE_CLOSE_SCOPE
    //  - Close the include guard with #endif
    // ===================================================================== //
    // --(BEGIN CUSTOM CODE)--
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
