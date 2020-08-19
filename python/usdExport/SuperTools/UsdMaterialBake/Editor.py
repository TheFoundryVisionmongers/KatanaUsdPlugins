# Copyright (c) 2020 The Foundry Visionmongers Ltd. All Rights Reserved.

import logging
import time

from Katana import QtWidgets, QtCore, QtGui
from Katana import NodegraphAPI, Utils
import LookFileBakeAPI
import UI4

from .VariantsWidget import VariantsWidget

log = logging.getLogger("UsdMaterialBake.Editor")

NodegraphAPI.AddNodeFlavor("UsdMaterialBake", "3d")
NodegraphAPI.AddNodeFlavor("UsdMaterialBake", "lookfile")

class UsdMaterialBakeEditor(QtWidgets.QFrame):
    """ The editor used for the UsdMaterialBake node parameters tab.
    """

    def __init__(self, parent, group):
        super(UsdMaterialBakeEditor, self).__init__(parent=parent)
        self.setLayout(QtWidgets.QVBoxLayout())

        self.__groupNode = group
        self.__interruptWidget = None
        self.__timer = None

        #For auto upgrade in the future
        # pylint: disable=undefined-variable
        versionValue = 1
        versionParam = group.getParameter('__networkVersion')

        if versionParam:
            versionValue = int(versionParam.getValue(0))

        self.__widgetFactory = UI4.FormMaster.ParameterWidgetFactory
        widgets = []
        self.__rootPolicy = UI4.FormMaster.CreateParameterPolicy(
            None, group.getParameters())
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('rootLocations')))
        variantsWidget = VariantsWidget(self.__groupNode, parent=self)
        widgets.append(variantsWidget)
        widgets.append(UI4.Widgets.VBoxLayoutResizer(variantsWidget))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('saveTo')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('fileName')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('fileFormat')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('alwaysCreateVariantSet')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('rootPrimName')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('variantSetName')))

        labelWidthList = []
        for w in widgets:
            if hasattr(w, "getLabelWidth"):
                labelWidthList.append(w.getLabelWidth())
        maxLabelWidth = reduce(max, labelWidthList)

        for w in widgets:
            if hasattr(w, "setLabelWidth"):
                w.setLabelWidth(maxLabelWidth)
            self.layout().addWidget(w)

        self.__writeButton = QtWidgets.QPushButton("Write", parent=self)
        self.__writeButton.clicked.connect(
            lambda checked : self.__groupNode.bake(parentWidget=self))
        self.layout().addWidget(self.__writeButton)
