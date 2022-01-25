#!/usr/bin/python
# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2018 by authors (see AUTHORS.txt)
#
#   This file is part of LuxCoreRender.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

import sys
import logging

try:
	import PySide.QtCore as QtCore
	import PySide.QtGui as QtGui
	import PySide.QtGui as QtWidgets
	PYSIDE_V = int(QtCore.qVersion()[:1])
except ImportError:
	try:
		from PySide2 import QtGui, QtCore, QtWidgets
		PYSIDE_V = int(QtCore.qVersion()[:1])
	except ImportError:
		from PySide6 import QtGui, QtCore, QtWidgets
		PYSIDE_V = int(QtCore.qVersion()[:1])

import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.pyluxcoremenu.menuwindow as menuwindow
import pyluxcoretools.pyluxcorenetnode.ui
import pyluxcoretools.pyluxcorenetconsole.ui

logger = logging.getLogger(loghandler.loggerName + ".luxcoremenu")

class MenuApp(QtWidgets.QMainWindow, menuwindow.Ui_MenuWindow):
	def __init__(self, parent=None):
		self.selectedTool = "None"

		super(MenuApp, self).__init__(parent)
		self.setupUi(self)
		if PYSIDE_V < 5:
			self.move(QtWidgets.QApplication.desktop().screen().rect().center()- self.rect().center())

	def clickedNetNode(self):
		self.selectedTool = "NetNode"
		self.close()

	def clickedNetConsole(self):
		self.selectedTool = "NetConsole"
		self.close()

def main(argv):
	app = QtWidgets.QApplication(sys.argv)
	form = MenuApp()
	form.show()

	app.exec_()

	if form.selectedTool == "NetNode":
		pyluxcoretools.pyluxcorenetnode.ui.ui(app)
	elif form.selectedTool == "NetConsole":
		pyluxcoretools.pyluxcorenetconsole.ui.ui(app)

if __name__ == "__main__":
	main(sys.argv)
