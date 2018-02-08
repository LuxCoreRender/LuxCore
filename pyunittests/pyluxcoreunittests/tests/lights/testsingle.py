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

################################################################################
# Point light test
################################################################################

def TestPointLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-point.cfg", "PointLight")

class PointLight(ImageTest):
    pass

PointLight = AddTests(PointLight, TestPointLight, GetTestCases())

################################################################################
# MapPoint light test
################################################################################

def TestMapPointLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-mappoint.cfg", "MapPointLight")

class MapPointLight(ImageTest):
    pass

MapPointLight = AddTests(MapPointLight, TestMapPointLight, GetTestCases())

################################################################################
# Spot light test
################################################################################

def TestSpotLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-spot.cfg", "SpotLight")

class SpotLight(ImageTest):
    pass

SpotLight = AddTests(SpotLight, TestSpotLight, GetTestCases())

################################################################################
# Area light test
################################################################################

def TestAreaLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-area.cfg", "AreaLight")

class AreaLight(ImageTest):
    pass

AreaLight = AddTests(AreaLight, TestAreaLight, GetTestCases())

################################################################################
# SunSky2 light test
################################################################################

def TestSunSky2Light(cls, params):
	StandardSceneTest(cls, params, "simple/light-sunsky2.cfg", "SunSky2Light")

class SunSky2Light(ImageTest):
    pass

SunSky2Light = AddTests(SunSky2Light, TestSunSky2Light, GetTestCases())

################################################################################
# Infinite light test
################################################################################

def TestInfiniteLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-infinite.cfg", "InfiniteLight")

class InfiniteLight(ImageTest):
    pass

InfiniteLight = AddTests(InfiniteLight, TestInfiniteLight, GetTestCases())

################################################################################
# Projection light test
################################################################################

def TestProjectionLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-projection.cfg", "ProjectionLight")

class ProjectionLight(ImageTest):
    pass

ProjectionLight = AddTests(ProjectionLight, TestProjectionLight, GetTestCases())

################################################################################
# ConstantInfinite light test
################################################################################

def TestConstantInfiniteLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-constantinfinite.cfg", "ConstantInfiniteLight")

class ConstantInfiniteLight(ImageTest):
    pass

ConstantInfiniteLight = AddTests(ConstantInfiniteLight, TestConstantInfiniteLight, GetTestCases())

################################################################################
# SharpDistant light test
################################################################################

def TestSharpDistantLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-sharpdistant.cfg", "SharpDistantLight")

class SharpDistantLight(ImageTest):
    pass

SharpDistantLight = AddTests(SharpDistantLight, TestSharpDistantLight, GetTestCases())

################################################################################
# Distant light test
################################################################################

def TestDistantLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-distant.cfg", "DistantLight")

class DistantLight(ImageTest):
    pass

DistantLight = AddTests(DistantLight, TestDistantLight, GetTestCases())

################################################################################
# Laser light test
################################################################################

def TestLaserLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-laser.cfg", "LaserLight")

class LaserLight(ImageTest):
    pass

LaserLight = AddTests(LaserLight, TestLaserLight, GetTestCases())
