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

import logging
import time

from Katana import NodegraphAPI, Utils
import LookFileBakeAPI
from LookFileBakeAPI import LookFileBaker
from Nodes3DAPI import LookFileBaking

log = logging.getLogger("UsdMaterialBake.Node")

__all__ = ['UsdMaterialBakeNode']


class UsdMaterialBakeNode(NodegraphAPI.SuperTool):
    """
    UsdMaterialBake node, with extra methods to allow for baking features
    and ability to change and re-order inputs driven by the Editor UI, or
    by calling these directly. Used for baking USD data using the UsdExport
    OutputFormat plug-in.
    """
    MIN_PORTS = 1

    def __init__(self):
        try:
            Utils.UndoStack.DisableCapture()
            try:
                self.setName("UsdMaterialBake")
                self.setType("UsdMaterialBake")

                networkVersion = "1.0"
                parameters_XML = _parameters_XML.format(
                    networkVersion=networkVersion)
                self.getParameters().parseXML(parameters_XML)

                self.addInputPort("orig")
                self.addInputPort("default")
                self.addOutputPort("out")

                dot = NodegraphAPI.CreateNode('Dot', self)
                dot.getInputPortByIndex(0).connect(self.getSendPort("orig"))
                dot.getOutputPortByIndex(0).connect(self.getReturnPort("out"))
                NodegraphAPI.SetNodePosition(dot, (0, 200))
                NodegraphAPI.SetNodeShapeAttr(self, 'basicDisplay', 1)
                NodegraphAPI.SetNodeShapeAttr(self, 'iconName', '')
            finally:
                Utils.UndoStack.EnableCapture()

        except Exception:
            log.exception("CREATE UsdMaterialBake FAILED.")
            raise
        self.__timer = None
        self.__interruptWidget = None

    # --- Public node API -------------------------------------------
    def addVariantInput(self, variantName):
        """
        Adds a new input port for a new variant name.

        @type variantName: C{str}
        @param variantName: New name to give to the new port.
        """
        self.addInputPort(variantName)

    def deleteVariantInput(self, index):
        """
        Deletes the existing variant from the index provided.
        @type index: C{int}
        @param index: Index of the input port to delete.
        """
        if index < self.MIN_PORTS or index >= self.getNumInputPorts():
            return

        portName = self.getInputPortByIndex(index).getName()
        self.removeInputPort(portName)

    def reorderInput(self, index, newIndex):
        """
        Reorders two input variants by Deleting the old port and recreating
        it in the new index.

        @type index: C{int}
        @type newIndex: C{int}
        @param index: Index of the input port to reposition.
        @param newIndex: New position of the input port. Assuming that the
            current index does not exist.
        """
        if index < self.MIN_PORTS or index >= self.getNumInputPorts():
            return
        if newIndex < self.MIN_PORTS or newIndex >= self.getNumInputPorts():
            return

        oldPort = self.getInputPortByIndex(index)
        connections = oldPort.getConnectedPorts()
        self.removeInputPort(oldPort.getName())
        oldPortName = oldPort.getName()
        del oldPort
        newPort = self.addInputPortAtIndex(oldPortName, newIndex)
        for i in connections:
            newPort.connect(i)

    def renameVariantInput(self, index, newName):
        """
        Renames an existing input. Does not change the input order.

        @type index: C{int}
        @type newName: C{str}
        @param index: Index of the input to rename.
        @param newName: New name to change the port to.
        """
        if index < self.MIN_PORTS or index >= self.getNumInputPorts():
            return

        oldPort = self.getInputPortByIndex(index)
        if newName == oldPort.getName():
            return

        connections = oldPort.getConnectedPorts()
        self.removeInputPort(oldPort.getName())
        del oldPort
        newPort = self.addInputPortAtIndex(newName, index)
        for i in connections:
            newPort.connect(i)

    def addParameterHints(self, attrName, inputDict):
        """
        Adds parameter hints to the given dictionary of hints for a
        GenericAssign parameter that shows the value of an attribute from the
        incoming scene.

        @type attrName: C{str}
        @type inputDict: C{dict}
        @param attrName: The name of the scene graph attribute from the
            incoming scene for which to add parameter hints.
        @param inputDict: The dictionary to which to add parameter hints.
        """
        inputDict.update(_ExtraHints.get(attrName, {}))

    def require3DInput(self, portName, graphState):
        """
        A Method from C{Node3D} used inside the look file bake code to gather
        the input port given the port name and graph state.

        @type portName: C{str}
        @type graphState: C{NodegraphAPI.GraphState}
        @param portName: Port name to read the 3dInput from.
        @param graphState: Graph state used to get the input source.
        @raise TypeError: If C{graphState} provided is not valid.
        @raise RuntimeError: If the port related to the C{portName} does
            not point to a valid node.
        @return: Tuple of the source Node, port and graphState.
        """
        # Required for the current iteration of LookFileBakeAPI, it expects
        # the node it is owrking on to be able to provide the 3dInput
        from Nodes3DAPI import Node3D
        if not isinstance(graphState, NodegraphAPI.GraphState):
            raise TypeError('Node3D.require3DInput(): Given graphState object '
                            'is not a NodegraphAPI.GraphState instance: %s'
                            % repr(graphState))

        sourcePort, sourceGraphState = self.getInputSource(portName,
                                                           graphState)

        sourceNode = None
        if sourcePort is not None:
            sourceNode = sourcePort.getNode()
        if not isinstance(sourceNode, Node3D):
            sourceNode = None

        if sourceNode is None:
            raise RuntimeError('The required input "%s" was not connected on '
                               'the node "%s".' % (portName, self.getName()))
        return (sourceNode, sourcePort, sourceGraphState)

    def bake(self, parentWidget=None):
        """
        Performs the bake based on the settings of this current node
        parameter settings. If a parentWidget is provided, we create a progress
        widget and setup callbacks to update it. If no parentWidget is provided
        we can run this without calling any UI code.

        @type parentWidget: C{QtWidgets.QWidget}
        @param parentWidget: Parent for the progress widget. If set to None,
            the progress callback is not produced.
        """
        graphState = NodegraphAPI.GetCurrentGraphState()
        frameTime = graphState.getTime()
        node = self
        inputPorts = node.getInputPorts()
        numPorts = len(inputPorts)
        if numPorts < 2:
            log.error("Requires at least two input ports to bake a USD Look")
        variantSetName = node.getParameter(
            "variantSetName").getValue(frameTime)
        rootPrimName = node.getParameter("rootPrimName").getValue(frameTime)
        alwaysCreateVariantSet = bool(node.getParameter(
            "alwaysCreateVariantSet").getValue(frameTime) == "Yes")
        looksFilename = node.getParameter("looksFilename").getValue(frameTime)
        looksFileFormat = node.getParameter("looksFileFormat").getValue(
            frameTime)
        createCompleteUsdAssemblyFile = bool(node.getParameter(
            "createCompleteUsdAssemblyFile").getValue(frameTime))
        assemblyFilename = node.getParameter("assemblyFilename").getValue(
            frameTime)
        payloadFilename = node.getParameter("payloadFilename").getValue(
            frameTime)
        createVariantSet = alwaysCreateVariantSet or (len(inputPorts) > 2)
        additionalSettings = {
            "variantSetName": variantSetName,
            "rootPrimName": rootPrimName,
            "createVariantSet": createVariantSet,
            "looksFileFormat": looksFileFormat,
            "looksFilename": looksFilename,
            "createCompleteUsdAssemblyFile": createCompleteUsdAssemblyFile,
            "assemblyFilename": assemblyFilename,
            "payloadFilename": payloadFilename,
            "bakeNodeName": self.getName(),
        }
        # Ensure the interruptWidget is only created in a UI session
        if parentWidget:
            import UI4
            self.__timer = time.time()
            self.__interruptWidget = UI4.Widgets.ModalProcessInterruptWidget(
                self.__interruptCallback, minWidth=512)
        else:
            self.__interruptWidget = None
            self.__timer = None

        assetId = node.getParameter("saveTo").getValue(frameTime)
        rootLocationsParam = node.getParameter("rootLocations")
        rootLocations = [x.getValue(frameTime) for x in
                         rootLocationsParam.getChildren()]

        # Retrieve the Ops for each of the inputs
        referenceOp, passNamesAndOps = self.__getBakerOps(
            self._getPassInputPortNames(), graphState)

        sourceFile = NodegraphAPI.GetOriginalSourceFile()
        if not sourceFile:
            # Use legacy API call in case this file was created in a very old
            # version of Katana (1.6.11 or earlier)
            sourceFile = NodegraphAPI.GetSourceFile()
        sourceAsset = NodegraphAPI.GetKatanaSceneName()

        # When updating to the later version of the LookFileBakeAPI, dont
        # forget to update require3DInput from the Node.py
        baker = LookFileBaker("UsdExport")
        baker.progressCallback = self.__progressCallback
        baker.additionalSettings = additionalSettings
        baker.sourceAsset = sourceAsset
        baker.sourceFile = sourceFile

        if self.__interruptWidget:
            self.__interruptWidget.show()
            self.__interruptWidget.update("Saving Materials To %s" % assetId,
                                          True)

        try:
            baker.bakeAndPublish(
                referenceOp, passNamesAndOps, rootLocations, assetId)
        finally:
            if self.__interruptWidget:
                self.__interruptWidget.close()
                self.__interruptWidget.setParent(None)
                self.__interruptWidget.deleteLater()
                self.__interruptWidget = None

    def __progressCallback(self, message=None):
        """
        The Callback for displaying progess in the interruptWidget whilst in
        UI mode. This is passed to the look file bake functionality code
        which handles passing back the text to write.

        @type message: C{str}
        @param message: The message to display in the progress bar.
        """
        if not self.__interruptWidget or self.__timer:
            return
        tick = False
        if (time.time() - self.__timer) > 0.1:
            # Ensure we import QtWidgets if we know we have the interrupt
            # widget, which means we must be in a UI session.
            from Katana import QtWidgets
            QtWidgets.QApplication.processEvents()
            self.__timer = time.time()
            tick = True
        return self.__interruptWidget.update(message, tick)

    def __interruptCallback(self):
        class InterruptionException(Exception):
            pass
        raise InterruptionException()

    def _getReferenceInputPort(self):
        return self.getInputPortByIndex(0)

    def _getPassInputPorts(self):
        return self.getInputPorts()[1:]

    def _getReferenceInputPortName(self):
        return self._getReferenceInputPort().getName()

    def _getPassInputPortNames(self):
        return [port.getName() for port in self._getPassInputPorts()]

    def __getBakerOps(self, passInputPortNames, graphState):
        ops = LookFileBaking.GetLookFileBakeOps(
            self, [self._getReferenceInputPortName()] + passInputPortNames,
            graphState)
        referenceOp = ops[0]
        passNamesAndOps = zip(passInputPortNames, ops[1:])
        return (referenceOp, passNamesAndOps)


_parameters_XML = """
    <group_parameter>
        <string_parameter name="__nodePanel" value="UsdMaterialBake"/>
        <number_parameter name="__networkVersion" value="{networkVersion}"/>
        <stringarray_parameter name="rootLocations" size="1" tupleSize="1">
            <string_parameter name="i0" value="/root/world"/>
        </stringarray_parameter>
        <string_parameter name="saveTo"/>
        <string_parameter name="looksFilename" value="shadingVariants"/>
        <string_parameter name="looksFileFormat" value="usd"/>
        <number_parameter name="createCompleteUsdAssemblyFile" value="0"/>
        <string_parameter name="assemblyFilename" value="assembly"/>
        <string_parameter name="payloadFilename" value=""/>
        <string_parameter name="rootPrimName" value="/root"/>
        <string_parameter name="variantSetName" value="shadingVariants"/>
        <string_parameter name="alwaysCreateVariantSet" value="No"/>
        <stringarray_parameter name="variants" size="0" tupleSize="1"/>
    </group_parameter>
"""

_ExtraHints = {

    "UsdMaterialBake": {
    },
    "UsdMaterialBake.rootLocations": {
        "widget": "scenegraphLocationArray",
        "help": """ Specify the rootLocations from which to read the scene
        graph difference from.
        """
    },
    "UsdMaterialBake.variants": {
        "widget": "lookfilebakeinputs",
    },
    "UsdMaterialBake.looksFilename": {
        "help": """
            The name of the file to write variant data into if not writing
            variants into separate files.
        """
    },
    "UsdMaterialBake.looksFileFormat": {
        "widget": "popup",
        "options": ['usd', 'usda', 'usdc'],
        "help": "The type of usd file to write."
    },
    "UsdMaterialBake.createCompleteUsdAssemblyFile": {
        "widget": "boolean",
        "help": """If enabled, An assembly file containing a reference to the
                    created look file and a payload for the specified payload
                    asset.
                """,
        "constant": True
    },
    "UsdMaterialBake.assemblyFilename": {
        "conditionalVisOps": {
            "conditionalVisOp": "equalTo",
            "conditionalVisPath": "../createCompleteUsdAssemblyFile",
            "conditionalVisValue": "1"},
        "help": """Specify the filename to use for the assembly. The file will
                    be written in "usda" format."""
    },
    "UsdMaterialBake.payloadFilename": {
        "conditionalVisOps": {
            "conditionalVisOp": "equalTo",
            "conditionalVisPath": "../createCompleteUsdAssemblyFile",
            "conditionalVisValue": "1"},
        "help": "Specify the filename of an asset to export as a payload.",
        "widget": "assetIdInput"
    },

    "UsdMaterialBake.saveTo": {
        "widget": "assetIdInput",
        "dirsOnly": "True",
        "context": "usd",
    },
    "UsdMaterialBake.alwaysCreateVariantSet": {
        "widget": "boolean",
        "help": """
            If enabled, the exported Usd file will write to the specified
            rootPrimName, and create a variantSet, even if there is only
            one non-original input to this node. VariantSet's will always be
            created if there are multiple variants to bake.
        """
    },
    "UsdMaterialBake.rootPrimName": {
        "help": """
            Specify the root prim name for where shading data will be written
            under. A root prim is required for referencing the resultant
            USD shading stage.
        """
    },
    "UsdMaterialBake.variantSetName": {
        "help": "The name given to the variantSet."
    },
}
