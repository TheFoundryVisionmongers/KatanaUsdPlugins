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

import sys

from UsdExport.common import (GetShaderNodeFromRegistry)
from UsdExport.pluginAPI import BaseUsdExportPlugin
from UsdExport.light import (ParseLightsFromMaterialAttrs)

class SdrLightUsdExportPlugin(BaseUsdExportPlugin):
    """
    A UsdExport plugin to write out parameters from the Sdr Registry to the
    given light location. A shader name is matched against nodes in the Sdr and
    attributes matching inputs on that node are written to the prim with the
    type specified by the input.
    """
    priority = sys.maxsize
    rendererPrefixMapping = {
        "prman" : "ri",
        "dl": "nsi"
    }

    @staticmethod
    def WritePrim(stage, sdfLocationPath, attrDict):
        """
        Gets the node from the Sdr Registry for a given light shader on a light
        location and writes attributes matching inputs to that node with the
        correct type as specified in that input.

        @type stage: C{Usd.Stage}
        @type sdfLocationPath: C{Sdf.Path}
        @type attrDict: C{dict}
        @param stage: The stage to write to.
        @param sdfLocationPath: The path to the location in the stage to write
            the Sdr parameters to.
        @param attrDict: A dictionary containing the Katana parameters for the
            light location.
        """
        prim = stage.GetPrimAtPath(sdfLocationPath)
        # Check for a material attribute group.
        materialAttrs = attrDict.get("material", None)
        if materialAttrs is None:
            return
        # Check for a valid shader and its accompanying attributes.
        lights = ParseLightsFromMaterialAttrs(materialAttrs)
        for lightShaderName, lightShaderAttrs in lights.items():
            (renderer, lightShaderName) = lightShaderName
            # Only continue if the shader exists in the Sdr Registry.
            node = GetShaderNodeFromRegistry(lightShaderName)
            if node is None:
                continue

            for attrName, attr in lightShaderAttrs.childList():
                nodeInput = node.GetInput(attrName)
                if nodeInput is None:
                    continue

                primAttr = SdrLightUsdExportPlugin.CreatePrefixedAttribute(
                    prim,
                    nodeInput,
                    renderer,
                    node.GetContext())

                attrValue = attr.getData()
                if len(attrValue) > 1:
                    attrValue = tuple(attrValue)
                elif len(attrValue) == 1:
                    attrValue = attrValue[0]
                else:
                    continue

                primAttr.Set(attrValue)

    @staticmethod
    def CreatePrefixedAttribute(prim, nodeInput, renderer, context):
        """
        Gets the node from the Sdr Registry for a given light shader on a light
        location and writes attributes matching inputs to that node with the
        correct type as specified in that input.

        @type prim: C{Usd.Prim}
        @type nodeInput: C{Sdr.ShaderProperty}
        @type renderer: C{str} or C{None}
        @type context: C{str} or C{None}
        @rtype: C{Usd.Attribute}
        @param prim: The prim to create the attribute on.
        @param nodeInput: The Sdr node input for this attribute.
        @param renderer: The renderer name which prefixes the shader.
        @param context: The context of the attribute, typically "light".
        @return: The created Usd Attribute.
        """
        rendererPrefix = SdrLightUsdExportPlugin.rendererPrefixMapping.get(
            renderer, renderer)

        attributeName = "inputs:"
        if rendererPrefix:
            attributeName += rendererPrefix + ":"
        if context:
            attributeName += context + ":"
        if nodeInput:
            attributeName += nodeInput.GetName()

        return prim.CreateAttribute(
            attributeName, nodeInput.GetTypeAsSdfType()[0])

PluginRegistry = [
    ("UsdExport", 1, "SdrLightParamWriter", (["light"],
        SdrLightUsdExportPlugin))
]
