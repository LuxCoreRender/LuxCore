#!/usr/bin/python
# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2018 by authors (see AUTHORS.txt)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

# This is a replacement for old SLG telnet interface.

import sys
sys.path.append("./lib")
import getopt
import errno
from code import InteractiveConsole
from array import *
import socket
from time import gmtime, strftime

import pyluxcore
from PySide.QtCore import *
from PySide.QtGui import *

################################################################################
#
# LuxCore Telnet Server
#
################################################################################

class RenderView(QMainWindow):
	def __init__(self, session):
		super(RenderView, self).__init__()
		
		self.createActions()
		self.createMenus()
		
		self.session = session
		self.config = session.GetRenderConfig()
		self.filmWidth, self.filmHeight = self.config.GetFilmSize()[:2]
	
		# Allocate the image for the rendering
		self.allocateImageBuffers()

		self.setGeometry(0, 0, self.filmWidth, self.filmHeight)
		self.setWindowTitle('LuxCore RenderView')
		self.center()
		
		self.timer = QBasicTimer()
		self.timer.start(500, self)
	
	def allocateImageBuffers(self):
		########################################################################
		# NOTICE THE DIFFERENT BEHAVIOR REQUIRED BY PYTHON 2.7
		########################################################################
		if sys.version_info<(3,0,0):
			self.imageBufferFloat = buffer(array('f', [0.0] * (self.filmWidth * self.filmHeight * 3)))
			self.imageBufferUChar = buffer(array('b', [0] * (self.filmWidth * self.filmHeight * 4)))
		else:
			self.imageBufferFloat = array('f', [0.0] * (self.filmWidth * self.filmHeight * 3))
			self.imageBufferUChar = array('b', [0] * (self.filmWidth * self.filmHeight * 4))

	def createActions(self):
		self.quitAct = QAction("&Quit", self, triggered = self.close)
		self.quitAct.setShortcuts(QKeySequence.Quit)
		self.saveImageAct = QAction("&Save image", self, triggered = self.saveImage)
	
	def createMenus(self):
		fileMenu = QMenu("&File", self)
		fileMenu.addAction(self.saveImageAct)
		fileMenu.addAction(self.quitAct)
		
		self.menuBar().addMenu(fileMenu)
	
	def center(self):
		screen = QDesktopWidget().screenGeometry()
		size =  self.geometry()
		self.move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2)
	
	def saveImage(self):
		# Save the rendered image
		self.session.GetFilm().Save()

	def timerEvent(self, event):
		if event.timerId() == self.timer.timerId():
			# Print some information about the rendering progress
			
			# Update statistics
			self.session.UpdateStats()
			
			stats = self.session.GetStats()
			LogHandler("[Elapsed time: %3.1fsec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
			
			# Update the image
			self.session.GetFilm().GetOutputFloat(pyluxcore.FilmOutputType.RGB_TONEMAPPED, self.imageBufferFloat)
			pyluxcore.ConvertFilmChannelOutput_3xFloat_To_4xUChar(self.filmWidth, self.filmHeight, self.imageBufferFloat, self.imageBufferUChar, False)
			
			self.update()
		else:
			QFrame.timerEvent(self, event)
	
	def paintEvent(self, event):
		painter = QPainter(self)
		image = QImage(self.imageBufferUChar, self.filmWidth, self.filmHeight, QImage.Format_RGB32)
		painter.drawImage(QPoint(0, 0), image)
	
	def resizeEvent(self, event):
		if (event.size().width() != self.filmWidth) or (event.size().height() != self.filmHeight):
			# Stop the rendering
			self.session.Stop()
			self.session = None

			# Set the new size
			self.filmWidth = int(event.size().width())
			self.filmHeight = int(event.size().height())
			self.config.Parse(pyluxcore.Properties().
				Set(pyluxcore.Property("film.width", [self.filmWidth])).
				Set(pyluxcore.Property("film.height", [self.filmHeight])))
			self.allocateImageBuffers()

			# Re-start the rendering
			self.session = pyluxcore.RenderSession(self.config)
			self.session.Start()
		
		super(RenderView, self).resizeEvent(event)
	
	def closeEvent(self, event):
		self.timer.stop()
		self.session.Stop()
		self.session = None
		self.config = None
		self.scene = None
		event.accept()

def LogHandler(msg):
	print("[%s]%s" % (strftime("%Y-%m-%d %H:%M:%S", gmtime()), msg))

################################################################################
#
# LuxCore Telnet Server
#
################################################################################

try:
	sys.ps1
except AttributeError:
	sys.ps1 = '>>> '
try:
	sys.ps2
except AttributeError:
	sys.ps2 = '... '

class FileProxy(object):
	def __init__(self, f, logOutput):
		self.f = f
		self.logOutput = logOutput
	
	def isatty(self): 
		return True
	
	def flush(self):
		pass
	
	def write(self, *a, **kw):
		self.f.write(*a, **kw)
		self.f.flush()
		if self.logOutput:
			self.logOutput.write(*a, **kw)
			self.logOutput.flush()
	
	def readline(self, *a):
		line = self.f.readline(*a)
		if self.logOutput:
			self.logOutput.write(line)
			self.logOutput.flush()
		return line.replace('\r\n', '\n')
	
	def __getattr__(self, attr):
		return getattr(self.f, attr)

class SocketConsole:
	def __init__(self, connFile, host, port, logOutput):
		self.host = host
		self.port = port
		self.outputProxy = FileProxy(connFile, logOutput)
	
	def run(self):
		try:
			vars = globals().copy()
			vars.update(locals())
			console = InteractiveConsole(vars)
			console.interact()
		finally:
			self.switchOut()
			self.finalize()
	
	def switch(self):
		self.saved = sys.stdin, sys.stderr, sys.stdout
		sys.stdin = sys.stdout = sys.stderr = self.outputProxy
		
	def switchOut(self):
		sys.stdin, sys.stderr, sys.stdout = self.saved

	def finalize(self):
		self.desc = None
		print("Connection closed: %s:%d" % (self.host, self.port))

class LuxCoreTelnetServer:
	def __init__(self, host, port, logOutput):
		self.host = host
		self.port = port
		self.logOutput = logOutput
		
		self.telnetSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.telnetSocket.bind((self.host, self.port))
		self.telnetSocket.listen(0)

	def handleConnection(self):
		try:
			try:
				while True:
					conn, addr = self.telnetSocket.accept()
					host, port = addr
					print("Connection from: %s:%d" % (host, port))
					
					connFile = conn.makefile("rw")
					console = SocketConsole(connFile, host, port, self.logOutput)
					console.switch()
					console.run()
			except socket.error as e:
				# Broken pipe means it was shutdown
				if e.errno != errno.EPIPE:
					raise
		finally:
			self.telnetSocket.close()

def main(argv):
	pyluxcore.Init(LogHandler)
	print("LuxCore %s" % pyluxcore.Version())
	
	# Get the command line options
	try:
		opts, args = getopt.getopt(argv[1:], "hH:p:q", ["help", "host=", "port=", "quiet"])
	except getopt.GetoptError:
		usage()
		sys.exit(-1)
	
	host = "localhost"
	port = 18081
	logOutput = sys.stdout
	for opt, arg in opts:
		if opt == '-h':
			usage()
			sys.exit()
		if opt in ('-H', '--host'):
			host = arg
		if opt in ('-p', '--port'):
			port = int(arg)
		if opt in ('-q', '--quiet'):
			logOutput = None
	print("Hostname: %s Port: %d" % (host, port))
	
	global qapp
	qapp = QApplication(sys.argv)
	
	while True:
		server = LuxCoreTelnetServer(host, port, logOutput)
		server.handleConnection()
	
	sys.exit(app.exec_())

if __name__ == '__main__':
	main(sys.argv)
