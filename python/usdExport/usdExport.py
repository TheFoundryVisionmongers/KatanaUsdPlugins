# Copyright (c) 2018 The Foundry Visionmongers Ltd. All Rights Reserved.

import datetime
import logging
import os

from Katana import LookFileBakeAPI, Callbacks, FnAttribute
from Nodes3DAPI.Manifest import PyScenegraphAttrFnAttributeBridge as AttrBridge
from GeoAPI.Util import LookFileUtil
log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
pxrImported = False
try:
    from fnpxr import Usd, UsdShade, Sdf, Gf
    # These includes also require fnpxr
    from .UsdExport.material import WriteMaterial, WriteMaterialOverride, WriteMaterialAssign
    pxrImported = True
except ImportError as e:
    log.warning('Error while importing pxr module (%s). Is '
                '"[USD install]/lib/python" in PYTHONPATH?', e.message)


BaseOutputFormat = LookFileBakeAPI.BaseLookFileBakeOutputFormat

class UsdExport(BaseOutputFormat):
    """
    Class implementing a USD look file bake output format.
    """

    # Class Variables ---------------------------------------------------------

    DisplayName = "USDExport"
    FileExtension = ""
    PassFileExtension = "usda"

    # Protected Class Methods -------------------------------------------------

    @classmethod
    def writeOverrides(cls, stage, outputDictList, sharedOverrides):
        # We want to write the sharedoverride to root locations, but have these not loaded,
        # they should only be loaded as references.
        # We need to write locations in an order to ensure that bindings
        # and references will work, otherwise we end up with missing bindings
        # because, for example, the Material does not exist before the
        # GeoPrim tries to bind to it.
        locationTypeWritingOrder = ["material","all"]
        for (outputDict, rootName, rootType) in outputDictList:
            _ = rootType
            for locationTypePass in locationTypeWritingOrder:
                for locationPath, sharedOverridesKey in outputDict.iteritems():
                    # We take the full path as the override path.
                    # since the root will be handled by UsdIn
                    if not locationPath:
                        continue

                    # Check to see if we are meant to add this type of location
                    # yet.
                    attr_dict = sharedOverrides.get(sharedOverridesKey)
                    if not attr_dict:
                        continue
                    typeAttr = attr_dict.get("type")
                    # This section determines whether we want to write the
                    # currently reached scene graph location or not.
                    # As described above, we will need to write certain
                    # location types first to ensure we can bind against them.
                    locationType = ""
                    if typeAttr:
                        locationType = typeAttr.getValue()
                    if locationType != locationTypePass:
                        if locationType in locationTypeWritingOrder:
                            continue
                        elif locationTypePass == "all":
                                pass
                        else:
                            continue

                    if locationPath[0] != "/":
                        locationPath = "/"+locationPath
                    sdfLocationPath = Sdf.Path(locationPath)
                    if stage.GetPrimAtPath(sdfLocationPath):
                        continue
                    overridePrim = stage.OverridePrim(sdfLocationPath)

                    # Now loop through the data, if it doesn't exist in the UsdStage
                    # create the location and data, and then reference.  If it already
                    # exists, just reference! key is an integer, the index between
                    # the sharedOverrides and rootOverrides locations they relate to.
                    for attr_name in attr_dict.keys():
                        attribute = AttrBridge.scenegraphAttrToAttr(
                            attr_dict[attr_name])
                        if attr_name == "material":
                            if locationType == "material":
                                WriteMaterial(stage, sdfLocationPath, attribute)
                            else:
                                material = WriteMaterialOverride(
                                    stage, sdfLocationPath, overridePrim, 
                                    sharedOverridesKey, attribute)
                                WriteMaterialAssign(material, overridePrim)
                        if attr_name == "materialAssign":
                            if not attribute:
                                continue
                            materialPath = cls.GetRelativeUsdSdfPathFromAttribute(
                                rootName, attribute)
                            material = UsdShade.Material.Get(stage, materialPath)
                            if material:
                                WriteMaterialAssign(material, overridePrim)



    @classmethod
    def GetRelativeUsdSdfPathFromAttribute(cls, rootName, assignAttr):
        """ To be used with values from attributes like "materialAssign".
            Katana will provide the full scenegraph location path, but since
            the lookfilebake writes out from a given root, we need to remove
            the root prefix.
        """
        assignValue = str(assignAttr.getValue())
        return assignValue[(assignValue.find(rootName)+len(rootName)):]


    # Instance Methods --------------------------------------------------------

    def __init__(self, settings):
        super(UsdExport, self).__init__(settings)
        settings.material_variantset_initialised = False
        # FOR THE TESTER
        # please modify the following attributes to match your project
        # these will be exposed in future node iterations
        settings.passCount = 2
        settings.material_variantset_root_prim = "/root"
        settings.material_variantset_usd_filename = "kvariants.usda"
        settings.material_variantset_name = "car_mat_var"

    def writeSinglePass(self, passData):
        """
        @type passData: C{LookFileBakeAPI.LookFilePassData}
        @rtype: C{list} of C{str}
        @param passData: The data representing a single look file pass to be
            baked.
        @return: A list of paths to files which have been written.
        """
        # Get the file path for this pass from the given pass data
        filePath = passData.filePath

        # If the enclosing directory doesn't exist, then try to create it
        LookFileUtil.CreateLookFileDirectory(os.path.dirname(filePath))

        # Not deleting this code because it will be useful for debugging whilst
        # code is in progress!
        # print dir(passData)

        # print "Shared overrides"
        # print "-"*30
        # for k, v in passData.sharedOverridesDict.iteritems():
        #     print k, "=", v
        # print "="*30

        # print "Root overrides"
        # print "-"*30
        # if passData.rootOverrideDict:
        #     for k, v in passData.rootOverrideDict.iteritems():
        #         print k, "=", v

        # print "="*30

        # print "outputDictList"
        # print "-"*30
        # if passData.outputDictList:
        #     for (overrideDict, rootName, rootType) in passData.outputDictList:
        #         print overrideDict,
        #         print "rootName =", rootName
        #         print "rootType =", rootType
        # print "="*30

        def do_material_write(stage):
            # Iterate over materials
            for materialLocationPath, (locationType, materialScenegraphAttr) in \
                    passData.materialDict.iteritems():
                _ = locationType
                # Discard materials if the path is not a valid SdfPath.
                if not Sdf.Path.IsValidPathString(materialLocationPath):
                    log.warning('"%s" is not a valid SdfPath. Material will '
                                'be skipped.', materialLocationPath)
                    continue
                # Create a material
                materialSdfPath = Sdf.Path(materialLocationPath)

                # Convert from PyScenegraphAttribute to FnAttribute
                materialAttribute = \
                    AttrBridge.scenegraphAttrToAttr(materialScenegraphAttr)
                if materialAttribute:
                    WriteMaterial(
                        stage, materialSdfPath, materialAttribute)

            self.__class__.writeOverrides(
                stage, passData.outputDictList, passData.sharedOverridesDict)

        if self._settings["passCount"] > 1:
            # write all material data to the same variant file
            # (create on first pass, then append in subsequent passes)
            variant_path = os.path.join(\
                os.path.dirname(filePath),\
                self._settings["material_variantset_usd_filename"])

            if self._settings["material_variantset_initialised"]:
                stage = Usd.Stage.Open(variant_path)

                root_prim = stage.GetPrimAtPath(\
                    self._settings["material_variantset_root_prim"])

                variantset = root_prim.GetVariantSets().GetVariantSet(\
                    self._settings["material_variantset_name"])
            else:
                stage = Usd.Stage.CreateNew(variant_path)
                now = datetime.datetime.now()
                stage.SetMetadata(\
                    "comment",\
                    "Generated by Katana on {}".format(\
                        now.strftime("%d/%b/%Y %H:%M:%S")))

                root_prim = stage.DefinePrim(\
                    self._settings["material_variantset_root_prim"])

                variantset = root_prim.GetVariantSets().AddVariantSet(\
                    self._settings["material_variantset_name"])
                self._settings["material_variantset_initialised"] = True

            variant_name = passData.passName

            variantset.AddVariant(variant_name)
            variantset.SetVariantSelection(variant_name)

            with variantset.GetVariantEditContext():
                do_material_write(stage)
        else:
            # Create a new USD stage
            stage = Usd.Stage.CreateNew(filePath)
            do_material_write(stage)

        # Save the stage
        stage.GetRootLayer().Save()

        return [filePath]

# Only register the output format if the pxr module has been imported
# successfully.
if pxrImported:
    LookFileBakeAPI.RegisterOutputFormat(UsdExport)
