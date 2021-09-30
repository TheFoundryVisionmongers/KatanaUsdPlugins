# Copyright (c) 2021 The Foundry Visionmongers Ltd.
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

from Katana import NodegraphAPI, Nodes3DAPI

log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
try:
    from pxr import Usd
    from UsdExport.common import (GetRelativeUsdSdfPath)
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)



def WriteLightLinking(stage, sdfLocationPath, lightListAttribute,
                      lightDict, rootName):
    """
    Write Resolved Light Linking information onto the light linked to this
    location.

    @type stage: C{Usd.Stage}
    @type rootName: C{str}
    @type sdfLocationPath: C{Sdf.Path}
    @type lightListAttribute: C{FnAttribute::GroupAttribute}
    @type lightDict: C{dict} of C{str} : C{Sdf.Path}
    @param stage: The C{Usd.Stage} to write data to.
    @param sdfLocationPath: The translated relative path for where this prim
        would be in the Usd Stage.
    @param lightListAttribute: The Katana attribute containing `lightList`.
    @param lightDict: A dictionary containing a mapping between the
        light location paths from Katana, and the SdfPath in the USD
        Stage.
    @param rootName: The root name from the C{overrideDictList}, see
        C{LookFilePassData} for more details.
    """
    for i in range(lightListAttribute.getNumberOfChildren()):
        lightAttr = lightListAttribute.getChildByIndex(i)
        lightPathAttr = lightAttr.getChildByName("path")
        if not lightPathAttr:
            continue
        lightPath = GetRelativeUsdSdfPath(lightDict, rootName,
            str(lightPathAttr.getValue()))
        if lightPath is None:
            continue
        lightPrim = stage.GetPrimAtPath(lightPath)
        if lightPrim:
            lightLinkEnableAttr = lightAttr.getChildByName("enable")
            shadowLinkEnableAttr = lightAttr.getChildByName("geoShadowEnable")
            AppendLightLinkToLight(sdfLocationPath, lightLinkEnableAttr,
                shadowLinkEnableAttr, lightPrim)

def AppendLightLinkToLight(sdfLocationPath, lightLinkEnableAttr,
                           shadowLinkEnableAttr, lightPrim):
    """
    Appends light linking information for the C{sdfLocationPath} to the
    provided C{lightPrim}

    @type sdfLocationPath: C{Sdf.Path}
    @type lightLinkEnableAttr: C{FnAttribute::IntAttribute} or C{None}
    @type shadowLinkEnableAttr: C{FnAttribute::IntAttribute} or C{None}
    @type lightPrim: C{Usd.Prim}
    @param sdfLocationPath: The translated relative path for where the light
        linking applies to.
    @param lightLinkEnableAttr: The lightList.<lightName>.enable attribute
    @param shadowLinkEnableAttr: The lightList.<lightName>.geoShadowEnable
        attribute
    @param lightPrim: The prim for the light to write the light linking relation
        to.
    """
    if lightLinkEnableAttr is not None:
        if lightLinkEnableAttr.getValue():
            CreateCollectionAttribute(lightPrim,
                                        "lightLink",
                                        Usd.CollectionAPI.CreateIncludesRel,
                                        sdfLocationPath)
        else:
            CreateCollectionAttribute(lightPrim,
                                        "lightLink",
                                        Usd.CollectionAPI.CreateExcludesRel,
                                        sdfLocationPath)
    if shadowLinkEnableAttr is not None:
        if shadowLinkEnableAttr.getValue():
            CreateCollectionAttribute(lightPrim,
                                        "shadowLink",
                                        Usd.CollectionAPI.CreateIncludesRel,
                                        sdfLocationPath)
        else:
            CreateCollectionAttribute(lightPrim,
                                        "shadowLink",
                                        Usd.CollectionAPI.CreateExcludesRel,
                                        sdfLocationPath)

def CreateCollectionAttribute(prim, collectionName, paramFunc, location):
    """
    Helper method to write collection data given the function and name of link.

    @type prim: C{Usd.Prim}
    @type collectionName: C{str}
    @type paramFunc: C{function}
    @type location: C{Sdf.Path}
    @param prim: The prim to write the collection to.
    @param collectionName: The name of the collection to write.
    @param paramFunc: The collection api function to use.
    @param location: The location to write into the collection.
    """
    collection = Usd.CollectionAPI(prim, collectionName)
    collectionTargets = paramFunc(collection)
    collectionTargets.AddTarget(location)
