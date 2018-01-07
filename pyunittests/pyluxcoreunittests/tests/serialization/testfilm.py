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

import unittest
import pyluxcore

from pyluxcoreunittests.tests.utils import *
from pyluxcoreunittests.tests.imagetest import *

class TestFilm(unittest.TestCase):
	def test_Film_SaveLoad(self):
		# Load the configuration from file
		props = pyluxcore.Properties("resources/scenes/simple/simple.cfg")

		# Change the render engine to PATHCPU
		props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))
		props.Set(pyluxcore.Property("sampler.type", ["RANDOM"]))
		props.Set(GetDefaultEngineProperties("PATHCPU"))

		config = pyluxcore.RenderConfig(props)
		session = DoRenderSession(config)

		# Get the imagepipeline result
		filmA = session.GetFilm()
		imageA = GetImagePipelineImage(filmA)
	
		# Save the film
		filmA.SaveFilm("simple.flm")

		# Load the film
		filmB = pyluxcore.Film("simple.flm")
		self.assertEqual(filmB.GetWidth(), filmA.GetWidth())
		self.assertEqual(filmB.GetHeight(), filmA.GetHeight())

		# Get the imagepipeline result
		imageB = GetImagePipelineImage(filmB)

		# To debug
		#imageA.save("imageA.png")
		#imageB.save("imageB.png")

		# Check if there is a difference
		(sameImage, diffCount, diffImage) = CompareImage(imageA, imageB)

		self.assertTrue(sameImage)
		
		os.unlink("simple.flm")

