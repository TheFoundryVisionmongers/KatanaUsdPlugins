# Copyright (c) 2024 The Foundry Visionmongers Ltd. All Rights Reserved.

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

import pytest
from fnpxr import Usd
from Katana import FnAttribute
import UsdExport.pluginRegistry

UsdExport.pluginRegistry.LoadPluginsFromKatanaResources(True)


class Test_UsdExportPlugins():
    shippedPlugins = [
        ("KatanaLightAPIWriter", "light"),
    ]

    @pytest.mark.parametrize("pluginName,pluginLocation", shippedPlugins)
    def test_getShippedPlugins(self, pluginName, pluginLocation):
        plugin = UsdExport.pluginRegistry.GetUsdExportPluginClass(pluginName, pluginLocation)
        assert plugin is not None

    def test_shippedKatanaLightAPIWriter_singleShader(self):
        plugin = UsdExport.pluginRegistry.GetUsdExportPluginClass("KatanaLightAPIWriter", "light")
        assert plugin is not None

        # Build the attributes to run through the plugin.
        materialAttrs = FnAttribute.GroupBuilder()
        geometryAttrs = FnAttribute.GroupBuilder()
        geometryAttrs.set("centerOfInterest", FnAttribute.FloatAttribute(12.3))

        attrs = {}
        attrs["material"] = materialAttrs.build()
        attrs["geometry"] = geometryAttrs.build()

        stage = Usd.Stage.CreateInMemory()
        stage.DefinePrim("/root/test_path")

        plugin.WritePrim(stage, "/root/test_path", attrs)

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_path")

        # Verify the prim.
        assert bool(prim)

        # Verify CoI.
        assert prim.HasAttribute("geometry:centerOfInterest")
        coiAttr = prim.GetAttribute("geometry:centerOfInterest")
        assert coiAttr.GetTypeName() == "double"
        assert not coiAttr.IsCustom()
        assert coiAttr.Get() == pytest.approx(12.3)
