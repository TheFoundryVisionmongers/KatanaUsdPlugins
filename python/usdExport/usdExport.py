# Copyright (c) 2020 The Foundry Visionmongers Ltd.
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
    from pxr import Kind, Sdf, Usd, UsdShade
    # These includes also require fnpxr
    from UsdExport.common import (LocationPathToSdfPath, GetRelativeUsdSdfPath)
    from UsdExport.light import (WriteLight, WriteLightList)
    from UsdExport.lightLinking import (WriteLightLinking)
    from UsdExport.material import (WriteMaterial, WriteMaterialAssign,
                                    WriteChildMaterial)
    from UsdExport.pluginRegistry import (GetUsdExportPluginsByType)
    from UsdExport.prmanStatements import (
        WritePrmanStatements, WritePrmanGeomGprims, WritePrmanModel)
    from UsdExport.transform import (WriteTransform)

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
    LocationTypeWritingOrder = ["material", "light", "all"]

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

    @classmethod
    def __getResolvedMaterialName(cls, attrDict, sdfLocationPath):
        """
        Returns the name which should be used for the resolved material. This can be customized
        by setting the `info.usdExport.resolvedMaterialName` attribute. Otherwise it builds the
        name by combining the sdfLocationPath's name (the name of that prim), the originally
        assigned material name, and then "_resolved".
        """
        if not isinstance(attrDict, dict) or not sdfLocationPath or not sdfLocationPath.name:
            return None

        infoAttr = attrDict.get("info")
        resolvedMaterialNameAttr = attrDict.get("info").getChildByName(
            "usdExport.resolvedMaterialName"
        )
        resolvedMaterialName = None
        if resolvedMaterialNameAttr:
            resolvedMaterialName = resolvedMaterialNameAttr.getValue()
        if not resolvedMaterialName or not isinstance(resolvedMaterialName, str):
            materialAssignAttr = infoAttr.getChildByName("materialAssign") if infoAttr else None
            assignedMaterialName = (
                materialAssignAttr.getValue().split("/")[-1] if materialAssignAttr else ""
            )
            resolvedMaterialName = f"{sdfLocationPath.name}_{assignedMaterialName}_resolved"
        return resolvedMaterialName

    @classmethod
    def WriteOverride(cls, stage, overrideDict, sharedOverridesDict,
                      locationTypePass, location, rootName, rootPrimName,
                      materialDict, lightDict):
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
        @type lightDict: C{dict} of C{str} : C{Sdf.Path}
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
        @param lightDict: A dictionary containing a mapping between the
            light location paths from Katana, and the SdfPath in the USD
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

        sdfLocationPath = LocationPathToSdfPath(location, rootPrimName)
        # By default create an override prim, any other node creation should
        # happen before the point of creating this prim.
        prim = None

        xformAttribute = attrDict.get("xform", None)
        if xformAttribute:
            prim = WriteTransform(stage,
                                  sdfLocationPath,
                                  xformAttribute)

        lightListAttribute = attrDict.get("lightList", None)
        if lightListAttribute:
            WriteLightLinking(stage, sdfLocationPath, lightListAttribute,
                lightDict, rootName)

        materialAttribute = attrDict.get("material")
        # Write materialLocations first, as sdfLocationPath may be altered when
        # writing to a child location.
        if locationType == "material":
            # Important: Here sdfLocationPath is overridden to match
            # the Sdf.Path used for the  the child material. This is such
            # that sdfLocationPath can be re-used regardless as to
            # whether this location is a child material and has had its
            # hierarchy altered, or not.
            sdfLocationPath = cls.writeMaterialAttribute(
                stage, materialAttribute, location, sdfLocationPath,
                materialDict)
            prim = stage.GetPrimAtPath(sdfLocationPath)

        elif locationType == "light":
            prim = WriteLight(stage, sdfLocationPath,
                                   materialAttribute)
            lightDict[location] = sdfLocationPath

        elif materialAttribute:
            # If we detect that there are material attributes on a non-material type location, we
            # create a child material from that resolved material and its shaders.
            resolvedMaterialName = cls.__getResolvedMaterialName(attrDict, sdfLocationPath)
            if resolvedMaterialName:
                sdfLocationPath = sdfLocationPath.AppendChild(resolvedMaterialName)
                sdfLocationPath = cls.writeMaterialAttribute(
                    stage, materialAttribute, location, sdfLocationPath, materialDict,
                )
                prim = stage.GetPrimAtPath(sdfLocationPath)
                if prim:
                    material = UsdShade.Material(prim)
                    overridePrim = prim.GetParent()
                    if material:
                        WriteMaterialAssign(material, overridePrim)

        prmanStatementsAttributes = {
            key: value for key, value in attrDict.items()
            if 'prmanStatements' in key}
        if prmanStatementsAttributes:
            if not prim:
                prim = stage.OverridePrim(sdfLocationPath)
            WritePrmanStatements(prmanStatementsAttributes, prim)
            WritePrmanGeomGprims(prmanStatementsAttributes, prim)
            WritePrmanModel(prmanStatementsAttributes, prim)

        # layout
        layout = attrDict.get("layout")
        if layout is not None:
            # Layout information could be written here.
            pass

        # materialAssign
        materialAssignAttribute = attrDict.get("materialAssign")
        if materialAssignAttribute is not None:
            if not prim:
                prim = stage.OverridePrim(sdfLocationPath)
            cls.WriteMaterialAssignAttr(materialDict, stage, rootName,
                                        materialAssignAttribute, prim)

        for plugin in GetUsdExportPluginsByType(locationType):
            plugin.WritePrim(stage, sdfLocationPath, attrDict)

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
        # Create a light dict for us to write mappings of relative paths
        # to sdfLocationPaths, such that we can use this when finding the
        # lights used in resolved light linking
        lightDict = {}
        for (overrideDict, rootName, rootType) in outputDictList:
            _ = rootType
            outputDictKeys = sorted(overrideDict.keys())
            for locationTypePass in cls.LocationTypeWritingOrder:
                for location in outputDictKeys:
                    cls.WriteOverride(stage, overrideDict,
                                      sharedOverridesDict,
                                      locationTypePass,
                                      location,
                                      rootName,
                                      rootPrimName,
                                      materialDict,
                                      lightDict)
        WriteLightList(stage)

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
        materialPath = GetRelativeUsdSdfPath(
            materialDict, rootName, assignValue)
        if materialPath:
            material = UsdShade.Material.Get(stage, materialPath)
            if material:
                WriteMaterialAssign(material, overridePrim)


    # Instance Methods --------------------------------------------------------

    def __init__(self, settings):
        super(UsdExport, self).__init__(settings)
        self._settings.materialVariantSetInitialized = False
        self._settings.assemblyWritten = False
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
            passDatamaterialDictKeys = list(passData.materialDict.keys())
            passDatamaterialDictKeys.sort()
            for location in passDatamaterialDictKeys:
                (locationType, materialAttribute) = \
                    passData.materialDict[location]
                if locationType != "material":
                    continue
                # Discard if the path is not a valid SdfPath.
                if not Sdf.Path.IsValidPathString(location):
                    log.warning('"%s" is not a valid SdfPath. Location will '
                                'be skipped.', location)
                    continue
                sdfLocationPath = LocationPathToSdfPath(location, rootPrimName)
                if locationType == "material":
                    self.__class__.writeMaterialAttribute(
                        stage, materialAttribute, location, sdfLocationPath,
                        materialDict)

            self.__class__.writeOverrides(stage,
                                          passData.outputDictList,
                                          passData.sharedOverridesDict,
                                          rootPrimName,
                                          materialDict)

        # Validate whether the required settings are provided
        try:
            createVariantSet = self._settings["createVariantSet"]
            looksFilename = self._settings["looksFilename"]
            looksFileFormat = self._settings["looksFileFormat"]
            createCompleteUsdAssemblyFile = self._settings[
                "createCompleteUsdAssemblyFile"]
            assemblyFilename = self._settings["assemblyFilename"]
            payloadFilename = self._settings["payloadFilename"]
            rootPrimName = self._settings["rootPrimName"]
            materialVariantSetInitialized = \
                self._settings["materialVariantSetInitialized"]
            assemblyWritten = \
                self._settings["assemblyWritten"]
            variantSetName = self._settings["variantSetName"]
        except ValueError:
            raise LookFileBakeException("Invalid Settings for UsdExport. "
                                        "The UsdExport Output Format plug-in "
                                        "is intended for use with the "
                                        "UsdMaterialBake node.")
        looksFilename += "." + looksFileFormat
        looksFilePath = os.path.join(fileDir, looksFilename)

        assemblyWritten = self._settings["assemblyWritten"]
        assemblyStage = None
        assemblyRootPrim = None
        if createCompleteUsdAssemblyFile and not assemblyWritten:
            # setup the assembly stage
            assemblyPath = os.path.join(fileDir, assemblyFilename) + ".usda"
            assemblyStage = _CreateNewStage(
                assemblyPath, rootPrimName, Kind.Tokens.assembly)
            assemblyRootPrim = assemblyStage.GetDefaultPrim()

            # add the payload asset
            if not payloadFilename:
                raise LookFileBakeException(
                    "No payload asset filename was specified!")
            if not os.path.exists(payloadFilename):
                raise LookFileBakeException(
                    "Payload asset at '{}' does not exist!".format(
                        payloadFilename
                    ))
            payload = Sdf.Payload(payloadFilename)
            assemblyRootPrim.GetPayloads().AddPayload(
                payload, position=Usd.ListPositionBackOfAppendList)
            assemblyWritten = True

        # Validate rootPrimName, must start with / and must not end with /
        if not rootPrimName.startswith("/"):
            rootPrimName = "/" + rootPrimName
        if rootPrimName.endswith("/"):
            rootPrimName = rootPrimName[:-1]
        variantName = None
        if createVariantSet:
            # Write all material data to the same variant file
            # (create on first pass, then append in subsequent passes)
            if materialVariantSetInitialized:
                stage = Usd.Stage.Open(looksFilePath)
                rootPrim = stage.GetPrimAtPath(rootPrimName)
                variantSet = rootPrim.GetVariantSets().GetVariantSet(
                    variantSetName)
            else:
                stage = _CreateNewStage(
                    looksFilePath, rootPrimName, Kind.Tokens.component)
                defaultPrim = stage.GetDefaultPrim()
                variantSet = defaultPrim.GetVariantSets().AddVariantSet(
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
            stage = _CreateNewStage(
                looksFilePath, rootPrimName, Kind.Tokens.component)
            rootPrim = stage.DefinePrim(rootPrimName)
            writePassData(stage, rootPrimName)

        # Save the stage
        stage.GetRootLayer().Save()

        if createCompleteUsdAssemblyFile and assemblyWritten:
            # add the lookfile as a reference
            assemblyRootPrim.GetReferences().AddReference(
                './'+ looksFilename, position=Usd.ListPositionBackOfAppendList)
            assemblyStage.GetRootLayer().Save()

        return [filePath]

def _CreateNewStage(filePath, rootPrimName, kind=None):
    stage = None
    existingLayer = Sdf.Layer.FindOrOpen(filePath)
    if existingLayer:
        existingLayer.Clear()
        stage = Usd.Stage.Open(existingLayer)
    else:
        stage = Usd.Stage.CreateNew(filePath)

    stage.RemovePrim(rootPrimName)
    now = datetime.datetime.now()
    stage.SetMetadata(\
        "comment",\
        "Generated by Katana on {}".format(\
            now.strftime("%d/%b/%Y %H:%M:%S")))
    rootPrim = stage.DefinePrim(rootPrimName)
    if kind:
        Usd.ModelAPI(rootPrim).SetKind(kind)
    stage.SetDefaultPrim(rootPrim)
    return stage

# Only register the output format if the pxr module has been imported
# successfully.
if pxrImported:
    LookFileBakeAPI.RegisterOutputFormat(UsdExport)
