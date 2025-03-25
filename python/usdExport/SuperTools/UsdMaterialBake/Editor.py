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

from functools import reduce
import logging
import time

from PySide6 import QtWidgets

from Katana import NodegraphAPI
import UI4
from .VariantsWidget import VariantsWidget


log = logging.getLogger("UsdMaterialBake.Editor")

NodegraphAPI.AddNodeFlavor("UsdMaterialBake", "3d")
NodegraphAPI.AddNodeFlavor("UsdMaterialBake", "usdin")


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
            self, self.__rootPolicy.getChildByName('looksFilename')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('looksFileFormat')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('alwaysCreateVariantSet')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName(
                'createCompleteUsdAssemblyFile')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('assemblyFilename')))
        widgets.append(self.__widgetFactory.buildWidget(
            self, self.__rootPolicy.getChildByName('payloadFilename')))
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
