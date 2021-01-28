# Copyright (c) 2020 The Foundry Visionmongers Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
# names, trademarks, service marks, or product names of the Licensor
# and its affiliates, except as required to comply with Section 4(c) of
# the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.

import logging

from Katana import FnAttribute

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from fnpxr import UsdRi, Tf, UsdGeom, Usd

except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)


def WritePrmanStatements(prmanStatementsAttributes, prim):
    """
    This function will write out 'prmanStatements' to a C{prim} using
    C{UsdRi.StatementsAPI} schema. Currently we are only writing out
    'prmanStatements.attribute'.

    @type prmanStatementsAttributes: C{(str, FnAttribute.Attribute)}
    @type prim: C{Usd.Prim}
    @param prmanStatementsAttributes: A 2-element tuple with the first element
        being the name of the attribute and the second the attribute itself.
    @param prim: A prim to write prmanStatements to.
    """
    prmanStatementsSchema = UsdRi.StatementsAPI(prim)
    for groupName, value in prmanStatementsAttributes.iteritems():
        if not groupName == "prmanStatements.attributes":
            continue
        for child in value.childList():
            _WriteRiAttributes(child, prmanStatementsSchema)


def WritePrmanGeomGprims(prmanStatementsAttributes, prim):
    """
    This function will write out prmanStatements to a C{prim} using
    C{UsdGeom.Gprim} schema. Currently it only writes out:
    "prmanStatements.sides"
    "prmanStatements.attributes.Sides"
    "prmanStatements.orientation"

    @type prmanStatementsAttributes: C{(str, FnAttribute.Attribute)}
    @type prim: C{Usd.Prim}
    @param prmanStatementsAttributes: A 2-element tuple with the first element
        being the name of the attribute and the second the attribute itself.
    @param prim: A prim to write prmanStatements to.
    """
    usdGeomGprim = UsdGeom.Gprim(prim)

    sidesAttr = prmanStatementsAttributes.get(
        "prmanStatements.sides")
    attributesAttr = prmanStatementsAttributes.get(
        "prmanStatements.attributes")
    if attributesAttr \
            and isinstance(attributesAttr, FnAttribute.GroupAttribute):
        attributesSidesAttr = attributesAttr.getChildByName("Sides")
        if attributesSidesAttr:
            sidesAttr = attributesSidesAttr
    if sidesAttr and not isinstance(sidesAttr, FnAttribute.GroupAttribute):
        sides = sidesAttr.getValue()
        isDoubleSided = True
        if sides == 1:
            isDoubleSided = False
        usdGeomGprim.CreateDoubleSidedAttr(isDoubleSided)

    orientationAttr = prmanStatementsAttributes.get(
        "prmanStatements.orientation")
    if orientationAttr:
        usdGeomGprim.CreateOrientationAttr(orientationAttr.getValue())


def WritePrmanModel(prmanStatementsAttributes, prim):
    """
    This function will make the prim a 'model' kind depending on the value of
    'scopedCoordinateSystemAttr'.

    @type prmanStatementsAttributes: C{(str, FnAttribute.Attribute)}
    @type prim: C{Usd.Prim}
    @param prmanStatementsAttributes: A 2-element tuple with the first element
        being the name of the attribute and the second the attribute itself.
    @param prim: A prim to write prmanStatements to.
    """
    scopedCoordinateSystemAttr = prmanStatementsAttributes.get(
        "prmanStatements.scopedCoordinateSystem")
    if scopedCoordinateSystemAttr \
            and not isinstance(scopedCoordinateSystemAttr,
                               FnAttribute.GroupAttribute) \
            and scopedCoordinateSystemAttr.getValue() == "ModelSpace":
        usdModel = Usd.ModelAPI(prim)
        usdModel.SetKind("model")


def _WriteRiAttributes(attributePair, prmanStatementsSchema, namespace=""):
    """
    This function will write out prmanStatement attributes. It will do it
    recursively until it reaches an attribute that is not
    C{FnAttribute.GroupAttribute}, accumulating the namespace along the way.

    @type attributePair: C{(str, FnAttribute.Attribute)}
    @type prmanStatementsSchema: C{UsdRi.StatementsAPI}
    @type namespace: C{str}
    @param attributePair: A 2-element tuple with the first element
        being the name of the attribute and the second the attribute itself.
    @param prmanStatementsSchema: A C{Usd.Prim} in a C{UsdRi.StatementsAPI}
        schema.
    @param namespace: The namespace of the attribute.
    """
    name = attributePair[0]
    attribute = attributePair[1]
    if not isinstance(attribute, FnAttribute.GroupAttribute):
        success, tfType, value = _ConvertKatanaAttributeToVtValue(attribute)
        if not success:
            log.warning(
                "Couldn't convert a Katana attribute %s to VtValue",
                namespace + "." + name)
            return
        if namespace:
            usdAttribute = prmanStatementsSchema.CreateRiAttribute(
                name, tfType, namespace)
        else:
            usdAttribute = \
                prmanStatementsSchema.CreateRiAttribute(name, tfType)
        usdAttribute.Set(value)
    else:
        if namespace:
            namespace = namespace + ":" + name
        else:
            namespace = name
        for child in attribute.childList():
            _WriteRiAttributes(child, prmanStatementsSchema, namespace)


def _ConvertKatanaAttributeToVtValue(katanaAttribute):
    """
    A simple function to convert between katana attribute types and Usd types.
    For now it seems like we only need scalar 'int', 'float' and 'string' but
    we might need more in the future.

    @type katanaAttribute: C{FnAttribute.Attribute}
    @rtype: C{(bool, Tf.Type, object)}
    @param katanaAttribute: A Katana attribute to convert to Usd type.
    @return: The resulting TfType.
    """
    size = katanaAttribute.getTupleSize()
    if size == 1:
        value = katanaAttribute.getValue()
        if isinstance(katanaAttribute, FnAttribute.IntAttribute):
            tfType = Tf.Type.FindByName("int")
            return (True, tfType, value)
        if isinstance(katanaAttribute, FnAttribute.FloatAttribute):
            tfType = Tf.Type.FindByName("float")
            return (True, tfType, value)
        if isinstance(katanaAttribute, FnAttribute.StringAttribute):
            tfType = Tf.Type.FindByName("string")
            return (True, tfType, value)
    return (False, None, None)
