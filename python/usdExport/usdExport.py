# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.
"""
LookFileBakeAPI OutputFormat plugin class override and registry.
"""

import datetime
import logging
import os

from Katana import LookFileBakeAPI
from LookFileBakeAPI import LookFileUtil, LookFileBakeException
log = logging.getLogger("UsdExport")

# [USD install]/lib/python needs to be on $PYTHONPATH for this import to work
pxrImported = False
try:
    from fnpxr import Usd, UsdShade, Sdf
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
        """
        A Helper method used to check whether the location should be written to
        the stage yet. The order in which to write the location types is stored
        in the C{LocationTypeWritingOrder} class variable. This is necessary
        because the USD Bindings must bind to a valid existing material.

        @type locationType: C{str}
        @type locationTypePass: C{str}
        @rtype: C{bool}
        @param locationType: The type attribute value for the current location.
        @param locationTypePass: The value for the current index of the
            C{LocationTypeWritingOrder}.
        @return: Boolean value whether to continue of skip this location
            based on its type.
        """
        if locationTypePass is None:
            return True
        if locationType != locationTypePass:
            if locationType in cls.LocationTypeWritingOrder:
                return False
            if locationTypePass == "all":
                return True
            return False
        return True

    @staticmethod
    def locationPathtoSdfPath(locationPath, rootPrimName):
        """
        A Helper method to simply validate and create SdfPaths from
        location paths, concatenating the rootPrim name onto the Katana
        location path.

        @type locationPath: C{str}
        @type rootPrimName: C{str}
        @rtype: C{Sdf.Path}
        @param locationPath: The path for the current Katana location.
        @param rootPrimName: The name of the root prim for the C{Usd.Stage}.
        @return: A validated concatenated C{Sdf.Path} to write the C{Prim} to.
        """
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
        """
        This method is responsible for writing all of the attributes from the
        C{passData.shaderdOverrides} dictionary to the USD Stage.
        See the C{LookFilePassData} class for more information about the
        C{overrideDict} and C{sharedOverridesDict}.

        @type stage: C{Usd.Stage}
        @type overrideDict: C{dict} of C{str} : C{int}
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
        @param locationTypePass: The current location types to allow writing.
            See L{WriteOverrides} for more info.
        @param location: The Katana location path for the current iteration
            relative to the C{rootName}. Used when creating the prim in the
            UsdStage, after prepending the C{rootPrimName}. When writing child
            materials, this is edited to concatenate parent materials with
            C{"_"} delimiters.
        @param rootName: The root name from the C{overrideDictList}, see
            C{LookFilePassData} for more details.
        @param rootPrimName: The name of the root C{Usd.Prim} for the given
            stage.
        @param materialDict: A dictionary containing a mapping between the
            material location paths from Katana, and the SdfPath in the USD
            Stage.
        """
        # pylint: disable=too-many-branches,
        # pylint: disable=too-many-locals
        # pylint: disable=too-many-arguments
        if not location:
            return
        sharedOverridesKey = overrideDict[location]
        attrDict = sharedOverridesDict.get(sharedOverridesKey)
        if not attrDict:
            return
        # Check to see if if this type of location should be written yet.
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
        # Write materialLocations first, as sdfLocationPath may be altered when
        # writing to a child location.
        if materialAttribute is not None and locationType == "material":
            # Important: Here sdfLocationPath is overridden to match
            # the Sdf.Path used for the  the child material. This is such
            # that sdfLocationPath can be re-used regardless as to
            # whether this location is a child material and has had its
            # hierarchy altered, or not.
            sdfLocationPath = cls.writeMaterialAttribute(
                stage, materialAttribute, location, sdfLocationPath,
                materialDict)
            createOverridePrim = False

        # Create an overridePrim if not disabled, and then get the current prim
        # to add any other extra data.
        if createOverridePrim:
            stage.OverridePrim(sdfLocationPath)
        prim = stage.GetPrimAtPath(sdfLocationPath)

        # layout
        layout = attrDict.get("layout")
        if layout is not None:
            # Layout information could be written here.
            pass

        # materialAssign
        materialAssignAttribute = attrDict.get("materialAssign")
        if materialAssignAttribute is not None:
            cls.WriteMaterialAssignAttr(materialDict, stage, rootName,
                                        materialAssignAttribute, prim)

    @staticmethod
    def writeMaterialAttribute(stage, materialAttribute, location,
                               sdfLocationPath, materialDict):
        """
        Writes a Material prim to the USD Stage, and returns the C{Sdf.Path}
        for where it wrote to. Handles child materials by writing them as
        siblings and using the C{BaseMaterial} mechanisms to ensure the
        `specializes` composition arc is utilized in USD.

        @type stage: C{Usd.Stage}
        @type materialAttribute: C{FnAttribute.GroupAttribute}
        @type location: C{str}
        @type sdfLocationPath: C{Sdf.Path}
        @type materialDict: C{dict} of C{ C{str} : C{Sdf.Path} }
        @rtype: C{Sdf.Path}
        @param stage: The C{Usd.Stage} to write data to.
        @param materialAttribute: The ``material`` attribute from the
            C{location}.
        @param location: The Katana location path for the current iteration
            relative to the C{rootName}.
        @param sdfLocationPath: The location this C{USD.Material} should be
            written to. This is changed if writing to a child material.
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
        C{WriteOverride} for each path in the C{overrideDict}.
        OverrideDict keys are sorted in order to ensure locations are added
        in order of their depth, otherwise the child location could be added
        before the parent, which would not provide all the information to the
        USD Stage writing.
        The locations are written in an order based on the class variable list
        C{LocationTypeWritingOrder} to ensure that bindings and references
        will work, because, for example, the Material does not exist before
        the GeoPrim tries to bind to it.

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
            C{Usd.Stage}.
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
    def WriteMaterialAssignAttr(cls, materialDict, stage, rootName, attribute,
                                overridePrim):
        """
        Retrieves the materialAssign value from the C{attribute}, and calls
        L{GetRelativeUsdSdfPath} to resolve the relative C{Sdf.Path} before
        calling C{UsdExport.WriteMaterialAssign} to write the material
        assignment to the USD Stage.

        @type materialDict: C{dict} of C{ C{str} : C{Sdf.Path} }
        @type stage: C{Usd.Stage}
        @type rootName: C{str}
        @type attribute: C{FnAttribute.StringAttribute}
        @type overridePrim: C{Usd.OverridePrim}
        @param materialDict: The dictionary of Katana relative paths to the
            location of L{rootName}, and their C{Usd.Sdf} paths.
        @param stage: The stage to write the assignment to, and to read the
            material to bind from.
        @param rootName: The name of the root location from Katana, see the
            C{LookFileBakePass} for more info.
        @param attribute: The materialAssign attribute.
        @param overridePrim: The Prim to write the material binding to.
        """
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
        Katana provides the absolute scene graph location path, but since
        the LookFileBakeAPI provides materials under the C{rootName}, as
        relative paths, the abolute paths must be translated to match
        the relative paths provided. If writing out materials from the
        root/materials, or other root paths, this is not needed since these
        are absolute already.
        Child material paths also need to be remapped since they are changed
        to sibling materials. The mappings from the original location in Katana
        and the C{Sdf.Path} in the USD file are stored in the C{materialDict}.

        @type materialDict: C{dict} of C{ C{str} : C{Sdf.Path} }
        @type rootName: C{str}
        @type locationPath: C{str}
        @rtype: C{str} C{None}
        @param materialDict: The dictionary of katana relative paths to the
            location of L{rootName}, and their C{Sdf.Path}s.
        @param rootName: Refer to L{LookFilePassData} for more information
        @param locationPath: Katana location path.
        @return: The path to the material in the USD Stage, read from the
            C{materialDict} after resolving the relative path.
        """
        if location in materialDict:
            return materialDict[location]
        rfindIndex = len(location)
        # Path separators to ensure the rootName is not matched within a
        # directory or file name. This will require stripping later on.
        rootNamePath = "/" + rootName + "/"
        rfindIndex = location.rfind(rootNamePath, 0, rfindIndex)
        if rfindIndex < 0:
            return None
        # Start reading with a closing "/"
        materialPath = location[rfindIndex + len(rootNamePath) - 1:]
        if materialPath in materialDict:
            return materialDict[materialPath]
        # If the location is not found in the first try, there may be
        # duplicate levels in  the hierarchy, so this must be repeated until
        # a valid relative material path is found.
        materialPath = location[rfindIndex:]
        # Start reading with a closing "/"
        rfindIndex = rfindIndex + 1
        while materialPath and materialPath not in materialDict:
            rfindIndex = location.rfind(rootNamePath, 0, rfindIndex)
            if rfindIndex < 0:
                # Can no longer find the rootNamePath.
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
        Method overridden from the C{LookFileBakeAPI.OutputFormat}. The root
        for our UsdExporting.

        @type passData: C{LookFileBakeAPI.LookFilePassData}
        @rtype: C{list} of C{str}
        @param passData: The data representing a single variant to be baked to
            the C{Usd.Stage}.
        @return: A list of paths to files which have been written.
        """
        # Get the file path for this pass from the given pass data
        filePath = passData.filePath
        fileDir = os.path.dirname(filePath)
        # If the enclosing directory doesn't exist, then try to create it
        LookFileUtil.CreateLookFileDirectory(fileDir)

        def writePassData(stage, rootPrimName):
            """
            Performs the writing action.

            @type stage: C{Usd.Stage}
            @type rootPrimName: C{str}
            @param stage: The Usd Stage to write to.
            @param rootPrimName: The name of the root Prim to write under.
            """
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

        # Validate whether the required settings are provided
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
                writePassData(stage, rootPrimName)

            # Set the default variant to the first variant seen.
            # variantSet.GetNames() returns the results in alphabetical order.
            # The variant must be set based on the cached value.
            if self._settings.defaultMaterialVariant:
                variantSet.SetVariantSelection(
                    self._settings.defaultMaterialVariant)
        else:
            # Create a new USD stage
            stage = Usd.Stage.CreateNew(filePath)
            rootPrim = stage.DefinePrim(rootPrimName)
            stage.SetDefaultPrim(rootPrim)
            writePassData(stage, rootPrimName)

        # Save the stage
        stage.GetRootLayer().Save()

        return [filePath]

# Only register the output format if the pxr module has been imported
# successfully.
if pxrImported:
    LookFileBakeAPI.RegisterOutputFormat(UsdExport)
