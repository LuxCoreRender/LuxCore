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

################################################################################
# ImageMapTexture wrap repeat test
################################################################################

def TestWrapRepeat(cls, params):
	StandardSceneTest(cls, params, "simple/tex-wrap-repeat.cfg", "WrapRepeat")

class WrapRepeat(ImageTest):
    pass

WrapRepeat = AddTests(WrapRepeat, TestWrapRepeat, GetTestCases())

################################################################################
# ImageMapTexture wrap black test
################################################################################

def TestWrapBlack(cls, params):
	StandardSceneTest(cls, params, "simple/tex-wrap-black.cfg", "WrapBlack")

class WrapBlack(ImageTest):
    pass

WrapBlack = AddTests(WrapBlack, TestWrapBlack, GetTestCases())

################################################################################
# ImageMapTexture wrap white test
################################################################################

def TestWrapWhite(cls, params):
	StandardSceneTest(cls, params, "simple/tex-wrap-white.cfg", "WrapWhite")

class WrapWhite(ImageTest):
    pass

WrapWhite = AddTests(WrapWhite, TestWrapWhite, GetTestCases())


################################################################################
# ImageMapTexture wrap clamp test
################################################################################

def TestWrapClamp(cls, params):
	StandardSceneTest(cls, params, "simple/tex-wrap-black.cfg", "WrapClamp")

class WrapClamp(ImageTest):
    pass

WrapClamp = AddTests(WrapClamp, TestWrapClamp, GetTestCases())

