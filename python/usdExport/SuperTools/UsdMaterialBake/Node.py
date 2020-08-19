# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

import logging
import time

from Katana import NodegraphAPI, Utils
import LookFileBakeAPI

log = logging.getLogger("UsdMaterialBake.Node")

__all__ = ['UsdMaterialBakeNode']


class UsdMaterialBakeNode(NodegraphAPI.SuperTool):
    """
        UsdMaterialBake node, with extra methods to allow for baking features
        and ability to change and re-order inputs driven by the Editor UI, or
        by calling these directly.
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
            # The node factory does not print exceptions, lame
            log.exception("CREATE UsdMaterialBake FAILED.")
            raise
        self.__timer = None
        self.__interruptWidget = None

    # --- Public node API -------------------------------------------
    def addVariantInput(self, variantName):
        """
            Add a new input port for a new variant name.
            @param variantName: The new name to give to the new port.
            @type variantName: C{str}
        """
        self.addInputPort(variantName)

    def deleteVariantInput(self, index):
        """
            Delete existing variant.
            @param index: The index of the input port to delete
            @type index: C{int}
        """
        if index < self.MIN_PORTS or index >= self.getNumInputPorts():
            return

        portName = self.getInputPortByIndex(index).getName()
        self.removeInputPort(portName)

    def reorderInput(self, index, newIndex):
        """
            Reorder two input variants. By Deleting the old port and recreating
            it in the new index.

            @param index: Index of the input port to reposition
            @param newIndex: The new position of the input port. Assuming that
                the current index does not exist.
            @type index: C{int}
            @type newIndex: C{int}
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
            Rename an existing input name.
            Does not change the input order.

            @param index: Index of the input to rename.
            @param newName: The new name to change the port to/
            @type index: C{int}
            @type newName: C{str}
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
        inputDict.update(_ExtraHints.get(attrName, {}))

    def require3DInput(self, portName, graphState):
        """
            A Method from the 3DNode used inside the look file bake code.
            Used to gather the input port given the port name and graphstate.

            @param portName: The port name to read the 3dInput from.
            @param newIndex: The graph state used to get the input source.
            @type portName: C{str}
            @type newIndex: C{NodegraphAPI.GraphState}
            @raises: TypeError, RuntimeError
            @returns: Tuple of the source Node, port and graphState
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
            parameter settings.

            @param parentWidget: This will be the parent that the progress
                widget will be parented to.  If set to None, the progress
                callback is not produced.
            @type parentWidget: C{QtWidgets.QWidget}
        """
        graphState = NodegraphAPI.GetCurrentGraphState()
        frameTime = graphState.getTime()
        node = self
        inputPorts = node.getInputPorts()
        numPorts = len(inputPorts)
        if numPorts < 2:
            log.error("Requires at least two ports to bake a Look file")
        variantSetName = node.getParameter(
            "variantSetName").getValue(frameTime)
        rootPrimName = node.getParameter(
            "rootPrimName").getValue(frameTime)
        alwaysCreateVariantSet = bool(node.getParameter(
            "alwaysCreateVariantSet").getValue(frameTime) == "Yes")
        fileName = node.getParameter("fileName").getValue(
            frameTime)
        fileFormat = node.getParameter("fileFormat").getValue(frameTime)

        createVariantSet = alwaysCreateVariantSet or (len(inputPorts) > 2)
        additionalSettings = {
            "variantSetName" : variantSetName,
            "rootPrimName" : rootPrimName,
            "createVariantSet" : createVariantSet,
            "fileFormat" : fileFormat,
            "fileName" : fileName
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

        #When updating to the later version of the LookFileBakeAPI, dont
        # forget to update require3DInput from the Node.py
        lfbf = LookFileBakeAPI.Functionality.LookFileBakeFunctionality(
            node,
            "UsdExport",
            originalInputName=None,
            evictionLocationInterval=None,
            includeGlobalAttributes=None,
            includeLodInfo=None,
            progressCallback=self.__progressCallback,
            materialTreeRootLocations=None,
            additionalSettings=additionalSettings)

        assetId = node.getParameter("saveTo").getValue(frameTime)
        rootLocationsParam = node.getParameter("rootLocations")
        rootLocations = [x.getValue(frameTime) for x in
            rootLocationsParam.getChildren()]

        if self.__interruptWidget:
            self.__interruptWidget.show()
            self.__interruptWidget.update("Saving Materials To %s" % assetId,
                True)

        lfbf.WriteToAsset(graphState, assetId, rootLocations)

        if self.__interruptWidget:
            self.__interruptWidget.close()
            self.__interruptWidget.setParent(None)
            self.__interruptWidget.deleteLater()
            self.__interruptWidget = None

    def __progressCallback(self, message=None):
        """
            Callback for displaying progess in the interruptWidget whilst in
            UI mode. This is passed to the look file bake functionality code
            which handles passing back the text to write.
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


_parameters_XML = """
    <group_parameter>
        <string_parameter name="__nodePanel" value="UsdMaterialBake"/>
        <number_parameter name="__networkVersion" value="{networkVersion}"/>
        <stringarray_parameter name="rootLocations" size="1" tupleSize="1">
            <string_parameter name="i0" value="/root/world"/>
        </stringarray_parameter>
        <string_parameter name="saveTo"/>
        <string_parameter name='fileName' value='shadingVariants'/>
        <string_parameter name="fileFormat" value="usd"/>
        <string_parameter name="rootPrimName" value="/root"/>
        <string_parameter name="variantSetName" value="shadingVariants"/>
        <string_parameter name='alwaysCreateVariantSet' value='No'/>
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
    "UsdMaterialBake.fileName": {
        "help": """
            The name of the file to write variant data into if not writing
            variants into separate files.
        """
    },
    "UsdMaterialBake.fileFormat": {
        "widget" : "popup",
        "options": ['usd', 'usda', 'usdc'],
        "help": "The type of usd file to write."
    },
    "UsdMaterialBake.saveTo": {
        "widget": "assetIdInput",
        "scope": "product",
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
            under.  A root prim is required for referencing the resultant
            USD shading stage.
        """
    },
    "UsdMaterialBake.variantSetName": {
        "help": "The name given to the variantSet."
    },
}
