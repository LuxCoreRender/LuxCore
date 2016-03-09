# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2015 by authors (see AUTHORS.txt)
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

PointLight = AddTests(PointLight, TestPointLight, GetEngineListWithSamplers())

################################################################################
# MapPoint light test
################################################################################

def TestMapPointLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-mappoint.cfg", "MapPointLight")

class MapPointLight(ImageTest):
    pass

MapPointLight = AddTests(MapPointLight, TestMapPointLight, GetEngineListWithSamplers())

################################################################################
# Spot light test
################################################################################

def TestSpotLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-spot.cfg", "SpotLight")

class SpotLight(ImageTest):
    pass

SpotLight = AddTests(SpotLight, TestSpotLight, GetEngineListWithSamplers())

################################################################################
# Area light test
################################################################################

def TestAreaLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-area.cfg", "AreaLight")

class AreaLight(ImageTest):
    pass

AreaLight = AddTests(AreaLight, TestAreaLight, GetEngineListWithSamplers())

################################################################################
# SunSky1 light test
################################################################################

def TestSunSky1Light(cls, params):
	StandardSceneTest(cls, params, "simple/light-sunsky1.cfg", "SunSky1Light")

class SunSky1Light(ImageTest):
    pass

SunSky1Light = AddTests(SunSky1Light, TestSunSky1Light, GetEngineListWithSamplers())

################################################################################
# SunSky2 light test
################################################################################

def TestSunSky2Light(cls, params):
	StandardSceneTest(cls, params, "simple/light-sunsky2.cfg", "SunSky2Light")

class SunSky2Light(ImageTest):
    pass

SunSky2Light = AddTests(SunSky2Light, TestSunSky2Light, GetEngineListWithSamplers())

################################################################################
# Infinite light test
################################################################################

def TestInfiniteLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-infinite.cfg", "InfiniteLight")

class InfiniteLight(ImageTest):
    pass

InfiniteLight = AddTests(InfiniteLight, TestInfiniteLight, GetEngineListWithSamplers())

################################################################################
# Projection light test
################################################################################

def TestProjectionLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-projection.cfg", "ProjectionLight")

class ProjectionLight(ImageTest):
    pass

ProjectionLight = AddTests(ProjectionLight, TestProjectionLight, GetEngineListWithSamplers())

################################################################################
# ConstantInfinite light test
################################################################################

def TestConstantInfiniteLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-constantinfinite.cfg", "ConstantInfiniteLight")

class ConstantInfiniteLight(ImageTest):
    pass

ConstantInfiniteLight = AddTests(ConstantInfiniteLight, TestConstantInfiniteLight, GetEngineListWithSamplers())

################################################################################
# SharpDistant light test
################################################################################

def TestSharpDistantLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-sharpdistant.cfg", "SharpDistantLight")

class SharpDistantLight(ImageTest):
    pass

SharpDistantLight = AddTests(SharpDistantLight, TestSharpDistantLight, GetEngineListWithSamplers())

################################################################################
# Distant light test
################################################################################

def TestDistantLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-distant.cfg", "DistantLight")

class DistantLight(ImageTest):
    pass

DistantLight = AddTests(DistantLight, TestDistantLight, GetEngineListWithSamplers())

################################################################################
# Laser light test
################################################################################

def TestLaserLight(cls, params):
	StandardSceneTest(cls, params, "simple/light-laser.cfg", "LaserLight")

class LaserLight(ImageTest):
    pass

LaserLight = AddTests(LaserLight, TestLaserLight, GetEngineListWithSamplers())
