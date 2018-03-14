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
import threading

import PySide.QtCore as QtCore
import PySide.QtGui as QtGui

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.renderfarm.renderfarm as renderfarm
import pyluxcoretools.renderfarm.renderfarmnode as renderfarmnode
import pyluxcoretools.pyluxcorenetnode.mainwindow as mainwindow

logger = logging.getLogger(loghandler.loggerName + ".luxcorenetnode")

class LogEvent(QtCore.QEvent):
	EVENT_TYPE = QtCore.QEvent.Type(QtCore.QEvent.registerEventType())

	def __init__(self, msg):
		super(LogEvent, self).__init__(self.EVENT_TYPE)
		self.msg = msg + "\n"

class UILogHandler(logging.Handler):
	def __init__(self, app):
		logging.Handler.__init__(self)
		self.level = logging.NOTSET

		self.app = app
		
	# Logging handler callback
	def emit(self, record):
		msg = self.format(record)
		self.app.PrintMsg(msg)

class MainApp(QtGui.QMainWindow, mainwindow.Ui_MainWindow, logging.Handler):
	def __init__(self, parent=None):
		super(MainApp, self).__init__(parent)
		self.setupUi(self)
		self.move(QtGui.QApplication.desktop().screen().rect().center()- self.rect().center())
		
		self.uiLogHandler = UILogHandler(self)
		logging.getLogger(loghandler.loggerName).addHandler(self.uiLogHandler)
		
		logger.info("LuxCore %s" % pyluxcore.Version())
		
		self.renderFarmNode = renderfarmnode.RenderFarmNode("", renderfarm.DEFAULT_PORT, "<broadcast>", 3.0, pyluxcore.Properties())
		self.renderFarmNodeThread = threading.Thread(target=self.renderFarmNode.Run)
		self.renderFarmNodeThread.start()

	def PrintMsg(self, msg):
		QtCore.QCoreApplication.postEvent(self, LogEvent(msg))

	def clickedQuit(self):
		self.close()
	
	def event(self, event):
		if event.type() == LogEvent.EVENT_TYPE:
			self.plainTextEditLog.moveCursor(QtGui.QTextCursor.End)
			self.plainTextEditLog.insertPlainText(event.msg)
			return True

		return QtGui.QWidget.event(self, event)

def ui(app):
	try:
		pyluxcore.Init(loghandler.LuxCoreLogHandler)

		form = MainApp()
		form.show()
		
		app.exec_()
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
	app = QtGui.QApplication(sys.argv)
	ui(app)
