# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2018 by authors (see AUTHORS.txt)
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
		session = pyluxcore.RenderSession(config)

		session.Start()
		session.WaitForDone()
		session.Stop()

		# Get the imagepipeline result
		film1 = session.GetFilm()
		imageBufferFloat1 = array('f', [0.0] * (film1.GetWidth() * film1.GetHeight() * 3))
		film1.GetOutputFloat(pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, imageBufferFloat1)
		imageA = ConvertToImage((film1.GetWidth(), film1.GetHeight()), imageBufferFloat1)
	
		# Save the film
		film1.SaveFilm("simple.flm")

		# Load the film
		film2 = pyluxcore.Film("simple.flm")
		self.assertEqual(film2.GetWidth(), film1.GetWidth())
		self.assertEqual(film2.GetHeight(), film1.GetHeight())

		# Get the imagepipeline result
		imageBufferFloat2 = array('f', [0.0] * (film2.GetWidth() * film2.GetHeight() * 3))
		film2.GetOutputFloat(pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, imageBufferFloat2)
		imageB = ConvertToImage((film2.GetWidth(), film2.GetHeight()), imageBufferFloat2)

		# To debug
		#imageA.save("imageA.png")
		#imageB.save("imageB.png")

		# Check if there is a difference
		(sameImage, diffCount, diffImage) = CompareImage(imageA, imageB)

		self.assertTrue(sameImage)
		
		os.unlink("simple.flm")

