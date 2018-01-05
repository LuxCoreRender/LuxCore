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
# ArchGlass material test
################################################################################

def TestArchGlassMaterial(cls, params):
	StandardSceneTest(cls, params, "simple/mat-archglass-area.cfg", "ArchGlassMaterial")

class ArchGlassMaterial(ImageTest):
    pass

ArchGlassMaterial = AddTests(ArchGlassMaterial, TestArchGlassMaterial, GetTestCases())

################################################################################
# CarPaint material test
################################################################################

def TestCarPaintMaterial(cls, params):
	StandardSceneTest(cls, params, "simple/mat-carpaint-area.cfg", "CarPaintMaterial")

class CarPaintMaterial(ImageTest):
    pass

CarPaintMaterial = AddTests(CarPaintMaterial, TestCarPaintMaterial, GetTestCases())

################################################################################
# Cloth material test
################################################################################

def TestClothMaterial(cls, params):
	StandardSceneTest(cls, params, "simple/mat-cloth-area.cfg", "ClothMaterial")

class ClothMaterial(ImageTest):
    pass

ClothMaterial = AddTests(ClothMaterial, TestClothMaterial, GetTestCases())

################################################################################
# Glass material test
################################################################################

def TestGlassMaterial(cls, params):
	StandardSceneTest(cls, params, "simple/mat-glass-area.cfg", "GlassMaterial")

class GlassMaterial(ImageTest):
    pass

GlassMaterial = AddTests(GlassMaterial, TestGlassMaterial, GetTestCases())

################################################################################
# Glass+Dispersion material test
################################################################################

def TestGlassDispMaterial(cls, params):
	StandardSceneTest(cls, params, "simple/mat-glass-disp-area.cfg", "GlassDispMaterial")

class GlassDispMaterial(ImageTest):
    pass

GlassDispMaterial = AddTests(GlassDispMaterial, TestGlassDispMaterial, GetTestCases())

################################################################################
# Matte material test
################################################################################

def TestMatteMaterial(cls, params):
	StandardSceneTest(cls, params, "simple/mat-matte-area.cfg", "MatteMaterial")

class MatteMaterial(ImageTest):
    pass

MatteMaterial = AddTests(MatteMaterial, TestMatteMaterial, GetTestCases())
