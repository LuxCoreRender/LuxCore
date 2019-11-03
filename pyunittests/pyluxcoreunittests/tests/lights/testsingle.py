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
# ConstantInfinite light test
################################################################################

def TestConstantInfiniteLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-constantinfinite.cfg", "ConstantInfiniteLight")

class ConstantInfiniteLight(ImageTest):
    pass

ConstantInfiniteLight = AddTests(ConstantInfiniteLight, TestConstantInfiniteLight, GetTestCases())

################################################################################
# Distant light test
################################################################################

def TestDistantLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-distant.cfg", "DistantLight")

class DistantLight(ImageTest):
    pass

DistantLight = AddTests(DistantLight, TestDistantLight, GetTestCases())

################################################################################
# Infinite light test
################################################################################

def TestInfiniteLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-infinite.cfg", "InfiniteLight")

class InfiniteLight(ImageTest):
    pass

InfiniteLight = AddTests(InfiniteLight, TestInfiniteLight, GetTestCases())

################################################################################
# Laser light test
################################################################################

def TestLaserLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-laser.cfg", "LaserLight")

class LaserLight(ImageTest):
    pass

LaserLight = AddTests(LaserLight, TestLaserLight, GetTestCases())

################################################################################
# MapPoint light test
################################################################################

def TestMapPointLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-mappoint.cfg", "MapPointLight")

class MapPointLight(ImageTest):
    pass

MapPointLight = AddTests(MapPointLight, TestMapPointLight, GetTestCases())

################################################################################
# MapSphere light test
################################################################################

def TestMapSphereLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-mapsphere.cfg", "MapSphereLight")

class MapSphereLight(ImageTest):
    pass

MapSphereLight = AddTests(MapSphereLight, TestMapSphereLight, GetTestCases())

################################################################################
# Point light test
################################################################################

def TestPointLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-point.cfg", "PointLight")

class PointLight(ImageTest):
    pass

PointLight = AddTests(PointLight, TestPointLight, GetTestCases())

################################################################################
# Projection light test
################################################################################

def TestProjectionLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-projection.cfg", "ProjectionLight")

class ProjectionLight(ImageTest):
    pass

ProjectionLight = AddTests(ProjectionLight, TestProjectionLight, GetTestCases())

################################################################################
# SharpDistant light test
################################################################################

def TestSharpDistantLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-sharpdistant.cfg", "SharpDistantLight")

class SharpDistantLight(ImageTest):
    pass

SharpDistantLight = AddTests(SharpDistantLight, TestSharpDistantLight, GetTestCases())

################################################################################
# Sky2 light test
################################################################################

def TestSky2Light(cls, params):
	StandardSceneTest(cls, params, "simple/light-sky2.cfg", "Sky2Light")

class Sky2Light(ImageTest):
    pass

Sky2Light = AddTests(Sky2Light, TestSky2Light, GetTestCases())

################################################################################
# Sphere light test
################################################################################

def TestSphereLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-sphere.cfg", "SphereLight")

class SphereLight(ImageTest):
    pass

SphereLight = AddTests(SphereLight, TestSphereLight, GetTestCases())

################################################################################
# Spot light test
################################################################################

def TestSpotLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-spot.cfg", "SpotLight")

class SpotLight(ImageTest):
    pass

SpotLight = AddTests(SpotLight, TestSpotLight, GetTestCases())

################################################################################
# Sun light test
################################################################################

def TestSunLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-sun.cfg", "SunLight")

class SunLight(ImageTest):
    pass

SunLight = AddTests(SunLight, TestSunLight, GetTestCases())

################################################################################
# Triangle light test
################################################################################

def TestTriangleLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-triangle.cfg", "TriangleLight")

class TriangleLight(ImageTest):
    pass

TriangleLight = AddTests(TriangleLight, TestTriangleLight, GetTestCases())
