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

import UsdKatana
from UsdExport.pluginAPI import BaseUsdExportPlugin
from UsdExport.light import (ParseLightsFromMaterialAttrs)

class KatanaLightApiUsdExportPlugin(BaseUsdExportPlugin):
    """
    A UsdExport plugin to write out the Katana LightAPI attributes for a given
    light location.
    """
    priority = sys.maxsize

    @staticmethod
    def WritePrim(stage, sdfLocationPath, attrDict):
        """
        Writes the Katana LightAPI attributes to the given Sdf path location
        where these values have been modified from their defaults.

        @type stage: C{Usd.Stage}
        @type sdfLocationPath: C{Sdf.Path}
        @type attrDict: C{dict}
        @param stage: The stage to write to.
        @param sdfLocationPath: The path to the location in the stage to write
            the Katana LightAPI to.
        @param attrDict: A dictionary containing the Katana parameters for the
            light location.
        """
        prim = stage.GetPrimAtPath(sdfLocationPath)
        if not prim:
            return
        # Check for a material attribute group.
        materialAttrs = attrDict.get("material", None)
        geometryAttrs = attrDict.get("geometry", None)
        if not materialAttrs and not geometryAttrs:
            return

        lightApi = UsdKatana.KatanaLightAPI(prim)
        lightApi.Apply(prim)

        if materialAttrs:
            lights = ParseLightsFromMaterialAttrs(materialAttrs)
            for lightShaderName, _ in lights.items():
                (renderer, lightShaderName) = lightShaderName

                idAttr = lightApi.CreateIdAttr()
                entries = list(idAttr.Get() if
                    idAttr.Get() is not None else [])
                entries.extend(["%s:%s" % (renderer, lightShaderName)])
                idAttr.Set(entries)

        if geometryAttrs:
            coiAttr = geometryAttrs.getChildByName("centerOfInterest")
            if coiAttr:
                coiValue = coiAttr.getData()[0]
                lightApi.CreateCenterOfInterestAttr(coiValue)

PluginRegistry = [
    ("UsdExport", 1, "KatanaLightAPIWriter", (["light"],
        KatanaLightApiUsdExportPlugin))
]
