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

# Test cases for subdivision memory bugs
# These tests verify that maxedgescreensize parameter works correctly
# and that memory usage doesn't grow excessively during subdivision

def TestSubdivMemoryBug(cls, params):
	"""
	Test subdivision with various maxedgescreensize values to ensure
	memory usage is controlled properly.
	"""
	StandardSceneTest(cls, params, "simple/shape-subdiv-memory.cfg", "SubdivMemory")

class SubdivMemory(ImageTest):
    pass

# Add test cases with different maxedgescreensize values
subdiv_memory_cases = [
    ("PATHCPU", "RANDOM", GetDefaultEngineProperties("PATHCPU"), 0.1),  # Default value
    ("PATHCPU", "RANDOM", GetDefaultEngineProperties("PATHCPU"), 0.5),  # Larger value
    ("PATHCPU", "RANDOM", GetDefaultEngineProperties("PATHCPU"), 0.01), # Smaller value
]

SubdivMemory = AddTests(SubdivMemory, TestSubdivMemoryBug, subdiv_memory_cases)

# Additional test to verify maxedgescreensize parameter is properly applied
class SubdivMaxEdgeScreenSize(unittest.TestCase):
    def test_maxedgescreensize_parameter(self):
        """
        Test that the maxedgescreensize parameter is properly applied
        and affects subdivision behavior.
        """
        # Create a scene with subdiv shape
        config = pyluxcore.RenderConfig("resources/scenes/simple/shape-subdiv-memory.cfg")
        
        session = pyluxcore.RenderSession(config)
        session.Start()
        session.RenderFrame()
        
        # Check that the parameter was applied
        value = session.GetProperty("scene.shapes.sphere2.maxedgescreensize")
        self.assertEqual(value, "0.1")
        
        session.Stop()