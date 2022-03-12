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

class TestDuplicateObject(unittest.TestCase):
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
	
	def DuplicateObject(self, type):
		config = self.CreateConfig(type)
		scene = config.GetScene()

		# Duplicate the base object
		mat = [1.0 if i==j else 0.0 for j in range(4) for i in range(4)]

		#t1 = time.time()
		#objCount = 1000000
		objCount = 5

		for i in range(objCount):
			mat[0 + 3 * 4] = 2.5 * (i + 1)
			objID = i
			scene.DuplicateObject("box1", "box1_dup" + str(i), mat, objID)
		#t2 = time.time()
		#logger.info("Elapsed time: " + str(t2 - t1))

		# Time for 1,000,000 Normal: 5.675945281982422 secs
		# Time for 1,000,000 Instance: 5.323255777359009 secs
		# Time for 1,000,000 Motion: 5.41925048828125 secs

		# Run the rendering
		StandardImageTest(self, "Scene_DuplicateObject" + type, config)

	def test_Scene_DuplicateObjectNormal(self):
		self.DuplicateObject("Normal")

	def test_Scene_DuplicateObjectInstance(self):
		self.DuplicateObject("Instance")

	def test_Scene_DuplicateObjectMotion(self):
		self.DuplicateObject("Motion")

	#---------------------------------------------------------------------------

	def DuplicateMotionObject(self, type):
		config = self.CreateConfig(type)
		scene = config.GetScene()

		# Duplicate the base object
		mat1 = [1.0 if i==j else 0.0 for j in range(4) for i in range(4)]
		mat2 = [1.0 if i==j else 0.0 for j in range(4) for i in range(4)]

		#t1 = time.time()
		#objCount = 1000000
		objCount = 5

		times = [0.0, 1.0]
		mats = [mat1, mat2]
		for i in range(objCount):
			mat1[0 + 3 * 4] = 2.5 * (i + 1);
			mat2[0 + 3 * 4] = mat1[0 + 3 * 4] + 0.5;
			objID = i
			scene.DuplicateObject("box1", "box1_dup" + str(i), 2, times, mats, objID)
		#t2 = time.time()
		#logger.info("Elapsed time: " + str(t2 - t1))

		# Time for 1,000,000 Normal: 10.409008741378784 secs

		# Run the rendering
		StandardImageTest(self, "Scene_DuplicateMotionObject" + type, config)

	def test_Scene_DuplicateMotionObjectNormal(self):
		self.DuplicateMotionObject("Normal")

	def test_Scene_DuplicateMotionObjectInstance(self):
		self.DuplicateMotionObject("Instance")

	def test_Scene_DuplicateMotionObjectMotion(self):
		self.DuplicateMotionObject("Motion")

	#---------------------------------------------------------------------------

	def DuplicateObjectMulti(self, type):
		config = self.CreateConfig(type)
		scene = config.GetScene()

		#objCount = 1000000
		objCount = 5

		mats = array("f", [0.0] * (16 * objCount))
		objIDs = array("I", [0] * objCount)
		index = 0
		for i in range(objCount):
			objIDs[i] = i

			for y in range(4):
				for x in range(4):
					if (x == y):
						mats[index + x + y * 4] = 1.0
				
			mats[index + 0 + 3 * 4] = 2.5 * (i + 1);
			
			index += 16

		# Duplicate the base object
		#t1 = time.time()
		scene.DuplicateObject("box1", "box1_dup", objCount, mats, objIDs)
		#t2 = time.time()
		#logger.info("Elapsed time: " + str(t2 - t1))

		# Time for 1,000,000 Normal: 3.742476224899292 secs
		# Time for 1,000,000 Instance: 3.73423433303833 secs
		# Time for 1,000,000 Motion: 3.923335552215576 secs

		# Run the rendering
		StandardImageTest(self, "Scene_DuplicateObjectMulti" + type, config)

	def test_Scene_DuplicateObjectMultiNormal(self):
		self.DuplicateObjectMulti("Normal")

	def test_Scene_DuplicateObjectMultiInstance(self):
		self.DuplicateObjectMulti("Instance")

	def test_Scene_DuplicateObjectMultiMotion(self):
		self.DuplicateObjectMulti("Motion")

	#---------------------------------------------------------------------------

	def DuplicateMotionObjectMulti(self, type):
		config = self.CreateConfig(type)
		scene = config.GetScene()

		#objCount = 1000000
		objCount = 5

		times = array("f", [0.0] * (2 * objCount))
		mats = array("f", [0.0] * (2 * 16 * objCount))
		objIDs = array("I", [0] * objCount)
		timesIndex = 0
		matsIndex = 0
		for i in range(objCount):
			times[timesIndex] = 0.0
			times[timesIndex + 1] = 1.0
			timesIndex += 2

			objIDs[i] = i

			for s in range(2):
				for y in range(4):
					for x in range(4):
						if (x == y):
							mats[matsIndex + x + y * 4] = 1.0
				
				mats[matsIndex + 0 + 3 * 4] = 2.5 * (i + 1) + s * 0.5
			
				matsIndex += 16

		# Duplicate the base object
		#t1 = time.time()
		scene.DuplicateObject("box1", "box1_dup", objCount, 2, times, mats, objIDs)
		#t2 = time.time()
		#logger.info("Elapsed time: " + str(t2 - t1))

		# Time for 1,000,000 Normal: 6.421358823776245 secs
		# Time for 1,000,000 Instance: 6.354687213897705 secs
		# Time for 1,000,000 Motion: 6.411757946014404 secs

		# Run the rendering
		StandardImageTest(self, "Scene_DuplicateMotionObjectMulti" + type, config)

	def test_Scene_DuplicateMotionObjectMultiNormal(self):
		self.DuplicateMotionObjectMulti("Normal")

	def test_Scene_DuplicateMotionObjectMultiInstance(self):
		self.DuplicateMotionObjectMulti("Instance")

	def test_Scene_DuplicateMotionObjectMultiMotion(self):
		self.DuplicateMotionObjectMulti("Motion")
