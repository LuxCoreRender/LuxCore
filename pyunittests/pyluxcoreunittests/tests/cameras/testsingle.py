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
# Environment material test
################################################################################

def TestEnvironmentCamera(cls, params):
	StandardSceneTest(cls, params, "simple/camera-environment-area.cfg", "EnvironmentCamera")

class EnvironmentCamera(ImageTest):
    pass

EnvironmentCamera = AddTests(EnvironmentCamera, TestEnvironmentCamera, GetTestCases())

################################################################################
# Orthographic material test
################################################################################

def TestOrthographicCamera(cls, params):
	StandardSceneTest(cls, params, "simple/camera-orthographic-area.cfg", "OrthographicCamera")

class OrthographicCamera(ImageTest):
    pass

OrthographicCamera = AddTests(OrthographicCamera, TestOrthographicCamera, GetTestCases())

################################################################################
# OrthographicDOF material test
################################################################################

def TestOrthographicDOFCamera(cls, params):
	StandardSceneTest(cls, params, "simple/camera-orthographicdof-area.cfg", "OrthographicDOFCamera")

class OrthographicDOFCamera(ImageTest):
    pass

OrthographicDOFCamera = AddTests(OrthographicDOFCamera, TestOrthographicDOFCamera, GetTestCases())

################################################################################
# Perspective material test
################################################################################

def TestPerspectiveCamera(cls, params):
	StandardSceneTest(cls, params, "simple/camera-perspective-area.cfg", "PerspectiveCamera")

class PerspectiveCamera(ImageTest):
    pass

PerspectiveCamera = AddTests(PerspectiveCamera, TestPerspectiveCamera, GetTestCases())

################################################################################
# PerspectiveDOF material test
################################################################################

def TestPerspectiveDOFCamera(cls, params):
	StandardSceneTest(cls, params, "simple/camera-perspectivedof-area.cfg", "PerspectiveDOFCamera")

class PerspectiveDOFCamera(ImageTest):
    pass

PerspectiveDOFCamera = AddTests(PerspectiveDOFCamera, TestPerspectiveDOFCamera, GetTestCases())
