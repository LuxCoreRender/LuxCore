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

def TestSimpleRendering(cls, engineType):
	# Create the rendering configuration
	props = pyluxcore.Properties(LuxCoreTest.customConfigProps)
	props.SetFromFile("resources/scenes/simple/simple.cfg")
	props.Set(GetEngineProperties(engineType))
	config = pyluxcore.RenderConfig(props)

	# Run the rendering
	StandardImageTest(cls, "simple_rendering_" + engineType, config)

class SimpleRendering(ImageTest):
    pass

SimpleRendering = AddTests(SimpleRendering, TestSimpleRendering, GetEngineList())
