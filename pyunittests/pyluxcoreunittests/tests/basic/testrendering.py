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

import unittest
import pyluxcore

from pyluxcoreunittests.tests.utils import *
from pyluxcoreunittests.tests.imagetest import *

def TestSimpleRendering(cls, params):
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
	StandardImageTest(cls, "SimpleRendering_" + engineType + ("" if not samplerType else ("_" + samplerType)), config)

class SimpleRendering(ImageTest):
    pass

SimpleRendering = AddTests(SimpleRendering, TestSimpleRendering, GetEngineListWithSamplers())
