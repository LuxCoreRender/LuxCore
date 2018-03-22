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
import time
import logging
import functools

import PySide.QtCore as QtCore
import PySide.QtGui as QtGui

import pyluxcore
import pyluxcoretools.renderfarm.renderfarm as renderfarm
import pyluxcoretools.renderfarm.renderfarmjobsingleimage as jobsingleimage
import pyluxcoretools.utils.loghandler as loghandler
from pyluxcoretools.utils.logevent import LogEvent
import pyluxcoretools.utils.uiloghandler as uiloghandler
import pyluxcoretools.utils.netbeacon as netbeacon
import pyluxcoretools.pyluxcorenetconsole.mainwindow as mainwindow

logger = logging.getLogger(loghandler.loggerName + ".luxcorenetconsoleui")

class MainApp(QtGui.QMainWindow, mainwindow.Ui_MainWindow, logging.Handler):
	def __init__(self, parent=None):
		super(MainApp, self).__init__(parent)
		self.setupUi(self)
		self.move(QtGui.QApplication.desktop().screen().rect().center()- self.rect().center())
		
		uiloghandler.AddUILogHandler(loghandler.loggerName, self)
		
		self.tabWidgetMain.setTabEnabled(0, False)
		
		self.lineEditHaltSPP.setValidator(QtGui.QIntValidator(0, 9999999))
		self.lineEditHaltTime.setValidator(QtGui.QIntValidator(0, 9999999))
		self.lineEditFilmUpdatePeriod.setValidator(QtGui.QIntValidator(0, 9999999))
		self.lineEditStatsPeriod.setValidator(QtGui.QIntValidator(1, 9999999))

		logger.info("LuxCore %s" % pyluxcore.Version())
		
		#-----------------------------------------------------------------------
		# Create the render farm
		#-----------------------------------------------------------------------

		self.renderFarm = renderfarm.RenderFarm()
		self.renderFarm.Start()
		
		#-----------------------------------------------------------------------
		# Start the beacon receiver
		#-----------------------------------------------------------------------
		
		self.beacon = netbeacon.NetBeaconReceiver(functools.partial(MainApp.__NodeDiscoveryCallBack, self))
		self.beacon.Start()

	def __NodeDiscoveryCallBack(self, ipAddress, port):
		self.renderFarm.DiscoveredNode(ipAddress, port, renderfarm.NodeDiscoveryType.AUTO_DISCOVERED)

	def PrintMsg(self, msg):
		QtCore.QCoreApplication.postEvent(self, LogEvent(msg))

	def __UpdateCurrentJobTab(self):
		currentJob = self.renderFarm.currentJob
		
		if currentJob:
			self.tabWidgetMain.setTabEnabled(0, True)
			
			self.labelRenderCfgFileName.setText("<font color='#0000FF'>" + currentJob.GetRenderConfigFileName() + "</font>")
			self.labelFilmFileName.setText("<font color='#0000FF'>" + currentJob.GetFilmFileName() + "</font>")
			self.labelImageFileName.setText("<font color='#0000FF'>" + currentJob.GetImageFileName() + "</font>")
			self.labelWorkDirectory.setText("<font color='#0000FF'>" + currentJob.GetWorkDirectory() + "</font>")
			self.labelStartTime.setText("<font color='#0000FF'>" + time.strftime("%H:%M:%S %Y/%m/%d", time.localtime(currentJob.GetStartTime())) + "</font>")
			self.lineEditHaltSPP.setText(str(currentJob.GetFilmHaltSPP()))
			self.lineEditHaltTime.setText(str(currentJob.GetFilmHaltTime()))
			self.lineEditFilmUpdatePeriod.setText(str(currentJob.GetFilmUpdatePeriod()))
			self.lineEditStatsPeriod.setText(str(currentJob.GetStatsPeriod()))
		else:
			self.tabWidgetMain.setTabEnabled(0, False)
		
	def clickedAddJob(self):
		fileToRender, _ = QtGui.QFileDialog.getOpenFileName(parent=self,
				caption='Open file to render', filter="Binary render configuration (*.bcf)")
		
		if fileToRender:
			logger.info("Creating single image render farm job: " + fileToRender);
			renderFarmJob = jobsingleimage.RenderFarmJobSingleImage(self.renderFarm, fileToRender)
			self.renderFarm.AddJob(renderFarmJob)
		
			self.__UpdateCurrentJobTab()
		
			self.tabWidgetMain.setCurrentIndex(0)
    
	def editedHaltSPP(self):
		currentJob = self.renderFarm.currentJob

		if currentJob:
			val = max(0, int(self.lineEditHaltSPP.text()))
			currentJob.SetFilmHaltSPP(val)
			logger.info("Halt SPP changed to: %d" % val)

	def editedHaltTime(self):
		currentJob = self.renderFarm.currentJob

		if currentJob:
			val = max(0, int(self.lineEditHaltTime.text()))
			currentJob.SetFilmHaltTime(val)
			logger.info("Halt time changed to: %d" % val)

	def editedFilmUpdatePeriod(self):
		currentJob = self.renderFarm.currentJob

		if currentJob:
			val = max(10, int(self.lineEditFilmUpdatePeriod.text()))
			currentJob.SetFilmUpdatePeriod(val)
			logger.info("Film update period changed to: %d" % val)
		
	def editedStatsPeriod(self):
		currentJob = self.renderFarm.currentJob

		if currentJob:
			val = max(1, int(self.lineEditStatsPeriod.text()))
			currentJob.SetStatsPeriod(val)
			logger.info("Statistics period changed to: %d" % val)

	def clickedForceFilmMerge(self):
		currentJob = self.renderFarm.currentJob

		if currentJob:
			currentJob.ForceFilmMerge()

	def clickedForceFilmDownload(self):
		currentJob = self.renderFarm.currentJob

		if currentJob:
			currentJob.ForceFilmDownload()

	def clickedQuit(self):
		self.close()

	def closeEvent(self, event):
		# Stop the beacon receiver
		self.beacon.Stop()

		# Stop the render farm
		self.renderFarm.Stop()

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

		return QtGui.QWidget.event(self, event)

def ui(app):
	try:
		pyluxcore.Init(loghandler.LuxCoreLogHandler)

		form = MainApp()
		form.show()
		
		app.exec_()
	finally:
		pyluxcore.SetLogHandler(None)

def main(argv):
	app = QtGui.QApplication(sys.argv)
	ui(app)

if __name__ == "__main__":
	main(sys.argv)
