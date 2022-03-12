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

import time
import array
import unittest
import pyluxcore

from pyluxcoreunittests.tests.utils import *
from pyluxcoreunittests.tests.imagetest import *

class TestDeleteObjects(unittest.TestCase):
	def CreateConfig(self, type):
		# Load the configuration from file
		props = pyluxcore.Properties("resources/scenes/simple/simple.cfg")

		# Change the render engine to PATHCPU
		props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))
		props.Set(pyluxcore.Property("sampler.type", ["RANDOM"]))
		props.Set(GetDefaultEngineProperties("PATHCPU"))

		config = pyluxcore.RenderConfig(props)
		scene = config.GetScene()

		# Delete the red and green boxes
		scene.DeleteObject("box1")
		scene.DeleteObject("box2")
		
		# Create the base object
		props = pyluxcore.Properties()
		if (type == "Normal"):
			props.SetFromString("""
				scene.objects.box1.ply = resources/scenes/simple/simple-mat-cube1.ply
				scene.objects.box1.material = redmatte
				""")
		elif (type == "Instance"):
			props.SetFromString("""
				scene.objects.box1.ply = resources/scenes/simple/simple-mat-cube1.ply
				scene.objects.box1.material = redmatte
				scene.objects.box1.transformation = 1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  -0.5 0.0 0.0 1.0
				""")			
		elif (type == "Motion"):
			props.SetFromString("""
				scene.objects.box1.ply = resources/scenes/simple/simple-mat-cube1.ply
				scene.objects.box1.material = redmatte
				scene.objects.box1.motion.0.time = 0.0
				scene.objects.box1.motion.0.transformation = 1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  -0.25 0.0 0.0 1.0
				scene.objects.box1.motion.1.time = 1.0
				scene.objects.box1.motion.1.transformation = 1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.25 0.0 0.0 1.0
				""")
		else:
			self.assertFalse()
		scene.Parse(props)
		
		return config
	
	#---------------------------------------------------------------------------
	
	def DeleteObjects(self, type):
		config = self.CreateConfig(type)
		scene = config.GetScene()

		# Duplicate the base object
		mat = [1.0 if i==j else 0.0 for j in range(4) for i in range(4)]

		#t1 = time.time()
		#objCount = 1000000
		objCount = 5

		objNames = []
		for i in range(objCount):
			mat[0 + 3 * 4] = 2.5 * (i + 1)
			objID = i
			scene.DuplicateObject("box1", "box1_dup" + str(i), mat, objID)
			objNames += ["box1_dup" + str(i)]
		#t2 = time.time()
		#logger.info("Elapsed time: " + str(t2 - t1))

		# Time for 1,000,000 Normal: 5.675945281982422 secs
		# Time for 1,000,000 Instance: 5.323255777359009 secs
		# Time for 1,000,000 Motion: 5.41925048828125 secs

		#t1 = time.time()
		scene.DeleteObjects(objNames)
		#t2 = time.time()
		#logger.info("Elapsed time: " + str(t2 - t1))

		# Time for 1,000,000 Normal (before optimization): 4.250431776046753 secs
		# Time for 1,000,000 Normal (after optimization): 0.9761378765106201 secs

		# Run the rendering
		StandardImageTest(self, "Scene_DeleteObject" + type, config)

	def test_Scene_DeleteObjectsNormal(self):
		self.DeleteObjects("Normal")

	def test_Scene_DeleteObjectsInstance(self):
		self.DeleteObjects("Instance")

	def test_Scene_DeletesObjectsMotion(self):
		self.DeleteObjects("Motion")
