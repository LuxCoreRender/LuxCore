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

def CreateRenderConfig():
	props = pyluxcore.Properties("resources/scenes/simple/simple.cfg")
	config = pyluxcore.RenderConfig(props)

	return config

def TestBasicRendering(cls, engineType):
	# Create the rendering configuration
	config = CreateRenderConfig()

	# Add halt condition
	config.GetProperties().Set(pyluxcore.Property("batch.haltdebug", "1"))
	# Set the engine type
	config.GetProperties().Set(pyluxcore.Property("renderengine.type", engineType))

	# Run the rendering
	StandardImageTest(cls, "basic_rendering_" + engineType, config)

class BasicRendering(unittest.TestCase, ImageTest):
    pass

BasicRendering = AddTests(BasicRendering, TestBasicRendering, ["PATHCPU", "BIDIRCPU"])
