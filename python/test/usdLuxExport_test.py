# Copyright (c) 2024 The Foundry Visionmongers Ltd. All Rights Reserved.

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

import pytest

from fnpxr import Usd, UsdLux, Sdf

from Katana import FnAttribute
from UsdExport.light import (ParseLightsFromMaterialAttrs, _ResolveLightPrimType)
from UsdExport.light import (WriteLight, WriteLightList)
from UsdExport.lightLinking import (WriteLightLinking)


class Test_UsdExportUtils():
    data = [
        ("DiskLight", "DiskLight"),
        ("UsdLuxDiskLight", "DiskLight"),
        ("UsdLuxRectLight", "RectLight"),
        ("RectLight", "RectLight"),
        ("UsdLuxDistantLight", "DistantLight"),
        ("UsdLuxDomeLight", "DomeLight"),
        ("UsdLuxSphereLight", "SphereLight"),
        ("UsdLuxCylinderLight", "CylinderLight"),
        ("PxrRectLight", None),
        ("SomethingElse", None),
    ]

    @pytest.mark.parametrize("shaderName,expectedPrimType", data)
    def test_resolvePrimType(self, shaderName, expectedPrimType):
        primType = _ResolveLightPrimType(shaderName)
        assert primType == expectedPrimType

    data = [
        ("usdLightShader", "UsdLuxDiskLight", ("usd", "UsdLuxDiskLight")),
        ("pxrLightShader", "PxrDiskLight", ("pxr", "PxrDiskLight")),
        ("someCustomLightShader", "someCustomDiskLight", ("someCustom", "someCustomDiskLight")),
    ]

    @pytest.mark.parametrize("shaderType,shadername,expectedKey", data)
    def test_parseLightsFromMaterialAttrs(self, shaderType, shadername, expectedKey):
        materialAttrs = FnAttribute.GroupBuilder()
        materialAttrs.set(shaderType, FnAttribute.StringAttribute(shadername))

        result = ParseLightsFromMaterialAttrs(materialAttrs.build())

        assert expectedKey in result.keys()


class Test_UsdExport():

    def test_writeEmptyLight(self):
        # Build the attributes to export.
        lightAttrs = FnAttribute.GroupBuilder()

        # Create a stage to write to.
        stage = Usd.Stage.CreateInMemory()

        WriteLight(stage, "/root/test_light", lightAttrs.build())

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_light")

        assert bool(prim)
        assert not prim.HasAuthoredTypeName()
        assert not prim.HasAPI(UsdLux.LightAPI)

    def test_writeInvalidLightShader(self):
        # Build the attributes to export.
        lightAttrs = FnAttribute.GroupBuilder()
        lightAttrs.set("usdLightShader", FnAttribute.StringAttribute("UsdLuxInvalidLight"))
        lightAttrs.set("usdLightParams.someCustomAttr", FnAttribute.IntAttribute(123))
        lightAttrs.set("usdLightParams.intensity", FnAttribute.FloatAttribute(0.4))

        # Create a stage to write to.
        stage = Usd.Stage.CreateInMemory()

        WriteLight(stage, "/root/test_light", lightAttrs.build())

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_light")

        assert bool(prim)
        assert not prim.HasAuthoredTypeName()
        assert not prim.HasAPI(UsdLux.LightAPI)

        lightApi = UsdLux.LightAPI(prim)
        intensityAttr = lightApi.GetIntensityAttr()
        assert not intensityAttr.HasAuthoredValue()

        # Verify someCustomAttr.
        assert not prim.HasAttribute("someCustomAttr")

    def test_writeDiskLight(self):
        # Build the attributes to export.
        lightAttrs = FnAttribute.GroupBuilder()
        lightAttrs.set("usdLightShader", FnAttribute.StringAttribute("UsdLuxDiskLight"))
        lightAttrs.set("usdLightParams.radius", FnAttribute.FloatAttribute(5.1))
        lightAttrs.set("usdLightParams.color", FnAttribute.FloatAttribute((0.33, 0.66, 0.99), 3))
        lightAttrs.set("usdLightParams.enableColorTemperature", FnAttribute.FloatAttribute(1.0))
        lightAttrs.set("usdLightParams.someCustomAttr", FnAttribute.IntAttribute(123))

        # Create a stage to write to.
        stage = Usd.Stage.CreateInMemory()

        WriteLight(stage, "/root/test_light", lightAttrs.build())

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_light")

        # Verify the prim.
        assert bool(prim)
        assert prim.HasAuthoredTypeName()
        assert prim.HasAPI(UsdLux.LightAPI)
        assert prim.GetTypeName() == "DiskLight"

        diskLight = UsdLux.DiskLight(prim)

        # Verify radius.
        radiusAttr = diskLight.GetRadiusAttr()
        assert bool(radiusAttr)
        assert radiusAttr.GetTypeName() == "float"
        assert not radiusAttr.IsCustom()
        assert radiusAttr.Get() == pytest.approx(5.1)

        # Verify color.
        colorAttr = diskLight.GetColorAttr()
        assert bool(colorAttr)
        assert colorAttr.GetTypeName() == "color3f"
        assert not colorAttr.IsCustom()

        colorValues = colorAttr.Get()
        assert len(colorValues) == 3
        assert colorValues[0] == pytest.approx(0.33)
        assert colorValues[1] == pytest.approx(0.66)
        assert colorValues[2] == pytest.approx(0.99)

        # Verify enableColorTemperature.
        ectAttr = diskLight.GetEnableColorTemperatureAttr()
        assert ectAttr.GetTypeName() == "bool"
        assert not ectAttr.IsCustom()
        assert ectAttr.Get()

        # Verify someCustomAttr.
        assert prim.HasAttribute("someCustomAttr")

        customAttr = prim.GetAttribute("someCustomAttr")
        assert customAttr.IsCustom()
        assert customAttr.Get() == 123


class Test_UsdExportNamespacedParameters():

    def test_writeShadowEnableFalse(self):
        # Build the attributes to export.
        lightAttrs = FnAttribute.GroupBuilder()
        lightAttrs.set("usdLightShader", FnAttribute.StringAttribute("UsdLuxDiskLight"))
        lightAttrs.set("usdLightParams.shadowEnable", FnAttribute.IntAttribute(0))

        # Create a stage to write to.
        stage = Usd.Stage.CreateInMemory()

        WriteLight(stage, "/root/test_light", lightAttrs.build())

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_light")

        # Verify the prim.
        assert bool(prim)
        assert prim.HasAuthoredTypeName()
        assert prim.HasAPI(UsdLux.LightAPI)
        assert prim.GetTypeName() == "DiskLight"

        shadowApi = UsdLux.ShadowAPI(prim)
        shadowEnableAttr = shadowApi.GetShadowEnableAttr()
        assert bool(shadowEnableAttr)
        assert not bool(shadowEnableAttr.Get())

    def test_writeShadowEnableTrue(self):
        # Build the attributes to export.
        lightAttrs = FnAttribute.GroupBuilder()
        lightAttrs.set("usdLightShader", FnAttribute.StringAttribute("UsdLuxDiskLight"))
        lightAttrs.set("usdLightParams.shadowEnable", FnAttribute.IntAttribute(1))

        # Create a stage to write to.
        stage = Usd.Stage.CreateInMemory()

        WriteLight(stage, "/root/test_light", lightAttrs.build())

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_light")

        # Verify the prim.
        assert bool(prim)
        assert prim.HasAuthoredTypeName()
        assert prim.HasAPI(UsdLux.LightAPI)
        assert prim.GetTypeName() == "DiskLight"

        shadowApi = UsdLux.ShadowAPI(prim)
        shadowEnableAttr = shadowApi.GetShadowEnableAttr()
        assert bool(shadowEnableAttr)
        assert bool(shadowEnableAttr.Get())

    def test_writeShadowFalloffGamma(self):
        # Build the attributes to export.
        lightAttrs = FnAttribute.GroupBuilder()
        lightAttrs.set("usdLightShader", FnAttribute.StringAttribute("UsdLuxDiskLight"))
        lightAttrs.set("usdLightParams.shadowFalloffGamma", FnAttribute.FloatAttribute(3.4))

        # Create a stage to write to.
        stage = Usd.Stage.CreateInMemory()

        WriteLight(stage, "/root/test_light", lightAttrs.build())

        # Query the stage for the expected export data.
        prim = stage.GetPrimAtPath("/root/test_light")

        # Verify the prim.
        assert bool(prim)
        assert prim.HasAuthoredTypeName()
        assert prim.HasAPI(UsdLux.LightAPI)
        assert prim.GetTypeName() == "DiskLight"

        shadowApi = UsdLux.ShadowAPI(prim)
        shadowFalloffGammaAttr = shadowApi.GetShadowFalloffGammaAttr()
        assert bool(shadowFalloffGammaAttr)
        assert shadowFalloffGammaAttr.Get() == pytest.approx(3.4)


class Test_UsdExportLightList():

    def test_writeLightList(self):
        stage = Usd.Stage.CreateInMemory()

        # Create a light which has a concrete type.
        stage.DefinePrim("/root/world/lgt/typed_light", "RectLight")
        # Create a light which has the generic "Light" type Katana uses.
        stage.DefinePrim("/root/world/lgt/generic_type_light", "Light")
        # Create a location with the UsdLuxLightAPI applied API.
        appliedApiLight = stage.DefinePrim("/root/world/lgt/applied_api_light")
        lightAPI = UsdLux.LightAPI(appliedApiLight)
        lightAPI.Apply(appliedApiLight)

        stage.SetDefaultPrim(stage.GetPrimAtPath("/root"))

        WriteLightList(stage)

        defaultPrim = stage.GetDefaultPrim()
        assert bool(defaultPrim)

        luxListAPI = UsdLux.ListAPI(defaultPrim)
        assert luxListAPI.GetLightListCacheBehaviorAttr().Get() == "consumeAndHalt"

        entries = luxListAPI.GetLightListRel().GetTargets()
        assert len(entries) == 3
        assert Sdf.Path("/root/world/lgt/typed_light") in entries
        assert Sdf.Path("/root/world/lgt/generic_type_light") in entries
        assert Sdf.Path("/root/world/lgt/applied_api_light") in entries


class Test_UsdExportLightLinking():

    def test_writeLightLinkingSingle(self):
        stage = Usd.Stage.CreateInMemory()
        lightPrim = stage.DefinePrim("/root/world/lgt/light1", "RectLight")
        stage.DefinePrim("/root/world/geo/mesh1", "Cube")

        mesh1LightListAttributeBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder.set("path", FnAttribute.StringAttribute("/root/world/lgt/light1"))
        light1LinkingAttrBuilder.set("enable", FnAttribute.IntAttribute(1))
        light1LinkingAttrBuilder.set("geoShadowEnable", FnAttribute.IntAttribute(0))

        mesh1LightListAttributeBuilder.set("root_world_lgt_light", light1LinkingAttrBuilder.build())

        lightDict = {"/root/world/lgt/light1": Sdf.Path("/root/world/lgt/light1")}
        rootName = "root"

        WriteLightLinking(stage, "/root/world/geo/mesh1", mesh1LightListAttributeBuilder.build(),
                          lightDict, rootName)

        # Verify light linking.
        lightLinkCollection = Usd.CollectionAPI(lightPrim, "lightLink")
        # Verify lightLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(lightLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 1
        assert "/root/world/geo/mesh1" in collectionIncludeTargetsList
        # Verify lightLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(lightLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

        # Verify shadow linking.
        shadowLinkCollection = Usd.CollectionAPI(lightPrim, "shadowLink")
        # Verify shadowLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(shadowLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 0
        # Verify shadowLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(shadowLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 1
        assert "/root/world/geo/mesh1" in collectionExcludeTargetsList

    def test_writeLightLinkingMultiple(self):
        stage = Usd.Stage.CreateInMemory()
        lightPrim = stage.DefinePrim("/root/world/lgt/light1", "RectLight")
        stage.DefinePrim("/root/world/geo/mesh1", "Cube")
        stage.DefinePrim("/root/world/geo/mesh2", "Sphere")

        mesh1LightListAttributeBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder.set("path", FnAttribute.StringAttribute("/root/world/lgt/light1"))
        light1LinkingAttrBuilder.set("enable", FnAttribute.IntAttribute(1))
        light1LinkingAttrBuilder.set("geoShadowEnable", FnAttribute.IntAttribute(0))
        mesh1LightListAttributeBuilder.set("root_world_lgt_light", light1LinkingAttrBuilder.build())

        mesh2LightListAttributeBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder.set("path", FnAttribute.StringAttribute("/root/world/lgt/light1"))
        light1LinkingAttrBuilder.set("enable", FnAttribute.IntAttribute(1))
        light1LinkingAttrBuilder.set("geoShadowEnable", FnAttribute.IntAttribute(0))
        mesh2LightListAttributeBuilder.set("root_world_lgt_light", light1LinkingAttrBuilder.build())

        lightDict = {"/root/world/lgt/light1": Sdf.Path("/root/world/lgt/light1")}
        rootName = "root"

        WriteLightLinking(stage, "/root/world/geo/mesh1", mesh1LightListAttributeBuilder.build(),
                          lightDict, rootName)
        WriteLightLinking(stage, "/root/world/geo/mesh2", mesh2LightListAttributeBuilder.build(),
                          lightDict, rootName)

        # Verify light linking.
        lightLinkCollection = Usd.CollectionAPI(lightPrim, "lightLink")
        # Verify lightLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(lightLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 2
        assert "/root/world/geo/mesh1" in collectionIncludeTargetsList
        assert "/root/world/geo/mesh2" in collectionIncludeTargetsList
        # Verify lightLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(lightLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

        # Verify shadow linking.
        shadowLinkCollection = Usd.CollectionAPI(lightPrim, "shadowLink")
        # Verify shadowLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(shadowLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 0
        # Verify shadowLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(shadowLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 2
        assert "/root/world/geo/mesh1" in collectionExcludeTargetsList
        assert "/root/world/geo/mesh2" in collectionExcludeTargetsList

        #raise Exception(stage.Flatten().ExportToString())

    def test_writeLightLinkingUnlinked(self):
        stage = Usd.Stage.CreateInMemory()
        lightPrim = stage.DefinePrim("/root/world/lgt/light1", "RectLight")
        stage.DefinePrim("/root/world/geo/mesh1", "Cube")

        mesh1LightListAttributeBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder.set("path", FnAttribute.StringAttribute("/root/world/lgt/light1"))

        mesh1LightListAttributeBuilder.set("root_world_lgt_light", light1LinkingAttrBuilder.build())

        lightDict = {"/root/world/lgt/light1": Sdf.Path("/root/world/lgt/light1")}
        rootName = "root"

        WriteLightLinking(stage, "/root/world/geo/mesh1", mesh1LightListAttributeBuilder.build(),
                          lightDict, rootName)

        # Verify light linking.
        lightLinkCollection = Usd.CollectionAPI(lightPrim, "lightLink")
        # Verify lightLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(lightLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 0
        # Verify lightLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(lightLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

        # Verify shadow linking.
        shadowLinkCollection = Usd.CollectionAPI(lightPrim, "shadowLink")
        # Verify shadowLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(shadowLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 0
        # Verify shadowLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(shadowLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

    def test_writeLightLinkingLightNoShadow(self):
        stage = Usd.Stage.CreateInMemory()
        lightPrim = stage.DefinePrim("/root/world/lgt/light1", "RectLight")
        stage.DefinePrim("/root/world/geo/mesh1", "Cube")

        mesh1LightListAttributeBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder.set("path", FnAttribute.StringAttribute("/root/world/lgt/light1"))
        light1LinkingAttrBuilder.set("enable", FnAttribute.IntAttribute(1))

        mesh1LightListAttributeBuilder.set("root_world_lgt_light", light1LinkingAttrBuilder.build())

        lightDict = {"/root/world/lgt/light1": Sdf.Path("/root/world/lgt/light1")}
        rootName = "root"

        WriteLightLinking(stage, "/root/world/geo/mesh1", mesh1LightListAttributeBuilder.build(),
                          lightDict, rootName)

        # Verify light linking.
        lightLinkCollection = Usd.CollectionAPI(lightPrim, "lightLink")
        # Verify lightLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(lightLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 1
        assert "/root/world/geo/mesh1" in collectionIncludeTargetsList
        # Verify lightLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(lightLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

        # Verify shadow linking.
        shadowLinkCollection = Usd.CollectionAPI(lightPrim, "shadowLink")
        # Verify shadowLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(shadowLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 0
        # Verify shadowLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(shadowLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

    def test_writeLightLinkingShadowNoLight(self):
        stage = Usd.Stage.CreateInMemory()
        lightPrim = stage.DefinePrim("/root/world/lgt/light1", "RectLight")
        stage.DefinePrim("/root/world/geo/mesh1", "Cube")

        mesh1LightListAttributeBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder = FnAttribute.GroupBuilder()
        light1LinkingAttrBuilder.set("path", FnAttribute.StringAttribute("/root/world/lgt/light1"))
        light1LinkingAttrBuilder.set("geoShadowEnable", FnAttribute.IntAttribute(1))

        mesh1LightListAttributeBuilder.set("root_world_lgt_light", light1LinkingAttrBuilder.build())

        lightDict = {"/root/world/lgt/light1": Sdf.Path("/root/world/lgt/light1")}
        rootName = "root"

        WriteLightLinking(stage, "/root/world/geo/mesh1", mesh1LightListAttributeBuilder.build(),
                          lightDict, rootName)

        # Verify light linking.
        lightLinkCollection = Usd.CollectionAPI(lightPrim, "lightLink")
        # Verify lightLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(lightLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 0
        # Verify lightLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(lightLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0

        # Verify shadow linking.
        shadowLinkCollection = Usd.CollectionAPI(lightPrim, "shadowLink")
        # Verify shadowLink includes.
        collectionIncludeTargets = Usd.CollectionAPI.GetIncludesRel(shadowLinkCollection)
        collectionIncludeTargetsList = collectionIncludeTargets.GetTargets()
        assert len(collectionIncludeTargetsList) == 1
        assert "/root/world/geo/mesh1" in collectionIncludeTargetsList
        # Verify shadowLink excludes.
        collectionExcludeTargets = Usd.CollectionAPI.GetExcludesRel(shadowLinkCollection)
        collectionExcludeTargetsList = collectionExcludeTargets.GetTargets()
        assert len(collectionExcludeTargetsList) == 0
