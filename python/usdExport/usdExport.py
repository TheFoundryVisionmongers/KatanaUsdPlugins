# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

import datetime
import logging
import os

from Katana import LookFileBakeAPI, Callbacks, FnAttribute
from Nodes3DAPI.Manifest import PyScenegraphAttrFnAttributeBridge as AttrBridge
from LookFileBakeAPI import LookFileUtil, LookFileBakeException
log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
pxrImported = False
try:
    from fnpxr import Usd, UsdShade, Sdf, Gf
    # These includes also require fnpxr
    from UsdExport.material import (WriteMaterial, WriteMaterialAssign,
        WriteChildMaterial)
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

    DisplayName = "UsdExport"
    FileExtension = ""
    PassFileExtension = "usda"
    Hidden = True
    LocationTypeWritingOrder = ["material", "all"]

    # Protected Class Methods -------------------------------------------------

    @classmethod
    def __checkTypeWritingOrder(cls, locationType, locationTypePass):
        # This section determines whether we want to write the
        # currently reached scene graph location or not.
        # As described above, we will need to write certain
        # location types first to ensure we can bind against them.
        # Special case for adding child materials
        if locationTypePass is None:
            return True
        if locationType != locationTypePass:
            if locationType in cls.LocationTypeWritingOrder:
                return False
            elif locationTypePass == "all":
                return True
            else:
                return False
        return True

    @staticmethod
    def locationPathtoSdfPath(locationPath, rootPrimName):
        if not locationPath.startswith("/"):
            locationPath = "/" + locationPath
        if rootPrimName:
            locationPath = rootPrimName + locationPath
        # ":" is not supported in an SdfPath
        locationPath = locationPath.replace(":", "_")
        return Sdf.Path(locationPath)

    @classmethod
    def WriteOverride(cls, stage, overrideDict, sharedOverridesDict,
                      locationTypePass, location, rootName, rootPrimName,
                      materialDict):
        # pylint: disable=too-many-branches,
        # pylint: disable=too-many-locals
        # pylint: disable=too-many-arguments
        """
        This method is responsible for writing all of the overrides
        from the passData.shaderdOverrides dictionary.
        We use the outputDict which has key value pairs of the location
        from katana, to the key of the sharedOverride from the
        sharedOverridesDict dictionary.
        This method is also called recursively, and in the recursive calls
        we also pass in outputDictKey and parentMaterialSdfList.
        @param outputDictKey:
        @param parentMaterialSdfList:
        @type stage: C{Usd.Stage}
        @type outputDict: C{dict} of C{str} : C{int}
        @type sharedOverridesDict: C{dict} of C{int} : (
            C{dict} of (
                    C{str} : C{FnAttribute.Attribute}
                )
            )
        @type locationTypePass: C{str}
        @type location: C{str}
        @type rootName: C{str}
        @type rootPrimName: C{str}
        @type materialDict: C{dict} of C{str} : C{Sdf.Path}
        @param stage: The C{Usd.Stage} to write data to.
        @param overrideDict: See C{LookFilePassData} for more details.
        @param sharedOverridesDict: See C{LookFilePassData} for more details.
        @param locationTypePass: The current locationTypes to allow writing.
            See WriteOverrides for more info.
        @param location: The Katana location path for the
        @param rootName: The root name from the overrideDictList, see
            C{LookFilePassData} for more details.
        @param rootPrimName: The name of the root C{Usd.Prim} for the given
            stage.
        @param materialDict: See C{LookFilePassData} for more details.
        """
        # We take the full path as the override path.
        # since the root will be handled by UsdIn
        if not location:
            return
        sharedOverridesKey = overrideDict[location]
        # Check to see if we are meant to add this type of location
        # yet.
        attrDict = sharedOverridesDict.get(sharedOverridesKey)
        if not attrDict:
            return
        typeAttr = attrDict.get("type")
        locationType = ""
        if typeAttr:
            locationType = typeAttr.getValue()
        if not cls.__checkTypeWritingOrder(locationType, locationTypePass):
            return

        sdfLocationPath = cls.locationPathtoSdfPath(location, rootPrimName)
        # By default create an override prim, any other node creation should
        # happen before the point of creating this prim.
        createOverridePrim = True

        # material
        materialAttribute = attrDict.get("material")
        # Write materialLocations first, as we may alter the sdfLocationPath
        # if we are writing to a child location.
        if materialAttribute is not None and locationType == "material":
            # Important: Here we override the sdfLocationPath to match
            # the one we are setting for the child material. This is such
            # that C{sdfLocationPath} can be re-used regardless as to
            # whether this location is a child material and has had its
            # hierarchy altered, or not.
            sdfLocationPath = cls.writeMaterialAttribute(
                stage, materialAttribute, location, sdfLocationPath,
                materialDict)
            createOverridePrim = False

        # Create an overridePrim if not disabled, and then get the prim
        # we are currently working on to add any other extra data.
        if createOverridePrim:
            stage.OverridePrim(sdfLocationPath)
        prim = stage.GetPrimAtPath(sdfLocationPath)

        # layout
        layout = attrDict.get("layout")
        if layout is not None:
            # Here is where we could write the layout information.
            pass

        # materialAssign
        materialAssignAttribute = attrDict.get("materialAssign")
        if materialAssignAttribute is not None:
            cls.writeMaterialAssign(materialDict, stage, rootName,
                                        materialAssignAttribute, prim)

    @staticmethod
    def writeMaterialAttribute(stage, materialAttribute, location,
                               sdfLocationPath, materialDict):
        """
        Writes a Material prim to the USD Stage, and returns the C{Sdf.Path}
        for where it wrote to. Handles child materials by writing them as
        siblings and using the C{BaseMaterial} mechanisms to ensure we
        utilise the `specializes` composition arc in USD.
        @type stage: C{Usd.Stage}
        @type materialAttribute: C{FnAttribute.GroupAttribute}
        @type location: C{str}
        @type sdfLocationPath: C{Sdf.Path}
        @type materialDict: C{dict} of C{ C{str} : C{Sdf.Path} }
        @rtype: C{Sdf.Path}
        @param stage: The C{Usd.Stage} to write data to.
        @param materialAttribute: The `material` attribute from the
            C{location}.
        @param location: The Katana location path for the current iteration
            relative to the C{rootName}.
        @param sdfLocationPath: The location this C{USD.Material} should be
            written to.  This is changed if we are writing a child material.
        @param materialDict: A dictionary containing a mapping between the
            material location paths from Katana, and the SdfPath in the USD
            Stage.
        @return: Returns the C{Sdf.Path} for the material being written.
            If this is not a child material, C{sdfLocationPath} will be
            returned.
        """
        lastHierarchyIndex = location.rfind("/")
        parentPath = None
        if lastHierarchyIndex > 0:
            parentPath = location[:lastHierarchyIndex]
            locationName = location[lastHierarchyIndex + 1:]
        if parentPath is not None and parentPath in materialDict:
            # This is a child material.
            parentMaterialPath = materialDict[parentPath].pathString
            childMaterialPath = Sdf.Path(parentMaterialPath + "_" +
                                         locationName)
            parentSdfPaths = []
            while parentPath in materialDict:
                parentSdfPaths.append(materialDict[parentPath])
                lastHierarchyIndex = parentPath.rfind("/")
                if lastHierarchyIndex <= 0:
                    break
                parentPath = parentPath[:lastHierarchyIndex]
            # Do not create an overridePrim for following attributes
            WriteChildMaterial(stage, childMaterialPath,
                               materialAttribute, parentSdfPaths)
            materialDict[location] = childMaterialPath
            return childMaterialPath
        WriteMaterial(stage, sdfLocationPath, materialAttribute)
        materialDict[location] = sdfLocationPath
        return sdfLocationPath

    @classmethod
    def writeOverrides(cls, stage, outputDictList, sharedOverridesDict,
                       rootPrimName, materialDict):
        """
        Loops through a sorted list of the output information and then loops
        through the class list of C{LocationTypeWritingOrder} and calls
        C{WriteOverride} for each path we find in the C{overrideDict}.
        We must sort the overrideDict keys in order to ensure we work from top
        to bottom with regard to hierarchies, since the locations are stored
        as strings, which could result in trying to populate a leaf before
        a branch; causing issues with parent-child material relationships.
        We want to write the sharedoverride to root locations, but have these
        not loaded, they should only be loaded as references.
        We need to write locations in an order to ensure that bindings
        and references will work, otherwise we end up with missing bindings
        because, for example, the Material does not exist before the
        GeoPrim tries to bind to it.

        @type stage: C{Usd.Stage}
        @type outputDictList: C{list}
        @type sharedOverridesDict: C{dict}
        @type rootPrimName: C{str}
        @param stage: The UsdStage to write data to.
        @param outputDictList: See the C{LookFilePassData} object for more
            details.
        @param sharedOverridesDict: See the C{LookFilePassData} object for more
            details.
        @param rootPrimName: The name of the root C{Usd.Prim} of the
            C{Usd.Stage}
        """
        for (overrideDict, rootName, rootType) in outputDictList:
            _ = rootType
            outputDictKeys = sorted(overrideDict.keys())
            for locationTypePass in cls.LocationTypeWritingOrder:
                for location in outputDictKeys:
                    cls.WriteOverride(stage, overrideDict,
                                      sharedOverridesDict,
                                      locationTypePass,
                                      location,
                                      rootName, rootPrimName,
                                      materialDict)

    @classmethod
    def writeMaterialAssign(cls, materialDict, stage, rootName, attribute,
                            overridePrim):

        assignValue = str(attribute.getValue())
        materialPath = cls.GetRelativeUsdSdfPath(
            materialDict, rootName, assignValue)

        if materialPath:
            material = UsdShade.Material.Get(stage, materialPath)
            if material:
                WriteMaterialAssign(material, overridePrim)

    @classmethod
    def GetRelativeUsdSdfPath(cls, materialDict, rootName, location):
        """
        To be used with values from attributes like "materialAssign".
        Katana will provide the full scenegraph location path, but since
        the LookFileBakeAPI writes out from a given root, we need to remove
        the root prefix; since this is not written to the USD file. We also
        need to remap child material paths, so we pass in the materialDict
        in order to get the SdfPath for the material from the original location
        in Katana.

        @type materialDict: C{dict} of C{ C{str} : C{Sdf.Path} }
        @type rootName: C{str}
        @type location: C{str}
        @rtype: C{str} C{None}
        @param materialDict: The dictionary of katana relative paths to the
            location of L{rootName}, and their C{Usd.Sdf} paths. This is
            filled in whilst populating the materials in the Usd Stage first.
        @param rootName: Refer to L{LookFilePassData} for more information
        @param location: Katana location
        @return: The material path to the
        """
        if location in materialDict:
            return materialDict[location]
        rfindIndex = len(location)
        # We need to add the path separators to ensure we are not just
        # finding the rootName word.
        # This does mean we need to take these extra characters into
        # account when using rfind, since we will strip the leading,
        # or closing "/" which we will need.
        rootNamePath = "/" + rootName + "/"
        rfindIndex = location.rfind(rootNamePath, 0, rfindIndex)
        if rfindIndex < 0:
            return None
        #Ensure we start reading with a closing "/"
        materialPath = location[rfindIndex + len(rootNamePath) - 1:]
        if materialPath in materialDict:
            return materialDict[materialPath]
        # If we don't find the path first try, it means we may be dealing
        # with a materialAssignment path with multiple duplicate levels in
        # the hierarchy, so we must check until we find our valid relative
        # material path.
        materialPath = location[rfindIndex:]
        #Ensure we start reading with a closing "/"
        rfindIndex = rfindIndex + 1
        while materialPath and materialPath not in materialDict:
            rfindIndex = location.rfind(rootNamePath, 0, rfindIndex)
            # If we can no longer find the rootNamePath.
            if rfindIndex < 0:
                return None
            materialPath = location[rfindIndex:]
            rfindIndex = rfindIndex + 1
        return materialDict[materialPath]


    # Instance Methods --------------------------------------------------------

    def __init__(self, settings):
        super(UsdExport, self).__init__(settings)
        self._settings.materialVariantSetInitialized = False
        self._settings.defaultMaterialVariant = None

    def writeSinglePass(self, passData):
        """
        @param passData: The data representing a single look file pass to be
            baked.
        @type passData: C{LookFileBakeAPI.LookFilePassData}
        @return: A list of paths to files which have been written.
        @rtype: C{list} of C{str}
        """
        # Get the file path for this pass from the given pass data
        filePath = passData.filePath
        fileDir = os.path.dirname(filePath)
        # If the enclosing directory doesn't exist, then try to create it
        LookFileUtil.CreateLookFileDirectory(fileDir)

        def do_material_write(stage, rootPrimName):
            # Iterate over materials
            # Maintain a dictionary of materialpaths and their resultant
            # sdf paths.
            materialDict = {}
            passDatamaterialDictKeys = passData.materialDict.keys()
            passDatamaterialDictKeys.sort()
            for location in passDatamaterialDictKeys:
                (locationType, materialAttribute) = \
                    passData.materialDict[location]
                if locationType != "material" or not materialAttribute:
                    continue
                # Discard materials if the path is not a valid SdfPath.
                if not Sdf.Path.IsValidPathString(location):
                    log.warning('"%s" is not a valid SdfPath. Material will '
                                'be skipped.', location)
                    continue
                sdfLocationPath = self.__class__.locationPathtoSdfPath(
                    location, rootPrimName)
                self.__class__.writeMaterialAttribute(
                    stage, materialAttribute, location, sdfLocationPath,
                    materialDict)
            self.__class__.writeOverrides(
                stage, passData.outputDictList, passData.sharedOverridesDict,
                rootPrimName, materialDict)

        # Validate whether we have the settings we need:
        try:
            createVariantSet = self._settings["createVariantSet"]
            fileName = self._settings["fileName"]
            fileFormat = self._settings["fileFormat"]
            rootPrimName = self._settings["rootPrimName"]
            materialVariantSetInitialized = \
                self._settings["materialVariantSetInitialized"]
            variantSetName = self._settings["variantSetName"]
        except ValueError:
            raise LookFileBakeException("Invalid Settings for UsdExport. "
                                        "The UsdExport Output Format plug-in "
                                        "is intended for use with the "
                                        "UsdMaterialBake node.")
        filePath = os.path.join(fileDir, fileName) + "." + fileFormat
        # Validate rootPrimName, must start with / and must not end with /
        if not rootPrimName.startswith("/"):
            rootPrimName = "/" + rootPrimName
        if rootPrimName.endswith("/"):
            rootPrimName = rootPrimName[:-1]
        if createVariantSet:
            # Write all material data to the same variant file
            # (create on first pass, then append in subsequent passes)
            if materialVariantSetInitialized:
                stage = Usd.Stage.Open(filePath)
                rootPrim = stage.GetPrimAtPath(rootPrimName)
                variantSet = rootPrim.GetVariantSets().GetVariantSet(
                    variantSetName)
            else:
                stage = Usd.Stage.CreateNew(filePath)
                now = datetime.datetime.now()
                stage.SetMetadata(\
                    "comment",\
                    "Generated by Katana on {}".format(\
                        now.strftime("%d/%b/%Y %H:%M:%S")))
                rootPrim = stage.DefinePrim(rootPrimName)
                stage.SetDefaultPrim(rootPrim)
                variantSet = rootPrim.GetVariantSets().AddVariantSet(
                    variantSetName)
                self._settings["defaultMaterialVariant"] = passData.passName
                self._settings["materialVariantSetInitialized"] = True

            variantName = passData.passName
            variantSet.AddVariant(variantName)
            variantSet.SetVariantSelection(variantName)

            with variantSet.GetVariantEditContext():
                do_material_write(stage, rootPrimName)

            # Set the default variant to the first variant seen.
            # We cant use the variantSet.GetNames() because that returns us
            # the results in alphabetical order, so we must remember which
            # we added first ourselves.
            if self._settings.defaultMaterialVariant:
                variantSet.SetVariantSelection(
                    self._settings.defaultMaterialVariant)
        else:
            # Create a new USD stage
            stage = Usd.Stage.CreateNew(filePath)
            rootPrim = stage.DefinePrim(rootPrimName)
            stage.SetDefaultPrim(rootPrim)
            do_material_write(stage, rootPrimName)

        # Save the stage
        stage.GetRootLayer().Save()

        return [filePath]

# Only register the output format if the pxr module has been imported
# successfully.
if pxrImported:
    LookFileBakeAPI.RegisterOutputFormat(UsdExport)
