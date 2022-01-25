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

# For some unknown reason, I need to import pyluxcore before PySide2 or
# it will break streams and parsing in an unpredictable way
import pyluxcore

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

import pyluxcoretools.renderfarm.renderfarm as renderfarm
import pyluxcoretools.renderfarm.renderfarmnode as renderfarmnode
import pyluxcoretools.utils.loghandler as loghandler
from pyluxcoretools.utils.logevent import LogEvent
import pyluxcoretools.utils.uiloghandler as uiloghandler
import pyluxcoretools.pyluxcorenetnode.mainwindow as mainwindow

logger = logging.getLogger(loghandler.loggerName + ".luxcorenetnodeui")

class MainApp(QtWidgets.QMainWindow, mainwindow.Ui_MainWindow, logging.Handler):
	def __init__(self, parent=None):
		super(MainApp, self).__init__(parent)
		self.setupUi(self)

		if PYSIDE_V < 5:
			self.move(QtWidgets.QApplication.desktop().screen().rect().center()- self.rect().center())
		
		uiloghandler.AddUILogHandler(loghandler.loggerName, self)
		
		if PYSIDE_V >= 6:
			ipRegExp = QtCore.QRegularExpression("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$")
			ipRegExpVal = QtGui.QRegularExpressionValidator(ipRegExp)
		else:
			ipRegExp = QtCore.QRegExp("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$")
			ipRegExpVal = QtGui.QRegExpValidator(ipRegExp)
		self.lineEditIPAddress.setValidator(ipRegExpVal)
		self.lineEditPort.setValidator(QtGui.QIntValidator(0, 65535))
		self.lineEditBroadcastAddress.setValidator(ipRegExpVal)

		self.__ResetConfigUI()
	
		logger.info("LuxCore %s" % pyluxcore.Version())
		
		self.renderFarmNode = None
		
		self.PrintMsg("Waiting for configuration...")

	def __ResetConfigUI(self):
		self.lineEditIPAddress.setText("")
		self.lineEditPort.setText(str(renderfarm.DEFAULT_PORT))
		self.lineEditBroadcastAddress.setText("<broadcast>")
		self.lineEditBroadcastPeriod.setText(str(3.0))
		self.plainTextEditProps.clear()

	def PrintMsg(self, msg):
		QtCore.QCoreApplication.postEvent(self, LogEvent(msg))

	def clickedResetConfig(self):
		self.__ResetConfigUI()

	def clickedStartNode(self):
		self.frameNodeConfig.setVisible(False)
		self.pushButtonStartNode.setEnabled(False)
		self.pushButtonStopNode.setEnabled(True)

		self.renderFarmNode = renderfarmnode.RenderFarmNode(
				self.lineEditIPAddress.text(),
				int(self.lineEditPort.text()),
				self.lineEditBroadcastAddress.text(),
				float(self.lineEditBroadcastPeriod.text()),
				pyluxcore.Properties())
		self.renderFarmNode.Start()
		
		self.PrintMsg("Started")

	def clickedStopNode(self):
		self.renderFarmNode.Stop()
		self.renderFarmNode = None
		
		self.pushButtonStartNode.setEnabled(True)
		self.pushButtonStopNode.setEnabled(False)
		self.frameNodeConfig.setVisible(True)
		self.PrintMsg("Waiting for configuration...")

	def clickedQuit(self):
		self.close()

	def closeEvent(self, event):
		if (self.renderFarmNode):
			self.renderFarmNode.Stop()
	
		event.accept()
	
	def event(self, event):
		if event.type() == LogEvent.EVENT_TYPE:
			self.textEditLog.moveCursor(QtGui.QTextCursor.End)
			if event.isHtml:
				self.textEditLog.insertHtml(event.msg)
				self.textEditLog.insertPlainText("\n")
			else:
				self.textEditLog.insertPlainText(event.msg)
				
				# Show few specific messages on the status bar
				if event.msg.startswith("Waiting for a new connection") or \
						event.msg.startswith("Started") or \
						event.msg.startswith("Waiting for configuration..."):
					self.statusbar.showMessage(event.msg)

			return True

		return QtWidgets.QWidget.event(self, event)

def ui(app):
	try:
		pyluxcore.Init(loghandler.LuxCoreLogHandler)

		form = MainApp()
		form.show()
		
		app.exec_()
	finally:
		pyluxcore.SetLogHandler(None)

def main(argv):
	app = QtWidgets.QApplication(sys.argv)
	ui(app)

if __name__ == "__main__":
	main(sys.argv)
