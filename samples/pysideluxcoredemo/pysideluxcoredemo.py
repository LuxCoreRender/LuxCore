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
		self.image = QImage(self.filmWidth, self.filmHeight, QImage.Format_RGB888)
		self.image.fill(qRgb(0, 0, 0))

		# Read the configuration and start the rendering
		self.config = pyluxcore.RenderConfig(props)
		self.session = pyluxcore.RenderSession(self.config)
		self.session.Start()
		
		self.timer = QBasicTimer()
		self.timer.start(1000, self)
	
	def center(self):
		screen = QDesktopWidget().screenGeometry()
		size =  self.geometry()
		self.move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2)
	
	def timerEvent(self, event):
		if event.timerId() == self.timer.timerId():
			# Print some information about the rendering progress
			
			# Update statistics
			self.session.UpdateStats()
			
			stats = self.session.GetStats();
			print("[Elapsed time: %3dsec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
			
			# Update the image
			imageBuffer = self.session.GetScreenBuffer();
			index = 0
			for y in range(self.filmHeight):
				for x in range(self.filmWidth):
					self.image.setPixel(x, self.filmHeight - y -1, qRgb(imageBuffer[index], imageBuffer[index + 1], imageBuffer[index + 2]))
					index += 3
			
			self.update()
		else:
			QFrame.timerEvent(self, event)
	
	def paintEvent(self, event):
		painter = QPainter(self)
		painter.drawImage(QPoint(0, 0), self.image)
	
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
		self.image = QImage(self.filmWidth, self.filmHeight, QImage.Format_RGB888)
		self.image.fill(qRgb(0, 0, 0))
		
		# Re-start the rendering
		self.session = pyluxcore.RenderSession(self.config)
		self.session.Start()
		
		super(RenderView, self).resizeEvent(event)
	
	def closeEvent(self, event):
		self.session.Stop()
		
		ret = QMessageBox.warning(self, "Save ?", \
				"Do you want to save the image ?", \
				QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel)
		if ret == QMessageBox.Save:
			# Save the rendered image
			self.session.SaveFilm()
			print("Done.")
			event.accept()
		elif ret == QMessageBox.Discard:
			print("Done.")
			event.accept()
		elif ret == QMessageBox.Cancel:
			event.ignore()

def main():   
	print("LuxCore %s" % pyluxcore.version())
	
	app = QApplication(sys.argv)
	rv = RenderView("scenes/luxball/luxball-hdr.cfg")
	rv.show()
	sys.exit(app.exec_())

if __name__ == '__main__':
	main()
