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
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)

def WriteLightLinking(prim, linkingData):
    """
    Converts the given light material C{GroupAttribute} into a C{Usd.Prim}.

    @type prim: C{Usd.Prim}
    @type linkingData: C{dict} of C{str} : C{list} of C{str}
    @param prim: The prim to write the linking data to.
    @param linkingData: A dictionary of linking collection type keys mapping to
        a list of the paths included in this linking collection.
    """

    lightLinkIncludes = linkingData.get("lightLinkIncludes", None)
    lightLinkExcludes = linkingData.get("lightLinkExcludes", None)
    shadowLinkIncludes = linkingData.get("shadowLinkIncludes", None)
    shadowLinkExcludes = linkingData.get("shadowLinkExcludes", None)

    def createParam(collectionName, paramFunc, locations):
        collection = Usd.CollectionAPI(prim, collectionName)
        collectionTargets = paramFunc(collection)
        for location in locations:
            collectionTargets.AddTarget(location)

    if lightLinkIncludes:
        createParam("lightLink",
                    Usd.CollectionAPI.CreateIncludesRel,
                    lightLinkIncludes)
    if lightLinkExcludes:
        createParam("lightLink",
                    Usd.CollectionAPI.CreateExcludesRel,
                    lightLinkExcludes)
    if shadowLinkIncludes:
        createParam("shadowLink",
                    Usd.CollectionAPI.CreateIncludesRel,
                    shadowLinkIncludes)
    if shadowLinkExcludes:
        createParam("shadowLink",
                    Usd.CollectionAPI.CreateExcludesRel,
                    shadowLinkExcludes)


def GetLinkingData(bakeNodeName, variantName):
    """
    Converts the given light material C{GroupAttribute} into a
    C{Usd.Prim}.

    @type bakeNodeName: C{Usd.Prim}
    @type variantName: C{dict} of C{str} : C{list}
    @rtype: C{dict} of C{str} : C{dict} of C{str} : C{list} of C{str}
    @param bakeNodeName: The USD stage to write the light to.
    @param variantName: The prim to write the linking data to.
    @return: A C{dict} mapping a light path (C{str}) to a C{dict} of linking
        collection names (C{str}) mapping to a list of the paths included in
        that collection.
    """

    def sanitizeCEL(paths):
        sanitizedPaths = []
        for path in paths:
            splitPaths = path.split()
            for splitPath in splitPaths:
                splitPath = splitPath.replace('(', '')
                splitPath = splitPath.replace(')', '')
                sanitizedPaths.append(splitPath)
        return sanitizedPaths

    node = NodegraphAPI.GetNode(bakeNodeName)
    if node is None:
        return None

    port = None
    if variantName is None:
        # The first input port will be the 'orig' so pick the only variant.
        port = node.getInputPortByIndex(1)
    else:
        port = node.getInputPort(variantName)
    if port is None:
        log.warning("Unable get light linking data for bake from node '%s'",
                    bakeNodeName)
        return None

    connectedPort = port.getConnectedPort(0)
    if connectedPort is None:
        return None

    linkingData = {}
    linkingDict = _cookLightLinkingAttrs(connectedPort.getNode())
    if linkingDict:
        for path, linkAttrs in linkingDict.items():
            enableOnCEL = sanitizeCEL(
                linkAttrs.getChildByName("enable.onCEL").getData())
            enableOffCEL = sanitizeCEL(
                linkAttrs.getChildByName("enable.offCEL").getData())
            geoShadowEnableOnCEL = sanitizeCEL(
                linkAttrs.getChildByName("geoShadowEnable.onCEL").getData())
            geoShadowEnableOffCEL = sanitizeCEL(
                linkAttrs.getChildByName("geoShadowEnable.offCEL").getData())

            linkingData[path] = {"lightLinkIncludes": enableOnCEL,
                                    "lightLinkExcludes": enableOffCEL,
                                    "shadowLinkIncludes":
                                        geoShadowEnableOnCEL,
                                    "shadowLinkExcludes":
                                        geoShadowEnableOffCEL}

    return linkingData

def _cookLightLinkingAttrs(node):
    linkingDict = {} # Maps light path to linking attribute.

    client = Nodes3DAPI.CreateClient(node)
    locationData = client.cookLocation("/root/world/")
    locationAttrs = locationData.getAttrs()
    if locationAttrs is None:
        return None
    lightListAttrs = locationAttrs.getChildByName("lightList")
    if lightListAttrs is None:
        return None

    for _, attr in lightListAttrs.childList():
        pathAttr = attr.getChildByName("path")
        linkingAttr = attr.getChildByName("linking")
        if pathAttr and linkingAttr:
            linkingDict[pathAttr.getData()[0]] = linkingAttr

    return linkingDict
