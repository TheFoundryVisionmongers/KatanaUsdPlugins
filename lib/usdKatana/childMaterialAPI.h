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
#ifndef USDKATANA_GENERATED_CHILDMATERIALAPI_H
#define USDKATANA_GENERATED_CHILDMATERIALAPI_H

/// \file usdKatana/childMaterialAPI.h

#include "pxr/pxr.h"
#include "pxr/usd/usd/apiSchemaBase.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "usdKatana/api.h"
#include "usdKatana/tokens.h"

#include "pxr/base/vt/value.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

class SdfAssetPath;

// -------------------------------------------------------------------------- //
// CHILDMATERIALAPI                                                           //
// -------------------------------------------------------------------------- //

/// \class UsdKatanaChildMaterialAPI
///
/// Katana-specific extensions of UsdShadeMaterial.
///
class UsdKatanaChildMaterialAPI : public UsdAPISchemaBase
{
public:
    /// Compile time constant representing what kind of schema this class is.
    ///
    /// \sa UsdSchemaKind
    static const UsdSchemaKind schemaKind = UsdSchemaKind::SingleApplyAPI;

    /// Construct a UsdKatanaChildMaterialAPI on UsdPrim \p prim .
    /// Equivalent to UsdKatanaChildMaterialAPI::Get(prim.GetStage(), prim.GetPath())
    /// for a \em valid \p prim, but will not immediately throw an error for
    /// an invalid \p prim
    explicit UsdKatanaChildMaterialAPI(const UsdPrim& prim = UsdPrim()) : UsdAPISchemaBase(prim) {}

    /// Construct a UsdKatanaChildMaterialAPI on the prim held by \p schemaObj .
    /// Should be preferred over UsdKatanaChildMaterialAPI(schemaObj.GetPrim()),
    /// as it preserves SchemaBase state.
    explicit UsdKatanaChildMaterialAPI(const UsdSchemaBase& schemaObj) : UsdAPISchemaBase(schemaObj)
    {
    }

    /// Destructor.
    USDKATANA_API
    virtual ~UsdKatanaChildMaterialAPI();

    /// Return a vector of names of all pre-declared attributes for this schema
    /// class and all its ancestor classes.  Does not include attributes that
    /// may be authored by custom/extended methods of the schemas involved.
    USDKATANA_API
    static const TfTokenVector& GetSchemaAttributeNames(bool includeInherited = true);

    /// Return a UsdKatanaChildMaterialAPI holding the prim adhering to this
    /// schema at \p path on \p stage.  If no prim exists at \p path on
    /// \p stage, or if the prim at that path does not adhere to this schema,
    /// return an invalid schema object.  This is shorthand for the following:
    ///
    /// \code
    /// UsdKatanaChildMaterialAPI(stage->GetPrimAtPath(path));
    /// \endcode
    ///
    USDKATANA_API
    static UsdKatanaChildMaterialAPI Get(const UsdStagePtr& stage, const SdfPath& path);

    /// Returns true if this <b>single-apply</b> API schema can be applied to
    /// the given \p prim. If this schema can not be a applied to the prim,
    /// this returns false and, if provided, populates \p whyNot with the
    /// reason it can not be applied.
    ///
    /// Note that if CanApply returns false, that does not necessarily imply
    /// that calling Apply will fail. Callers are expected to call CanApply
    /// before calling Apply if they want to ensure that it is valid to
    /// apply a schema.
    ///
    /// \sa UsdPrim::GetAppliedSchemas()
    /// \sa UsdPrim::HasAPI()
    /// \sa UsdPrim::CanApplyAPI()
    /// \sa UsdPrim::ApplyAPI()
    /// \sa UsdPrim::RemoveAPI()
    ///
    USDKATANA_API
    static bool CanApply(const UsdPrim& prim, std::string* whyNot = nullptr);

    /// Applies this <b>single-apply</b> API schema to the given \p prim.
    /// This information is stored by adding "ChildMaterialAPI" to the
    /// token-valued, listOp metadata \em apiSchemas on the prim.
    ///
    /// \return A valid UsdKatanaChildMaterialAPI object is returned upon success.
    /// An invalid (or empty) UsdKatanaChildMaterialAPI object is returned upon
    /// failure. See \ref UsdPrim::ApplyAPI() for conditions
    /// resulting in failure.
    ///
    /// \sa UsdPrim::GetAppliedSchemas()
    /// \sa UsdPrim::HasAPI()
    /// \sa UsdPrim::CanApplyAPI()
    /// \sa UsdPrim::ApplyAPI()
    /// \sa UsdPrim::RemoveAPI()
    ///
    USDKATANA_API
    static UsdKatanaChildMaterialAPI Apply(const UsdPrim& prim);

protected:
    /// Returns the kind of schema this class belongs to.
    ///
    /// \sa UsdSchemaKind
    USDKATANA_API
    UsdSchemaKind _GetSchemaKind() const override;

private:
    // needs to invoke _GetStaticTfType.
    friend class UsdSchemaRegistry;
    USDKATANA_API
    static const TfType& _GetStaticTfType();

    static bool _IsTypedSchema();

    // override SchemaBase virtuals.
    USDKATANA_API
    const TfType& _GetTfType() const override;

public:
    // --------------------------------------------------------------------- //
    // PRIMNAME
    // --------------------------------------------------------------------- //
    /// When a Material derives from another, "base" Material (see

    /// \ref UsdShadeMaterial::SetBaseMaterial() "SetBaseMaterial()"), it seems

    /// natural to think about a "child" that inherits from its base Material

    /// "parent".

    /// However, in USD, the derived Material cannot be a child of the base Material

    /// because the \em derives relationship would cause an infinite

    /// recursion in the composition graph (because the derived Material must

    /// inherit not just the base Material prim itself, but all of the shader and

    /// other prims scoped underneath it, which would include the derived Material

    /// itself).

    ///
    /// For UI's that want to present the hierarchy that derivation implies,

    /// we provide \em primName, which specifies the derived Material's

    /// "relative name" with respect to the base Material.

    ///
    /// For example, a structure that looks like:

    /// - Metal

    /// - .. Brass

    /// - .. Aluminum

    ///
    /// will be encoded as

    /// - Metal

    /// - Metal_Brass

    /// - Metal_Aluminum

    ///
    /// We set derivedName on Metal_Brass and Metal_Aluminum to Brass and

    /// Aluminum, to be able to have proper child names if the hierarchy

    /// is reconstructed.

    ///
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform string katana:primName` |
    /// | C++ Type | std::string |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->String |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    USDKATANA_API
    UsdAttribute GetPrimNameAttr() const;

    /// See GetPrimNameAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    USDKATANA_API
    UsdAttribute CreatePrimNameAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

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
