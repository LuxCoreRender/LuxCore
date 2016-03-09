# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2015 by authors (see AUTHORS.txt)
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

def TestSceneEditRendering(cls, params):
	engineType = params[0]
	samplerType = params[1]

	# Create the rendering configuration
	props = pyluxcore.Properties(LuxCoreTest.customConfigProps)
	props.SetFromFile("resources/scenes/simple/simple.cfg")
	
	# Set the rendering engine
	props.Set(GetEngineProperties(engineType))
	# Set the sampler (if required)
	if samplerType:
		props.Set(pyluxcore.Property("sampler.type", samplerType))

	config = pyluxcore.RenderConfig(props)

	# Run the rendering
	StandardAnimTest(cls, "SceneEditRendering_" + engineType + ("" if not samplerType else ("_" + samplerType)), config, 5)

def SceneEdit(self, session, frame):
	config = session.GetRenderConfig()
	scene = config.GetScene()

	props = pyluxcore.Properties()
	props.SetFromString("""
		scene.objects.box2.ply = resources/scenes/simple/simple-mat-cube2.ply
		scene.objects.box2.material = greenmatte
		scene.objects.box2.transformation = 1 0 0 0  0 1 0 0  0 0 1 0  %f 0 0 1
		""" % ((frame + 1) / 5.0))

	scene.Parse(props)

class SceneEditRendering(ImageTest):
	def SceneEdit(self, session, frame):
		SceneEdit(self, session, frame)

SceneEditRendering = AddTests(SceneEditRendering, TestSceneEditRendering, GetEngineListWithSamplers())
