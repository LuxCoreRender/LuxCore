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

# Note: keep in alphabetical order

################################################################################
# Displacement shape test
################################################################################

def TestDisplacementShape(cls, params):
	StandardSceneTest(cls, params, "simple/shape-displacement-area.cfg", "DisplacementShape")

class DisplacementShape(ImageTest):
    pass

DisplacementShape = AddTests(DisplacementShape, TestDisplacementShape, GetTestCases())

################################################################################
# Subdiv shape test
################################################################################

def TestSubdivShape(cls, params):
	StandardSceneTest(cls, params, "simple/shape-subdiv-area.cfg", "SubdivShape")

class SubdivShape(ImageTest):
    pass

SubdivShape = AddTests(SubdivShape, TestSubdivShape, GetTestCases())

################################################################################
# Simplify shape test
################################################################################

def TestSimplifyShape(cls, params):
	StandardSceneTest(cls, params, "simple/shape-simplify-area.cfg", "SimplifyShape")

class SimplifyShape(ImageTest):
    pass

SimplifyShape = AddTests(SimplifyShape, TestSimplifyShape, GetTestCases())
