# Copyright (c) 2022 The Foundry Visionmongers Ltd. All Rights Reserved.
"""
Global initialisation for this test package.
"""
from __future__ import absolute_import

from PyQt5 import QtWidgets

import TestUtils.Integration

session = TestUtils.Integration.Initialize(withKatanaModule=True)
app = QtWidgets.QApplication([])
