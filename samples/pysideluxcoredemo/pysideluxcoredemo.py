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
sys.path.append("./lib")
from array import *
from time import localtime, strftime
from functools import partial

import pyluxcore
try:
	from PySide.QtCore import *
	from PySide.QtGui import *
	PYSIDE2 = False
except ImportError:
	from PySide2.QtCore import *
	from PySide2.QtGui import *
	from PySide2.QtWidgets import *
	PYSIDE2 = True

class RenderView(QMainWindow):
	def __init__(self):
		super(RenderView, self).__init__()
		
		self.dofEnabled = True
		self.luxBallShapeIsCube = False
		self.selectedFilmChannel = pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE
		
		self.createActions()
		self.createMenus()
		
		# Load the configuration from file
		props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")
		
		# Change the render engine to PATHCPU
		props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

		# Read the configuration and start the rendering
		self.scene = pyluxcore.Scene(props.Get("scene.file").GetString())
		sceneProps = self.scene.ToProperties()
		# Save Camera position
		self.cameraPos = sceneProps.Get("scene.camera.lookat.orig").GetFloats()
		
		# Create the rendering configuration
		self.config = pyluxcore.RenderConfig(props, self.scene)
		self.filmWidth, self.filmHeight = self.config.GetFilmSize()[:2]
	
		# Allocate the image for the rendering
		self.allocateImageBuffers()

		self.setGeometry(0, 0, self.filmWidth, self.filmHeight)
		self.setWindowTitle('LuxCore RenderView')
		self.center()
		
		# Create the rendering session
		self.session = pyluxcore.RenderSession(self.config)
		# Start the rendering
		self.session.Start()
		
		self.timer = QBasicTimer()
		self.timer.start(500, self)
	
	def allocateImageBuffers(self):
		########################################################################
		# NOTICE THE DIFFERENT BEHAVIOR REQUIRED BY PYTHON 2.7
		########################################################################
		if sys.version_info < (3,0,0):
			self.imageBufferFloat = buffer(array('f', [0.0] * (self.filmWidth * self.filmHeight * 3)))
			self.imageBufferUChar = buffer(array('b', [0] * (self.filmWidth * self.filmHeight * 4)))
		else:
			self.imageBufferFloat = array('f', [0.0] * (self.filmWidth * self.filmHeight * 3))
			self.imageBufferUChar = array('b', [0] * (self.filmWidth * self.filmHeight * 4))

	def createActions(self):
		self.quitAct = QAction("&Quit", self, triggered = self.close)
		self.quitAct.setShortcuts(QKeySequence.Quit)
		self.saveImageAct = QAction("&Save image", self, triggered = self.saveImage)
		
		# RenderEngine type
		self.renderEnginePathCPUAct = QAction("Path&CPU", self, triggered = self.renderEnginePathCPU)
		# Get the list of all OpenCL devices available
		self.deviceList = pyluxcore.GetOpenCLDeviceList()
		self.renderEnginePathOCLActs = []
		for i in range(len(self.deviceList)):
			self.renderEnginePathOCLActs.append(
				QAction(self.deviceList[i][0], self, triggered = partial(self.renderEnginePathOCL, i)))
		
		self.cameraToggleDOFAct = QAction("Toggle &DOF", self, triggered = self.cameraToggleDOF)
		self.cameraMoveLeftAct = QAction("Move &left", self, triggered = partial(self.cameraMove, -0.5))
		self.cameraMoveLeftAct.setShortcuts(QKeySequence.MoveToPreviousChar)
		self.cameraMoveRightAct = QAction("Move &right", self, triggered = partial(self.cameraMove, 0.5))
		self.cameraMoveRightAct.setShortcuts(QKeySequence.MoveToNextChar)
		
		self.luxBallMatMirrorAct = QAction("&Mirror", self, triggered = self.luxBallMatMirror)
		self.luxBallMatMatteAct = QAction("M&atte", self, triggered = self.luxBallMatMatte)
		self.luxBallMatGlassAct = QAction("&Glass", self, triggered = self.luxBallMatGlass)
		self.luxBallMatGlossyImageMapAct = QAction("G&lossy with image map", self, triggered = self.luxBallMatGlossyImageMap)

		self.luxBallMoveLeftAct = QAction("Move &left", self, triggered = partial(self.luxBallMove, -0.1))
		self.luxBallMoveLeftAct.setShortcuts([QKeySequence(Qt.CTRL + Qt.Key_Left)])
		self.luxBallMoveRightAct = QAction("Move &right", self, triggered = partial(self.luxBallMove, 0.1))
		self.luxBallMoveRightAct.setShortcuts([QKeySequence(Qt.CTRL + Qt.Key_Right)])
		
		self.luxBallShapeToggleAct = QAction("Toggle S&hell", self, triggered = self.luxBallShapeToggle)

		self.filmSetOutputChannel_RGB_IMAGEPIPELINE_Act = QAction("&RGB TONEMAPPED output channel", self,
			triggered = partial(self.filmSetOutputChannel, pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE))
		self.filmSetOutputChannel_DIRECT_DIFFUSE_Act = QAction("&DIRECT DIFFUSE output channel", self,
			triggered = partial(self.filmSetOutputChannel, pyluxcore.FilmOutputType.DIRECT_DIFFUSE))
		self.filmSetOutputChannel_INDIRECT_SPECULAR_Act = QAction("&INDIRECT SPECULAR output channel", self,
			triggered = partial(self.filmSetOutputChannel, pyluxcore.FilmOutputType.INDIRECT_SPECULAR))
		self.filmSetOutputChannel_EMISSION_Act = QAction("&EMISSION output channel", self,
			triggered = partial(self.filmSetOutputChannel, pyluxcore.FilmOutputType.EMISSION))
	
	def createMenus(self):
		fileMenu = QMenu("&File", self)
		fileMenu.addAction(self.saveImageAct)
		fileMenu.addAction(self.quitAct)
		
		renderEngineMenu = QMenu("&Render Engine", self)
		renderEngineMenu.addAction(self.renderEnginePathCPUAct)
		renderEnginePathOCLMenu = QMenu("Path&GPU", self)
		renderEngineMenu.addMenu(renderEnginePathOCLMenu);
		# Add an entry for each OpenCL device
		for i in range(len(self.deviceList)):
			renderEnginePathOCLMenu.addAction(self.renderEnginePathOCLActs[i])

		cameraMenu = QMenu("&Camera", self)
		cameraMenu.addAction(self.cameraToggleDOFAct)
		cameraMenu.addAction(self.cameraMoveLeftAct)
		cameraMenu.addAction(self.cameraMoveRightAct)

		luxBallMatMenu = QMenu("&LuxBall Material", self)
		luxBallMatMenu.addAction(self.luxBallMatMirrorAct)
		luxBallMatMenu.addAction(self.luxBallMatMatteAct)
		luxBallMatMenu.addAction(self.luxBallMatGlassAct)
		luxBallMatMenu.addAction(self.luxBallMatGlossyImageMapAct)
		
		luxBallPosMenu = QMenu("&LuxBall Position", self)
		luxBallPosMenu.addAction(self.luxBallMoveLeftAct)
		luxBallPosMenu.addAction(self.luxBallMoveRightAct)
		
		luxBallShapeMenu = QMenu("&LuxBall Shape", self)
		luxBallShapeMenu.addAction(self.luxBallShapeToggleAct)
		
		filmMenu = QMenu("Film", self)
		filmMenu.addAction(self.filmSetOutputChannel_RGB_IMAGEPIPELINE_Act)
		filmMenu.addAction(self.filmSetOutputChannel_DIRECT_DIFFUSE_Act)
		filmMenu.addAction(self.filmSetOutputChannel_INDIRECT_SPECULAR_Act)
		filmMenu.addAction(self.filmSetOutputChannel_EMISSION_Act)
		
		self.menuBar().addMenu(fileMenu)
		self.menuBar().addMenu(renderEngineMenu)
		self.menuBar().addMenu(cameraMenu)
		self.menuBar().addMenu(luxBallMatMenu)
		self.menuBar().addMenu(luxBallPosMenu)
		self.menuBar().addMenu(luxBallShapeMenu)
		self.menuBar().addMenu(filmMenu)
	
	def center(self):
		screen = QDesktopWidget().screenGeometry()
		size =  self.geometry()
		if not PYSIDE2:
			self.move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2)
	
	def saveImage(self):
		# Save the rendered image
		self.session.GetFilm().Save()
		print("Image saved")
	
	def renderEnginePathCPU(self):
		# Stop the rendering
		self.session.Stop()
		self.session = None
		
		# Change the render engine to PATHCPU
		props = self.config.GetProperties()
		props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))
		
		# Create the new RenderConfig
		self.config = pyluxcore.RenderConfig(props, self.scene)
		
		# Re-start the rendering
		self.session = pyluxcore.RenderSession(self.config)
		self.session.Start()
		print("PathCPU selected")
	
	def renderEnginePathOCL(self, index):
		# Stop the rendering
		self.session.Stop()
		self.session = None
		
		# Change the render engine to PATHCPU
		props = self.config.GetProperties()
		selectString = list("0" * len(self.deviceList))
		selectString[index] = "1"
		props.Set(pyluxcore.Property("renderengine.type", ["PATHOCL"])). \
			Set(pyluxcore.Property("opencl.devices.select", ["".join(selectString)]))
		
		# Create the new RenderConfig
		self.config = pyluxcore.RenderConfig(props, self.scene)
		
		# Re-start the rendering
		self.session = pyluxcore.RenderSession(self.config)
		self.session.Start()
		print("PathOCL selected: %s" % self.deviceList[index][0])
	
	def cameraToggleDOF(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.dofEnabled = not self.dofEnabled
		self.scene.Parse(self.scene.ToProperties().GetAllProperties("scene.camera").
			Set(pyluxcore.Property("scene.camera.lensradius", [0.015 if self.dofEnabled else 0.0])))

		# End scene editing
		self.session.EndSceneEdit()
		print("Camera DOF toggled: %s" % (str(self.dofEnabled)))
	
	def cameraMove(self, t):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Edit the camera
		self.scene.GetCamera().TranslateRight(t);

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
		# To remove unreferenced constant textures defined implicitly
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
		
		# To remove unreferenced constant textures defined implicitly
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
		
		# To remove unreferenced constant textures defined implicitly
		self.scene.RemoveUnusedTextures()
		# To remove all unreferenced image maps (note: the order of call does matter)
		self.scene.RemoveUnusedImageMaps()
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Glass")

	def luxBallMatGlossyImageMap(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# Define check map
		imageMap = array('f', [0.0] * (128 * 128 * 3))
		for y in range(128):
			for x in range(128):
				offset = (x + y * 128) * 3
				if (x % 64 < 32) ^ (y % 64 < 32):
					imageMap[offset] = 1.0
					imageMap[offset] = 1.0
					imageMap[offset] = 1.0
				else:
					imageMap[offset] = 1.0
					imageMap[offset] = 0.0
					imageMap[offset] = 0.0
		########################################################################
		# NOTICE THE DIFFERENT BEHAVIOR REQUIRED BY PYTHON 2.7
		########################################################################
		self.scene.DefineImageMap("check_map", buffer(imageMap) if sys.version_info < (3,0,0) else imageMap, 2.2, 3, 128, 128)

		# Edit the material
		self.scene.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("scene.textures.tex.type", ["imagemap"])).
			Set(pyluxcore.Property("scene.textures.tex.file", ["check_map"])).
			Set(pyluxcore.Property("scene.textures.tex.gain", [0.6])).
			Set(pyluxcore.Property("scene.textures.tex.mapping.uvscale", [16, -16])).
			Set(pyluxcore.Property("scene.materials.shell.type", ["glossy2"])).
			Set(pyluxcore.Property("scene.materials.shell.kd", ["tex"])).
			Set(pyluxcore.Property("scene.materials.shell.ks", [0.25, 0.0, 0.0])).
			Set(pyluxcore.Property("scene.materials.shell.uroughness", [0.05])).
			Set(pyluxcore.Property("scene.materials.shell.vroughness", [0.05])))

		# To remove unreferenced constant textures defined implicitly
		self.scene.RemoveUnusedTextures()
		# To remove all unreferenced image maps (note: the order of call does matter)
		self.scene.RemoveUnusedImageMaps()
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall material set to: Glossy with image map")
	
	def luxBallMove(self, t):
		# Begin scene editing
		self.session.BeginSceneEdit()
		
		# Set the new LuxBall position
		mat = [1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			t, 0.0, 0.0, 1.0]

		self.scene.UpdateObjectTransformation("luxtext", mat)
		self.scene.UpdateObjectTransformation("luxinner", mat)
		self.scene.UpdateObjectTransformation("luxshell", mat)
		
		# End scene editing
		self.session.EndSceneEdit()
		print("LuxBall moved")
		
	def luxBallShapeToggle(self):
		# Begin scene editing
		self.session.BeginSceneEdit()

		# I delete and re-create the LuxBall to reset objects position in
		# case they have been moved

		# Delete the LuxBall
		self.scene.DeleteObject("luxtext")
		self.scene.DeleteObject("luxinner")
		self.scene.DeleteObject("luxshell")
		self.scene.RemoveUnusedMeshes()

		# Re-create the LuxBall inner and text
		sceneProps = pyluxcore.Properties("scenes/luxball/luxball-hdr.scn")
		self.scene.Parse(sceneProps.GetAllProperties("scene.objects.luxtext"))
		self.scene.Parse(sceneProps.GetAllProperties("scene.objects.luxinner"))

		# Re-create the LuxBall shape
		if self.luxBallShapeIsCube:
			self.scene.Parse(sceneProps.GetAllProperties("scene.objects.luxshell").
				Set(pyluxcore.Property("scene.objects.luxshell.ply", ["scenes/luxball/luxball-shell.ply"])))
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
			self.scene.Parse(sceneProps.GetAllProperties("scene.objects.luxshell").
				Set(pyluxcore.Property("scene.objects.luxshell.ply", ["LuxCubeMesh"])))
			self.luxBallShapeIsCube = True

		# End scene editing
		self.session.EndSceneEdit()
	
	def filmSetOutputChannel(self, type):
		# Stop the rendering
		self.session.Stop()
		self.session = None
		
		# Delete old channel outputs
		self.config.Delete("film.outputs")
		
		# Set the new channel outputs
		self.config.Parse(pyluxcore.Properties().
			Set(pyluxcore.Property("film.outputs.1.type", ["RGB_IMAGEPIPELINE"])).
			Set(pyluxcore.Property("film.outputs.1.filename", ["luxball_RGB_IMAGEPIPELINE.png"])).
			Set(pyluxcore.Property("film.outputs.2.type", [str(type)])).
			Set(pyluxcore.Property("film.outputs.2.filename", ["luxball_SELECTED_OUTPUT.exr"])))
		self.selectedFilmChannel = type
		
		# Re-start the rendering
		self.session = pyluxcore.RenderSession(self.config)
		self.session.Start()
		print("Film channel selected: %s" % (str(type)))
	
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
			self.session.GetFilm().GetOutputFloat(self.selectedFilmChannel, self.imageBufferFloat)
			pyluxcore.ConvertFilmChannelOutput_3xFloat_To_4xUChar(self.filmWidth, self.filmHeight, self.imageBufferFloat, self.imageBufferUChar,
				False if self.selectedFilmChannel == pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE else True)
			
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
	print("[%s]%s" % (strftime("%Y-%m-%d %H:%M:%S", localtime()), msg))

def main():
	print("LuxCore %s" % pyluxcore.Version())
	pyluxcore.Init(LogHandler)
	
	app = QApplication(sys.argv)
	rv = RenderView()
	rv.show()
	app.exec_()

if __name__ == '__main__':
	main()
