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

from Katana import QtWidgets, QtCore, QtGui
from Katana import NodegraphAPI, Utils
import UI4

class VariantsWidget(QtWidgets.QFrame):
    """ Main Variants widget, contains a toolbar and the VariantsListWidget.
    """

    def __init__(self, node, newVariantName=None, parent=None):
        """
        @param node: The UsdMaterialBake node which should have the following
            methods. addVariantInput, deleteVariantInput, reorderInput,
            renameVariantInput, require3DInput, and the MIN_PORTS attribute.
        @param newVariantName: The name to use when adding new variants with
            the plus button.  If not set, it will default to "variant"
        @param parent: The Qt parent widget for this QFrame.
        @type node: C{GroupNode} (Supertool)
        @type newVariantName: C{str}
        @type parent: C{QtWidgets.Widget}
        """
        super(VariantsWidget, self).__init__(parent=parent)
        self.node = node
        self.newVariantName = newVariantName or "variant"
        self.minPorts = self.node.MIN_PORTS
        self.setAutoFillBackground(True)
        self.setBackgroundRole(QtGui.QPalette.Base)

        Utils.EventModule.RegisterEventHandler(
            self.__onAddOrRemoveInputPort, 'node_addInputPort', None)
        Utils.EventModule.RegisterEventHandler(
            self.__onAddOrRemoveInputPort, 'node_removeInputPort', None)

        # Toolbar to work as heading for Variants widget.
        self.toolbar = QtWidgets.QToolBar(self)
        self.toolbar.setMovable(False)
        variantsLabel = QtWidgets.QLabel("Variants")
        variantsLabel.setIndent(6)
        self.toolbar.addWidget(variantsLabel)

        # Spacer widget to allow us to align some widgets to the right of the
        # toolbar.
        spacerWidget = QtWidgets.QWidget(self)
        spacerWidget.setSizePolicy(QtWidgets.QSizePolicy.Expanding,
                                   QtWidgets.QSizePolicy.Preferred)
        spacerWidget.setVisible(True)
        self.toolbar.addWidget(spacerWidget)

        # Toolbar Actions
        addIcon = UI4.Util.IconManager.GetIcon('Icons/plus16.png')
        self.addAction = self.toolbar.addAction(addIcon, "Add")
        self.addAction.triggered.connect(self.addNewVariant)

        # List Widget
        self.listWidget = VariantsListWidget(self)
        self.listWidget.itemChanged.connect(self.__onCurrentTextChanged)
        self.listWidget.customContextMenuRequested.connect(
            self.customContextMenu)
        self.listWidget.model().rowsMoved.connect(self.__onRowMoved)

        self.deleteSelectedAction = QtWidgets.QAction('Delete',
                                                      self.listWidget)
        self.listWidget.addAction(self.deleteSelectedAction)
        self.deleteSelectedAction.triggered.connect(self.deleteSelectedVariant)
        self.deleteSelectedAction.setShortcut(QtCore.Qt.Key_Delete)

        # Layout
        layout = QtWidgets.QVBoxLayout()
        layout.setSpacing(0)
        self.setLayout(layout)
        layout.addWidget(self.toolbar)
        layout.addWidget(self.listWidget)
        self.updateWidgets()

    def updateWidgets(self):
        """ Perform a clean reset of the widget and its items.
        """
        self.listWidget.clear()
        # Ensure self.minPorts default ports always available.
        for port in self.node.getInputPorts()[self.minPorts:]:
            item = QtWidgets.QListWidgetItem(port.getName())
            item.setFlags(
                QtCore.Qt.ItemIsDragEnabled | QtCore.Qt.ItemIsDropEnabled |
                QtCore.Qt.ItemIsEnabled | QtCore.Qt.ItemNeverHasChildren |
                QtCore.Qt.ItemIsSelectable | QtCore.Qt.ItemIsEditable)
            self.listWidget.addItem(item)

    def addNewVariant(self):
        """ Add a new input port to this widgets attached node. """
        self.node.addVariantInput(self.newVariantName)

    def deleteSelectedVariant(self):
        """ Delete the input port on this widgets attached node matching the
            currently selected items name.
        """
        index = self.listWidget.currentRow() + self.node.MIN_PORTS
        self.node.deleteVariantInput(index)

    def __onCurrentTextChanged(self, item):
        """ Update the input ports name on the node attached to this widget.
        """
        itemIndex = self.listWidget.row(item) + self.minPorts
        self.node.renameVariantInput(itemIndex, item.text())

    def customContextMenu(self, pos):
        """ Open up a custom context menu. """
        # pylint: disable=unused-argument
        self.deleteSelectedAction.setEnabled(self.listWidget.currentRow() > -1)
        menu = QtWidgets.QMenu()
        menu.addAction(self.deleteSelectedAction)
        menu.exec_(QtGui.QCursor.pos())

    # pylint: disable=undefined-variable
    def __onRowMoved(self, parent, start, end, dest, row):
        """ Update the order of the inputs on the attached node when a row has
            been moved in the listWidget.
        """
        # Because we are moving an item, the resultant row is one earlier than
        # Qt thinks at this point, because it counts our item as existing
        # still.
        if row > start:
            row = row-1
        self.node.reorderInput(start + self.minPorts, row + self.minPorts)

    def __onAddOrRemoveInputPort(self, eventType, eventID,
                               nodeName, portName, port, **kwargs):
        """ When we add or remove input ports, ensure we trigger a new update.
        """
        self.updateWidgets()


class VariantsListWidget(QtWidgets.QListWidget):
    """ A List widget setup to allow for custom context menu's, drag and drop
        as well as some custom styling to match Katana's UI.
    """

    def __init__(self, parent=None):
        super(VariantsListWidget, self).__init__(parent=parent)
        self.setAlternatingRowColors(True)
        self.setAutoFillBackground(True)
        self.setBackgroundRole(QtGui.QPalette.AlternateBase)
        self.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.setDragEnabled(True)
        self.setDragDropMode(QtWidgets.QAbstractItemView.InternalMove)
        self.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self.setStyleSheet(
            "QListWidget {padding: 6px;} QListWidget::item { margin: 3px; }")
        self.setUniformItemSizes(True)
