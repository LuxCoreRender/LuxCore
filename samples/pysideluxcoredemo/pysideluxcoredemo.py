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
from array import *

import pyluxcore
from PySide.QtCore import *
from PySide.QtGui import *

class RenderView(QMainWindow):
	def __init__(self, cfgFileName):
		super(RenderView, self).__init__()
		
		self.dofEnabled = True
		self.luxBallShapeIsCube = False
		
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
		self.imageBuffer = array('B', [0] * (self.filmWidth * self.filmHeight * 4))

		# Read the configuration and start the rendering
		self.scene = pyluxcore.Scene(props.Get("scene.file").GetString(),
			props.Get("images.scale", [1.0]).GetFloat())
		sceneProps = self.scene.GetProperties()
		# Save Camera position
		self.cameraPos = sceneProps.Get("scene.camera.lookat.orig").GetFloats()
		self.luxBallPos = [0.0, 0.0, 0.0]
		# Create the rendering configuration
		self.config = pyluxcore.RenderConfig(props, self.scene)
		# Create the rendering session
		self.session = pyluxcore.RenderSession(self.config)
		# Start the rendering
		self.session.Start()
		
		self.timer = QBasicTimer()
		self.timer.start(500, self)

	def createActions(self):
		self.quitAct = QAction("&Quit", self, triggered = self.close)
		self.quitAct.setShortcuts(QKeySequence.Quit)
		self.saveImageAct = QAction("&Save image", self, triggered = self.saveImage)
		
		self.cameraToggleDOFAct = QAction("Togle &DOF", self, triggered = self.cameraToggleDOF)
		self.cameraMoveLeftAct = QAction("Move &left", self, triggered = lambda: self.cameraMove(-0.5))
		self.cameraMoveLeftAct.setShortcuts(QKeySequence.MoveToPreviousChar)
		self.cameraMoveRightAct = QAction("Move &right", self, triggered = lambda: self.cameraMove(0.5))
		self.cameraMoveRightAct.setShortcuts(QKeySequence.MoveToNextChar)
		
		self.luxBallMatMirrorAct = QAction("&Mirror", self, triggered = self.luxBallMatMirror)
		self.luxBallMatMatteAct = QAction("M&atte", self, triggered = self.luxBallMatMatte)
		self.luxBallMatGlassAct = QAction("&Glass", self, triggered = self.luxBallMatGlass)
		self.luxBallMatGlossyImageMapAct = QAction("G&lossy with image map", self, triggered = self.luxBallMatGlossyImageMap)

		self.luxBallMoveLeftAct = QAction("Move &left", self, triggered = lambda: self.luxBallMove(-0.2))
		self.luxBallMoveLeftAct.setShortcuts([QKeySequence(Qt.CTRL + Qt.Key_Left)])
		self.luxBallMoveRightAct = QAction("Move &right", self, triggered = lambda: self.luxBallMove(0.2))
		self.luxBallMoveRightAct.setShortcuts([QKeySequence(Qt.CTRL + Qt.Key_Right)])
		
		self.luxBallShapeToggleAct = QAction("Toggle S&hell", self, triggered = self.luxBallShapeToggle)
	
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
		luxBallMatMenu.addAction(self.luxBallMatGlossyImageMapAct)
		
		luxBallMatMenu = QMenu("&LuxBall Position", self)
		luxBallMatMenu.addAction(self.luxBallMoveLeftAct)
		luxBallMatMenu.addAction(self.luxBallMoveRightAct)
		
		luxBallShapeMenu = QMenu("&LuxBall Shape", self)
		luxBallShapeMenu.addAction(self.luxBallShapeToggleAct)
		
		self.menuBar().addMenu(fileMenu)
		self.menuBar().addMenu(cameraMenu)
		self.menuBar().addMenu(luxBallMatMenu)
		self.menuBar().addMenu(luxBallShapeMenu)
	
	def center(self):
		screen = QDesktopWidget().screenGeometry()
		size =  self.geometry()
		self.move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2)
	
	def saveImage(self):
		# Save the rendered image
		self.session.SaveFilm()
		print("Image saved")
	
	def cameraToggleDOF(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.dofEnabled = not self.dofEnabled
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.camera").
			Set(pyluxcore.Property("scene.camera.lensradius", [0.015 if self.dofEnabled else 0.0])))

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera DOF toggled: %s" % (str(self.dofEnabled)))
	
	def cameraMove(self, t):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.cameraPos[0] += t
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.camera").
			Set(pyluxcore.Property("scene.camera.lookat.orig", self.cameraPos)))

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera new position: %f, %f, %f" % (self.cameraPos[0], self.cameraPos[1], self.cameraPos[2]))
	
	def luxBallMatMirror(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.materials.shell.type", ["mirror"])).
			Set(pyluxcore.Property("scene.materials.shell.kr", [0.75, 0.75, 0.75])))
		# To remove unreferenced constant textures defined implicitely
		self.scene.RemoveUnusedTextures()
		# To remove all unreferenced image maps (note: the order of call does matter)
		self.scene.RemoveUnusedImageMaps()
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Mirror")
	
	def luxBallMatMatte(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.materials.shell.type", ["matte"])).
			Set(pyluxcore.Property("scene.materials.shell.kd", [0.75, 0.0, 0.0])))
		
		# To remove unreferenced constant textures defined implicitely
		self.scene.RemoveUnusedTextures()
		# To remove all unreferenced image maps (note: the order of call does matter)
		self.scene.RemoveUnusedImageMaps()
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Matte")
	
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
		
		# To remove unreferenced constant textures defined implicitely
		self.scene.RemoveUnusedTextures()
		# To remove all unreferenced image maps (note: the order of call does matter)
		self.scene.RemoveUnusedImageMaps()
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Matte")

	def luxBallMatGlossyImageMap(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.textures.tex.type", ["imagemap"])).
			Set(pyluxcore.Property("scene.textures.tex.file", ["scenes/bump/map.png"])).
			Set(pyluxcore.Property("scene.textures.tex.gain", [0.6])).
			Set(pyluxcore.Property("scene.textures.tex.mapping.uvscale", [16, -16])).
			Set(pyluxcore.Property("scene.materials.shell.type", ["glossy2"])).
			Set(pyluxcore.Property("scene.materials.shell.kd", ["tex"])).
			Set(pyluxcore.Property("scene.materials.shell.ks", [0.25, 0.0, 0.0])).
			Set(pyluxcore.Property("scene.materials.shell.uroughness", [0.05])).
			Set(pyluxcore.Property("scene.materials.shell.vroughness", [0.05])))

		# To remove unreferenced constant textures defined implicitely
		self.scene.RemoveUnusedTextures()
		# To remove all unreferenced image maps (note: the order of call does matter)
		self.scene.RemoveUnusedImageMaps()
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Matte")
	
	def luxBallMove(self, t):
		# Begin scene editing
		self.session.BeginSceneEdit()
		
		self.luxBallPos[0] += t
		# Set the new LuxBall position (note: using the transpose matrix)
		mat = [1.0, 0.0, 0.0, self.luxBallPos[0],
			0.0, 1.0, 0.0, self.luxBallPos[1],
			0.0, 0.0, 1.0, self.luxBallPos[2],
			0.0, 0.0, 0.0, 1.0]
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.objects.luxtext").
			Set(pyluxcore.Property("scene.objects.luxtext.transformation", mat)))
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.objects.luxinner").
			Set(pyluxcore.Property("scene.objects.luxinner.transformation", mat)))
		self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.objects.luxshell").
			Set(pyluxcore.Property("scene.objects.luxshell.transformation", mat)))
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall new position: %f, %f, %f" % (self.luxBallPos[0], self.luxBallPos[1], self.luxBallPos[2]))
		
	def luxBallShapeToggle(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the LuxBall shape
		if self.luxBallShapeIsCube:
			self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.objects.luxshell").
				Set(pyluxcore.Property("scene.objects.luxshell.ply", ["scenes/luxball/luxball-shell.ply"])).
				Set(pyluxcore.Property("scene.objects.luxshell.useplynormals", [False])))
			self.luxBallShapeIsCube = False
		else:
			self.scene.DefineMesh("LuxCubeMesh", [
				# Bottom face
				(-0.405577, -0.343839, 0.14),
				(-0.405577, 0.506553, 0.14),
				(0.443491, 0.506553, 0.14),
				(0.443491, -0.343839, 0.14),
				# Top face
				(-0.405577, -0.343839, 0.819073),
				(0.443491, -0.343839, 0.819073),
				(0.443491, 0.506553, 0.819073),
				(-0.405577, 0.506553, 0.819073),
				# Side left
				(-0.405577, -0.343839, 0.14),
				(-0.405577, -0.343839, 0.819073),
				(-0.405577, 0.506553, 0.819073),
				(-0.405577, 0.506553, 0.14),
				# Side right
				(0.443491, -0.343839, 0.14),
				(0.443491, 0.506553, 0.14),
				(0.443491, 0.506553, 0.819073),
				(0.443491, -0.343839, 0.819073),
				# Side back
				(-0.405577, -0.343839, 0.14),
				(0.443491, -0.343839, 0.14),
				(0.443491, -0.343839, 0.819073),
				(-0.405577, -0.343839, 0.819073),
				# Side front
				(-0.405577, 0.506553, 0.14),
				(-0.405577, 0.506553, 0.819073),
				(0.443491, 0.506553, 0.819073),
				(0.443491, 0.506553, 0.14)], [
				# Bottom face
				(0, 1, 2), (2, 3, 0),
				# Top face
				(4, 5, 6), (6, 7, 4),
				# Side left
				(8, 9, 10), (10, 11, 8),
				# Side right
				(12, 13, 14), (14, 15, 12),
				# Side back
				(16, 17, 18), (18, 19, 16),
				# Side back
				(20, 21, 22), (22, 23, 20)
				], None, [
				# Bottom face
				(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0),
				# Top face
				(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0),
				# Side left
				(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0),
				# Side right
				(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0),
				# Side back
				(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0),
				# Side front
				(0.0, 0.0),(1.0, 0.0), (1.0, 1.0),	(0.0, 1.0)
				], None, None)
			self.scene.Parse(self.scene.GetProperties().GetAllProperties("scene.objects.luxshell").
				Set(pyluxcore.Property("scene.objects.luxshell.ply", ["LuxCubeMesh"])).
				Set(pyluxcore.Property("scene.objects.luxshell.useplynormals", [True])))
			self.luxBallShapeIsCube = True
			

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera new position: %f, %f, %f" % (self.cameraPos[0], self.cameraPos[1], self.cameraPos[2]))
	
	def timerEvent(self, event):
		if event.timerId() == self.timer.timerId():
			# Print some information about the rendering progress
			
			# Update statistics
			self.session.UpdateStats()
			
			stats = self.session.GetStats()
			print("[Elapsed time: %3.1fsec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
			
			# Update the image
			self.session.GetScreenBuffer(self.imageBuffer)
			
			self.update()
		else:
			QFrame.timerEvent(self, event)
	
	def paintEvent(self, event):
		painter = QPainter(self)
		image = QImage(self.imageBuffer, self.filmWidth, self.filmHeight, QImage.Format_RGB32)
		painter.drawImage(QPoint(0, 0), image)
	
	def resizeEvent(self, event):
		# Stop the rendering
		self.session.Stop()
		self.session = None
		
		# Set the new size
		self.filmWidth = int(event.size().width())
		self.filmHeight = int(event.size().height())
		self.config.Parse(
			pyluxcore.Properties().
			Set(pyluxcore.Property("film.width", [self.filmWidth])).
			Set(pyluxcore.Property("film.height", [self.filmHeight])))
		self.imageBuffer = array('B', [0] * (self.filmWidth * self.filmHeight * 4))
		
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
	pyluxcore.Init()
	print("LuxCore %s" % pyluxcore.version())
	
	app = QApplication(sys.argv)
	rv = RenderView("scenes/luxball/luxball-hdr.cfg")
	rv.show()
	sys.exit(app.exec_())

if __name__ == '__main__':
	main()
