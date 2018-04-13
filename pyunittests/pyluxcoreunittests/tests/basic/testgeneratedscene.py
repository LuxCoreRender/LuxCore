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
from pyluxcoreunittests.tests.build import *

def TestGeneratedScene(cls, params):
	engineType = params[0]
	samplerType = params[1]
	renderConfigAdditionalProps = params[2]

	# Create the rendering configuration
	cfgProps = pyluxcore.Properties()
	cfgProps.SetFromString("""
		film.width = 512
		film.height = 384
		""")

	# Set the rendering engine
	cfgProps.Set(pyluxcore.Property("renderengine.type", engineType))
	# Set the sampler
	cfgProps.Set(pyluxcore.Property("sampler.type", samplerType))

	cfgProps.Set(renderConfigAdditionalProps)
	cfgProps.Set(LuxCoreTest.customConfigProps)

	# Create the scene properties
	scnProps = pyluxcore.Properties()
	
	# Set the camera position
	scnProps.SetFromString("""
		scene.camera.lookat.orig = 0.0 5.0 2.0
		scene.camera.lookat.target = 0.0 0.0 0.0
		""")
	
	# Define a white matte material
	scnProps.SetFromString("""
		scene.materials.whitematte.type = matte
		scene.materials.whitematte.kd = 0.75 0.75 0.75
		""")

	# Add a plane
	scnProps.Set(BuildPlane("plane1", "whitematte"))
	
	# Add a distant light source
	scnProps.SetFromString("""
		scene.lights.distl.type = sharpdistant
		scene.lights.distl.color = 1.0 1.0 1.0
		scene.lights.distl.gain = 2.0 2.0 2.0
		scene.lights.distl.direction = 1.0 1.0 -1.0
		""")
	
	# Create the scene
	scene = pyluxcore.Scene()
	scene.Parse(scnProps)

	# Create the render config
	config = pyluxcore.RenderConfig(cfgProps, scene)

	# Run the rendering
	StandardImageTest(cls, "GeneratedScene_" + engineType + ("" if not samplerType else ("_" + samplerType)), config)

class GeneratedScene(ImageTest):
    pass

GeneratedScene = AddTests(GeneratedScene, TestGeneratedScene, GetTestCases())
