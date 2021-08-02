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
import socket

# For some unknown reason, I need to import pyluxcore before PySide2 or
# it will break streams and parsing in an unpredictable way
import pyluxcore

try:
	import PySide.QtCore as QtCore
	import PySide.QtGui as QtGui
	import PySide.QtGui as QtWidgets
	PYSIDE2 = False
except ImportError:
	from PySide2 import QtGui, QtCore, QtWidgets
	PYSIDE2 = True

import pyluxcoretools.renderfarm.renderfarm as renderfarm
import pyluxcoretools.renderfarm.renderfarmjobsingleimage as jobsingleimage
import pyluxcoretools.utils.loghandler as loghandler
from pyluxcoretools.utils.logevent import LogEvent
import pyluxcoretools.utils.uiloghandler as uiloghandler
import pyluxcoretools.utils.netbeacon as netbeacon
import pyluxcoretools.pyluxcorenetconsole.mainwindow as mainwindow
import pyluxcoretools.pyluxcorenetconsole.addnodedialog as addnodedialog

logger = logging.getLogger(loghandler.loggerName + ".luxcorenetconsoleui")

class CurrentJobUpdateEvent(QtCore.QEvent):
	EVENT_TYPE = QtCore.QEvent.Type(QtCore.QEvent.registerEventType())

	def __init__(self):
		super(CurrentJobUpdateEvent, self).__init__(self.EVENT_TYPE)

class JobsUpdateEvent(QtCore.QEvent):
	EVENT_TYPE = QtCore.QEvent.Type(QtCore.QEvent.registerEventType())

	def __init__(self):
		super(JobsUpdateEvent, self).__init__(self.EVENT_TYPE)

class NodesUpdateEvent(QtCore.QEvent):
	EVENT_TYPE = QtCore.QEvent.Type(QtCore.QEvent.registerEventType())

	def __init__(self):
		super(NodesUpdateEvent, self).__init__(self.EVENT_TYPE)

class QueuedJobsTableModel(QtCore.QAbstractTableModel):
	def __init__(self, parent, renderFarm, * args):
		QtCore.QAbstractTableModel.__init__(self, parent, * args)
		self.renderFarm = renderFarm

	def rowCount(self, parent):
		return self.renderFarm.GetQueuedJobCount()

	def columnCount(self, parent):
		return 2

	def data(self, index, role):
		if not index.isValid():
			return None
		elif role != QtCore.Qt.DisplayRole:
			return None
		else:
			if index.column() == 0:
				return index.row()
			elif index.column() == 1:
				return self.renderFarm.GetQueuedJobList()[index.row()].GetRenderConfigFileName()
			else:
				return ""

	def headerData(self, col, orientation, role):
		if orientation == QtCore.Qt.Horizontal and role == QtCore.Qt.DisplayRole:
			if col == 0:
				return "#";
			elif col == 1:
				return "Render configuration"
			else:
				return ""
		return None

	def Update(self):
		self.emit(QtCore.SIGNAL("layoutChanged()"))

class NodesTableModel(QtCore.QAbstractTableModel):
	def __init__(self, parent, renderFarm, * args):
		QtCore.QAbstractTableModel.__init__(self, parent, * args)
		self.renderFarm = renderFarm

	def rowCount(self, parent):
		return self.renderFarm.GetNodesListCount()

	def columnCount(self, parent):
		return 3

	def data(self, index, role):
		if not index.isValid():
			return None
		elif role != QtCore.Qt.DisplayRole:
			return None
		else:
			if index.column() == 0:
				return index.row()
			elif index.column() == 1:
				node = self.renderFarm.GetNodesList()[index.row()]

				return node.GetKey()
			elif index.column() == 2:
				node = self.renderFarm.GetNodesList()[index.row()]

				return node.state.name
			else:
				return ""

	def headerData(self, col, orientation, role):
		if orientation == QtCore.Qt.Horizontal and role == QtCore.Qt.DisplayRole:
			if col == 0:
				return "#";
			elif col == 1:
				return "Rendering node address"
			elif col == 2:
				return "Status"
			else:
				return ""

		return None

	def Update(self):
		self.emit(QtCore.SIGNAL("layoutChanged()"))

class AddNodeDialog(QtWidgets.QDialog, addnodedialog.Ui_DialogAddNode):
	def __init__(self, parent = None):
		super(AddNodeDialog, self).__init__(parent)
		self.setupUi(self)

		ipRegExp = QtCore.QRegExp("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$")
		self.lineEditIPAddress.setValidator(QtGui.QRegExpValidator(ipRegExp))
		self.lineEditPort.setValidator(QtGui.QIntValidator(0, 65535))
		self.lineEditPort.setText(str(renderfarm.DEFAULT_PORT))

		if not PYSIDE2:
			self.move(QtWidgets.QApplication.desktop().screen().rect().center()- self.rect().center())
		
	def GetIPAddress(self):
		return self.lineEditIPAddress.text()
	
	def GetPort(self):
		return self.lineEditPort.text()
		

class MainApp(QtWidgets.QMainWindow, mainwindow.Ui_MainWindow, logging.Handler):
	def __init__(self, parent=None):
		super(MainApp, self).__init__(parent)
		self.setupUi(self)

		if not PYSIDE2:
			self.move(QtWidgets.QApplication.desktop().screen().rect().center()- self.rect().center())
		
		uiloghandler.AddUILogHandler(loghandler.loggerName, self)
		
		self.tabWidgetMain.setTabEnabled(0, False)
		self.tabWidgetMain.setCurrentIndex(1)
		
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
		
		#-----------------------------------------------------------------------
		# Create the queued jobs widget table
		#-----------------------------------------------------------------------

		self.queuedJobsTableModel = QueuedJobsTableModel(self, self.renderFarm)
		self.queuedJobsTableView = QtWidgets.QTableView()
		self.queuedJobsTableView.setModel(self.queuedJobsTableModel)
		self.queuedJobsTableView.resizeColumnsToContents()

		self.vboxLayoutQueuedJobs = QtWidgets.QVBoxLayout(self.scrollAreaQueuedJobs)
		self.vboxLayoutQueuedJobs.setObjectName("vboxLayoutQueuedJobs")
		self.vboxLayoutQueuedJobs.addWidget(self.queuedJobsTableView)
		self.scrollAreaQueuedJobs.setLayout(self.vboxLayoutQueuedJobs)

		#-----------------------------------------------------------------------
		# Create the nodes widget table
		#-----------------------------------------------------------------------

		self.nodesTableModel = NodesTableModel(self, self.renderFarm)
		self.nodesTableView = QtWidgets.QTableView()
		self.nodesTableView.setModel(self.nodesTableModel)
		self.nodesTableView.resizeColumnsToContents()

		self.vboxLayoutNodes = QtWidgets.QVBoxLayout(self.scrollAreaNodes)
		self.vboxLayoutNodes.setObjectName("vboxLayoutNodes")
		self.vboxLayoutNodes.addWidget(self.nodesTableView)
		self.scrollAreaNodes.setLayout(self.vboxLayoutNodes)

		#-----------------------------------------------------------------------

		self.renderFarm.SetJobsUpdateCallBack(functools.partial(MainApp.__RenderFarmJobsUpdateCallBack, self))
		self.__RenderFarmJobsUpdateCallBack()
		
		self.renderFarm.SetNodesUpdateCallBack(functools.partial(MainApp.__RenderFarmNodesUpdateCallBack, self))
		self.__RenderFarmNodesUpdateCallBack()

	def PrintMsg(self, msg):
		QtCore.QCoreApplication.postEvent(self, LogEvent(msg))

	def __FormatSamplesSec(self, val):
		if val < 1000000.0:
			return "%.1f" % (val / 1000.0) + "K"
		else:
			return "%.1f" % (val / 1000000.0) + "M"

	def __NodeDiscoveryCallBack(self, ipAddress, port):
		self.renderFarm.DiscoveredNode(ipAddress, port, renderfarm.NodeDiscoveryType.AUTO_DISCOVERED)

	def __RenderFarmJobsUpdateCallBack(self):
		QtCore.QCoreApplication.postEvent(self, JobsUpdateEvent())

	def __RenderFarmNodesUpdateCallBack(self):
		QtCore.QCoreApplication.postEvent(self, NodesUpdateEvent())

	def __CurrentJobUpdateCallBack(self):
		QtCore.QCoreApplication.postEvent(self, CurrentJobUpdateEvent())

	def __UpdateNodesTab(self):
		self.nodesTableModel.Update()
		self.nodesTableView.resizeColumnsToContents()
	
	def __UpdateCurrentRenderingImage(self):
		currentJob = self.renderFarm.currentJob
		
		if currentJob:
			pixMap = QtGui.QPixmap(currentJob.GetImageFileName())

			if pixMap.isNull():
				self.labelRenderingImage.setPixmap(None)
				self.labelRenderingImage.setText("Waiting for film download and merge")
				self.labelRenderingImage.show()
			else:
				self.labelRenderingImage.setPixmap(pixMap)
				self.labelRenderingImage.setText("")
				self.labelRenderingImage.resize(pixMap.size())
				self.labelRenderingImage.show()
		else:
			self.labelRenderingImage.setPixmap(None)
			self.labelRenderingImage.setText("N/A")
			self.labelRenderingImage.show()

	def __UpdateCurrentJobTab(self):
		currentJob = self.renderFarm.currentJob
		
		if currentJob:
			self.tabWidgetMain.setTabEnabled(0, True)
			
			self.labelRenderCfgFileName.setText("<font color='#0000FF'>" + currentJob.GetRenderConfigFileName() + "</font>")
			self.labelFilmFileName.setText("<font color='#0000FF'>" + currentJob.GetFilmFileName() + "</font>")
			self.labelImageFileName.setText("<font color='#0000FF'>" + currentJob.GetImageFileName() + "</font>")
			self.labelWorkDirectory.setText("<font color='#0000FF'>" + currentJob.GetWorkDirectory() + "</font>")
			
			renderingStartTime = currentJob.GetStartTime()
			self.labelStartTime.setText("<font color='#0000FF'>" + time.strftime("%H:%M:%S %Y/%m/%d", time.localtime(renderingStartTime)) + "</font>")

			dt = time.time() - renderingStartTime
			self.labelRenderingTime.setText("<font color='#0000FF'>" + time.strftime("%H:%M:%S", time.gmtime(dt)) + "</font>")
			self.labelSamplesPixel.setText("<font color='#0000FF'>" + "%.1f" % (currentJob.GetSamplesPixel()) + "</font>")
			self.labelSamplesSec.setText("<font color='#0000FF'>" + self.__FormatSamplesSec(currentJob.GetSamplesSec()) + "</font>")

			self.lineEditHaltSPP.setText(str(currentJob.GetFilmHaltSPP()))
			self.lineEditHaltTime.setText(str(currentJob.GetFilmHaltTime()))
			self.lineEditFilmUpdatePeriod.setText(str(currentJob.GetFilmUpdatePeriod()))
			self.lineEditStatsPeriod.setText(str(currentJob.GetStatsPeriod()))

			# Update the RenderingImage
			self.__UpdateCurrentRenderingImage()

			currentJob.SetJobUpdateCallBack(self.__CurrentJobUpdateCallBack)
		else:
			self.tabWidgetMain.setTabEnabled(0, False)
	
	def __UpdateQueuedJobsTab(self):
		self.queuedJobsTableModel.Update()
		self.queuedJobsTableView.resizeColumnsToContents()

	def clickedAddNode(self):
		dialog = AddNodeDialog(self)
		if dialog.exec_() == QtWidgets.QDialog.Accepted:
			ipAddress = dialog.GetIPAddress()
			port = dialog.GetPort()

			# Check if it is a valid ip address
			try:
				socket.inet_aton(ipAddress)
			except socket.error:
				raise SyntaxError("Rendering node ip address syntax error: " + node)

			# Check if it is a valid port
			try:
				port = int(port)
			except ValueError:
				raise SyntaxError("Rendering node port syntax error: " + node)
			
			self.renderFarm.DiscoveredNode(ipAddress, port, renderfarm.NodeDiscoveryType.MANUALLY_DISCOVERED)

	def clickedAddJob(self):
		fileToRender, _ = QtWidgets.QFileDialog.getOpenFileName(parent=self,
				caption='Open file to render', filter="Binary render configuration (*.bcf)")
		
		if fileToRender:
			logger.info("Creating single image render farm job: " + fileToRender);
			renderFarmJob = jobsingleimage.RenderFarmJobSingleImage(self.renderFarm, fileToRender)
			self.renderFarm.AddJob(renderFarmJob)
		
			self.__UpdateCurrentJobTab()
			self.__UpdateQueuedJobsTab()
		
			self.tabWidgetMain.setCurrentIndex(0)
    
	def clickedRemovePendingJobs(self):
		self.renderFarm.RemovePendingJobs()
		self.__UpdateQueuedJobsTab()

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

	def clickedFinishJob(self):
		self.renderFarm.StopCurrentJob()
		self.__UpdateCurrentJobTab()
		self.__UpdateQueuedJobsTab()

	def clickedRefreshNodesList(self):
		self.__RenderFarmNodesUpdateCallBack()

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
		elif event.type() == CurrentJobUpdateEvent.EVENT_TYPE:
			self.__UpdateCurrentJobTab()
		elif event.type() == JobsUpdateEvent.EVENT_TYPE:
			self.__UpdateCurrentJobTab()
			self.__UpdateQueuedJobsTab()
		elif event.type() == NodesUpdateEvent.EVENT_TYPE:
			self.__UpdateNodesTab()

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
