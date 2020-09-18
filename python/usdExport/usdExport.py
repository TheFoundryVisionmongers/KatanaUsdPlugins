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
    def writeOverride(cls, stage, outputDict, sharedOverrides,
                      locationTypePass, locationPath, rootName, rootPrimName,
                      pathsToRemove, materialDict, outputDictKey=None,
                      parentMaterialSdfList=None):
        """
        This method is responsible for writing all of the overrides
        from the passData.shaderdOverrides dictionary.
        We use the outputDict which has key value pairs of the location
        from katana, to the key of the sharedOverride from the
        sharedOverrides dictionary.
        This method is also called recursively, and in the recursive calls
        we also pass in outputDictKey and parentMaterialSdfList.
        @param stage: The USD Stage to write the overrides onto.
        @param outputDict: A dictionary from the passData, which has a
            dictionary keyed by the location, with the value being a key
            into the sharedOverrides dictionary.
        @param sharedOverrides: A dictionary from the passData which
            contains attribute data which is shared across many locations
            as a de-duplication step.  The key is an integer which is
            stored as the value on each location keyed value of the
            outputDict.
        @param locationTypePass:
        @param locationPath:
        @param rootName:
        @param rootPrimName:
        @param pathsToRemove:
        @param materialDict: A dictionary containing a mapping of the
            Katana location paths to the Sdf.Path's of the material prim
            created for that path.  This ensures child materials
            are bindable, and we pass this to our materialAssignment logic.
        @param outputDictKey:
        @param parentMaterialSdfList:
        @type stage: C{Usd.Stage}
        @type outputDict: C{dict} of C{str} : C{int}
        @type sharedOverrides: C{dict} of C{int} : (
            C{dict} of (
                    C{str} : C{FnAttribute.Attribute}
                )
            )
        @type locationTypePass: C{str}
        @type locationPath: C{str}
        @type rootName: C{str}
        @type rootPrimName: C{str}
        @type pathsToRemove: C{list} of C{str}
        @type materialDict: C{dict} of C{str} : C{Sdf.Path}
        @type outputDictKey: C{str}
        @type parentMaterialSdfList: C{list} of C{Sdf.Path}
        """
        # We take the full path as the override path.
        # since the root will be handled by UsdIn
        if not locationPath:
            return pathsToRemove
        if outputDictKey:
            sharedOverridesKey = outputDict[outputDictKey]
        else:
            sharedOverridesKey = outputDict[locationPath]
            outputDictKey = locationPath
        # Copy this such that if we change locationPath later,
        # it wont affect our deletion.
        originalLocationPath = locationPath
        # Check to see if we are meant to add this type of location
        # yet.
        attrDict = sharedOverrides.get(sharedOverridesKey)
        if not attrDict:
            return pathsToRemove
        typeAttr = attrDict.get("type")
        locationType = ""
        if typeAttr:
            locationType = typeAttr.getValue()
        if not cls.__checkTypeWritingOrder(locationType, locationTypePass):
            return pathsToRemove

        sdfLocationPath = cls.locationPathtoSdfPath(locationPath, rootPrimName)
        overridePrim = stage.OverridePrim(sdfLocationPath)

        # Now loop through the data, if it doesn't exist in
        # the UsdStage create the location and data, and then
        # reference.  If it already exists, just reference! key is
        # an integer, the index between the sharedOverrides and
        # rootOverrides locations they relate to.
        for attrName in attrDict.keys():
            if attrName == "layout":
                # Here is where we would look to support layout info.
                continue
            attribute = attrDict[attrName]
            if not attribute:
                continue
            if attrName == "material":
                if locationType == "material":
                    if parentMaterialSdfList:
                        material = WriteMaterial(
                            stage, sdfLocationPath, attribute)
                        WriteChildMaterial(
                            stage, sdfLocationPath, attribute,
                            parentMaterialSdfList)
                    else:
                        WriteMaterial(stage, sdfLocationPath, attribute)
                    materialDict[outputDictKey] = sdfLocationPath
            elif attrName == "lookfileChildren":
                # The children returned are for any location not just
                # not just material children.  Here we only care about
                # material children.
                if not "material" in attrDict:
                    continue
                childNames = attribute.getNearestSample(0)
                # Loop through all the children of this material.
                for childName in childNames:
                    # Build the new locationPath for the child material
                    childLocationPath = originalLocationPath + "_" +\
                        childName
                    # Build the child material's original location as
                    # seen in Katana.
                    childOriginalPath = outputDictKey + "/" + childName
                    # If we have already retrieved the
                    parentSdf = sdfLocationPath
                    materialPrim = stage.GetPrimAtPath(parentSdf)
                    parentNames = []
                    while not materialPrim:
                        parentNames.append(parentSdf.name)
                        parentSdf = parentSdf.GetParentPath()
                        materialPrim = stage.GetPrimAtPath(parentSdf)

                    # build its closest original parent material
                    if parentNames:
                        parentNames.append(parentSdf.name)
                        parentAppendName = "_".join(reversed(parentNames))
                        parentSdf = parentSdf.ReplaceName(parentAppendName)
                    if parentMaterialSdfList:
                        # Copy the list first, because otherwise the
                        # changes will be retained in this for loop for the
                        # next child, and not just passed down the depth of
                        # the tree.
                        newParentSdfList = parentMaterialSdfList[:]
                        newParentSdfList.append(parentSdf)
                    else:
                        newParentSdfList = [parentSdf]
                    childpathsToRemove = cls.writeOverride(stage,
                        outputDict, sharedOverrides, None,
                        childLocationPath, rootName, rootPrimName,
                        [], materialDict, outputDictKey=childOriginalPath,
                        parentMaterialSdfList=newParentSdfList)
                    pathsToRemove.extend(childpathsToRemove)
                    pathsToRemove.append(childOriginalPath)
            elif attrName == "materialAssign":
                cls.writeMaterialAssign(materialDict, stage, rootName,
                    attribute, overridePrim)
        if not outputDictKey:
            pathsToRemove.append(originalLocationPath)
        return pathsToRemove

    @classmethod
    def writeOverrides(cls, stage, outputDictList, sharedOverrides,
                       rootPrimName):
        # We want to write the sharedoverride to root locations, but have these not loaded,
        # they should only be loaded as references.
        # We need to write locations in an order to ensure that bindings
        # and references will work, otherwise we end up with missing bindings
        # because, for example, the Material does not exist before the
        # GeoPrim tries to bind to it.
        for (outputDict, rootName, rootType) in outputDictList:
            _ = rootType
            pathsToRemove = []
            outputDictKeys = sorted(outputDict.keys())
            materialDict = {}
            for locationTypePass in cls.LocationTypeWritingOrder:
                for path in pathsToRemove:
                    if path in outputDictKeys:
                        outputDictKeys.remove(path)
                pathsToRemove = []
                for locationPath in outputDictKeys:
                    if locationPath in pathsToRemove:
                        continue
                    pathsToRemove = cls.writeOverride(stage, outputDict,
                      sharedOverrides, locationTypePass, locationPath, rootName,
                      rootPrimName, pathsToRemove, materialDict)

    @classmethod
    def writeMaterialAssign(cls, materialDict, stage, rootName, attribute,
                            overridePrim):

        assignValue = str(attribute.getValue())
        materialPath = cls.GetRelativeUsdSdfPathFromAttribute(
            materialDict, rootName, assignValue)

        if materialPath:
            material = UsdShade.Material.Get(stage, materialPath)
            if material:
                WriteMaterialAssign(material, overridePrim)

    @classmethod
    def GetRelativeUsdSdfPathFromAttribute(cls, materialDict, rootName,
                                           assignValue):
        """ To be used with values from attributes like "materialAssign".
            Katana will provide the full scenegraph location path, but since
            the lookfilebake writes out from a given root, we need to remove
            the root prefix. We also need to remap child material paths,
            so we pass in the materialDict which
        """
        materialKeys = materialDict.keys()
        rfindIndex = len(assignValue)
        # We need to add the path separators to ensure we are not just
        # finding the rootName word.
        # This does mean we need to take these extra characters into
        # account when using rfind, since we will strip the leading,
        # or closing "/" which we will need.
        rootNamePath = "/" + rootName + "/"
        rfindIndex = assignValue.rfind(rootNamePath, 0, rfindIndex)
        if rfindIndex < 0:
            return None
        #Ensure we start reading with a closing "/"
        materialPath = assignValue[rfindIndex + len(rootNamePath) - 1:]
        if materialPath in materialKeys:
            return materialDict[materialPath]
        # If we don't find the path first try, it means we may be dealing
        # with a materialAssignment path with multiple duplicate levels in
        # the hierarchy, so we must check until we find our valid relative
        # material path.
        materialPath = assignValue[rfindIndex:]
        #Ensure we start reading with a closing "/"
        rfindIndex = rfindIndex + 1
        while materialPath and materialPath not in materialKeys:
            rfindIndex = assignValue.rfind(rootNamePath, 0, rfindIndex)
            # If we can no longer find the rootNamePath.
            if rfindIndex < 0:
                return None
            materialPath = assignValue[rfindIndex:]
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
            for materialLocationPath, (locationType, materialAttribute) \
                    in passData.materialDict.iteritems():
                _ = locationType
                # Discard materials if the path is not a valid SdfPath.
                if not Sdf.Path.IsValidPathString(materialLocationPath):
                    log.warning('"%s" is not a valid SdfPath. Material will '
                                'be skipped.', materialLocationPath)
                    continue
                materialSdfPath = self.__class__.locationPathtoSdfPath(
                    materialLocationPath, rootPrimName)
                if materialAttribute:
                    WriteMaterial(
                        stage, materialSdfPath, materialAttribute)
            self.__class__.writeOverrides(
                stage, passData.outputDictList, passData.sharedOverridesDict,
                rootPrimName)

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
