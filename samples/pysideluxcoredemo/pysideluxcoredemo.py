#!/usr/bin/python
# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
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
sys.path.append("./lib")

import pyluxcore
from PySide.QtCore import *
from PySide.QtGui import *

class RenderView(QMainWindow):
	def __init__(self, cfgFileName):
		super(RenderView, self).__init__()
		
		self.dofEnabled = True
		
		self.createActions()
		self.createMenus()
		
		# Load the configuration from file
		props = pyluxcore.Properties(cfgFileName)
		
		# Change the render engine to PATHCPU
		props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

		self.filmWidth = props.Get("film.width").GetInt()
		self.filmHeight = props.Get("film.height").GetInt()
		self.setGeometry(0, 0, self.filmWidth, self.filmHeight)
		self.setWindowTitle('LuxCore RenderView')
		self.center()
		
		# Allocate the image for the rendering
		self.imageBuffer = bytearray(self.filmWidth * self.filmHeight * 3)

		# Read the configuration and start the rendering
		self.scene = pyluxcore.Scene(props.Get("scene.file").GetString(),
			props.Get("images.scale", [1.0]).GetFloat())
		sceneProps = self.scene.GetProperties()
		# Save Camera position
		self.cameraPos = [
			sceneProps.Get("scene.camera.lookat.orig").GetValueFloat(0),
			sceneProps.Get("scene.camera.lookat.orig").GetValueFloat(1),
			sceneProps.Get("scene.camera.lookat.orig").GetValueFloat(2)]
		self.config = pyluxcore.RenderConfig(props, self.scene)
		self.session = pyluxcore.RenderSession(self.config)
		self.session.Start()
		
		self.timer = QBasicTimer()
		self.timer.start(500, self)

	def createActions(self):
		self.quitAct = QAction("&Quit", self, triggered = self.close)
		self.quitAct.setShortcuts(QKeySequence.Quit)
		self.saveImageAct = QAction("&Save image", self, triggered = self.saveImage)
		
		self.cameraToggleDOFAct = QAction("Togle &DOF", self, triggered = self.cameraToggleDOF)
		self.cameraMoveLeftAct = QAction("Move &left", self, triggered = self.cameraMoveLeft)
		self.cameraMoveLeftAct.setShortcuts(QKeySequence.MoveToPreviousChar)
		self.cameraMoveRightAct = QAction("Move &right", self, triggered = self.cameraMoveRight)
		self.cameraMoveRightAct.setShortcuts(QKeySequence.MoveToNextChar)
		
		self.luxBallMatMirrorAct = QAction("&Mirror", self, triggered = self.luxBallMatMirror)
		self.luxBallMatMatteAct = QAction("M&atte", self, triggered = self.luxBallMatMatte)
		self.luxBallMatGlassAct = QAction("&Glass", self, triggered = self.luxBallMatGlass)
	
	def createMenus(self):
		fileMenu = QMenu("&File", self)
		fileMenu.addAction(self.saveImageAct)
		fileMenu.addAction(self.quitAct)
		
		cameraMenu = QMenu("&Camera", self)
		cameraMenu.addAction(self.cameraToggleDOFAct)
		cameraMenu.addAction(self.cameraMoveLeftAct)
		cameraMenu.addAction(self.cameraMoveRightAct)

		luxBallMatMenu = QMenu("&LuxBall Material", self)
		luxBallMatMenu.addAction(self.luxBallMatMirrorAct)
		luxBallMatMenu.addAction(self.luxBallMatMatteAct)
		luxBallMatMenu.addAction(self.luxBallMatGlassAct)
		
		self.menuBar().addMenu(fileMenu)
		self.menuBar().addMenu(cameraMenu)
		self.menuBar().addMenu(luxBallMatMenu)
	
	def center(self):
		screen = QDesktopWidget().screenGeometry()
		size =  self.geometry()
		self.move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2)
	
	def saveImage(self):
		# Save the rendered image
		self.session.SaveFilm()
		print("Image saved");
	
	def cameraToggleDOF(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.dofEnabled = not self.dofEnabled;
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.camera").
			Set(pyluxcore.Property("scene.camera.lensradius", [0.015 if self.dofEnabled else 0.0])))

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera DOF toggled: %s" % (str(self.dofEnabled)))
	
	def cameraMoveLeft(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.cameraPos[0] -= 0.5
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.camera").
			Set(pyluxcore.Property("scene.camera.lookat.orig", self.cameraPos)))

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera new position: %f, %f, %f" % (self.cameraPos[0], self.cameraPos[2], self.cameraPos[2]));
	
	def cameraMoveRight(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.cameraPos[0] += 0.5
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.camera").
			Set(pyluxcore.Property("scene.camera.lookat.orig", self.cameraPos)))

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera new position: %f, %f, %f" % (self.cameraPos[0], self.cameraPos[2], self.cameraPos[2]));
	
	def luxBallMatMirror(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.materials.shell.type", ["mirror"])).
			Set(pyluxcore.Property("scene.materials.shell.kr", [0.75, 0.75, 0.75])))

		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Mirror");
	
	def luxBallMatMatte(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.materials.shell.type", ["matte"])).
			Set(pyluxcore.Property("scene.materials.shell.kd", [0.75, 0.0, 0.0])))

		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Matte");
	
	def luxBallMatGlass(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.materials.shell.type", ["glass"])).
			Set(pyluxcore.Property("scene.materials.shell.kr", [0.69, 0.78, 1.0])).
			Set(pyluxcore.Property("scene.materials.shell.kt", [0.69, 0.78, 1.0])).
			Set(pyluxcore.Property("scene.materials.shell.ioroutside", [1.0])).
			Set(pyluxcore.Property("scene.materials.shell.iorinside", [1.45]))
			)

		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Matte");
		
	def timerEvent(self, event):
		if event.timerId() == self.timer.timerId():
			# Print some information about the rendering progress
			
			# Update statistics
			self.session.UpdateStats()
			
			stats = self.session.GetStats();
			print("[Elapsed time: %3.1fsec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
			
			# Update the image
			self.session.GetScreenBuffer(self.imageBuffer);
			
			self.update()
		else:
			QFrame.timerEvent(self, event)
	
	def paintEvent(self, event):
		painter = QPainter(self)
		image = QImage(self.imageBuffer, self.filmWidth, self.filmHeight, QImage.Format_RGB888)
		painter.drawImage(QPoint(0, 0), image)
	
	def resizeEvent(self, event):
		# Stop the rendering
		self.session.Stop()
		self.session = None
		
		# Set the new size
		self.filmWidth = event.size().width()
		self.filmHeight = event.size().height()
		self.config.Parse(
			pyluxcore.Properties().
			Set(pyluxcore.Property("film.width", [self.filmWidth])).
			Set(pyluxcore.Property("film.height", [self.filmHeight])))
		self.imageBuffer = bytearray(self.filmWidth * self.filmHeight * 3)
		
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

def main():   
	print("LuxCore %s" % pyluxcore.version())
	
	app = QApplication(sys.argv)
	rv = RenderView("scenes/luxball/luxball-hdr.cfg")
	rv.show()
	sys.exit(app.exec_())

if __name__ == '__main__':
	main()
