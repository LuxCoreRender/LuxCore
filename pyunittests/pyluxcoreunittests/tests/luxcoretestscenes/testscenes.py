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
# 2-glasses scene test
################################################################################

def TestTwoGlassesLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/2-glasses/render.cfg"
	filmWidth = 384
	filmHeight = 384

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))
	additionalProps.Set(pyluxcore.Property("periodicsave.film.outputs.period", 0))

	StandardSceneTest(cls, params, cfgFile, "TwoGlassesLuxCoreTestScene", additionalProps)

class TwoGlassesLuxCoreTestScene(ImageTest):
    pass

TwoGlassesLuxCoreTestScene = AddTests(TwoGlassesLuxCoreTestScene, TestTwoGlassesLuxCoreTestScene, GetTestCases())

################################################################################
# 3-spheres scene test
################################################################################

def TestThreeSpheresLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/3-spheres/render.cfg"
	filmWidth = 300
	filmHeight = 225

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "ThreeSpheresLuxCoreTestScene", additionalProps)

class ThreeSpheresLuxCoreTestScene(ImageTest):
    pass

ThreeSpheresLuxCoreTestScene = AddTests(ThreeSpheresLuxCoreTestScene, TestThreeSpheresLuxCoreTestScene, GetTestCases())

################################################################################
# DanishMood scene test
################################################################################

def TestDanishMoodLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/DanishMood/LuxCoreScene/render.cfg"
	filmWidth = 423
	filmHeight = 570

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "DanishMoodLuxCoreTestScene", additionalProps)

class DanishMoodLuxCoreTestScene(ImageTest):
    pass

DanishMoodLuxCoreTestScene = AddTests(DanishMoodLuxCoreTestScene, TestDanishMoodLuxCoreTestScene, GetTestCases())

################################################################################
# DLSC scene test
################################################################################

def TestDLSCLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/DLSC/LuxCoreScene/render.cfg"
	filmWidth = 320
	filmHeight = 432

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "DLSCLuxCoreTestScene", additionalProps)

class DLSCLuxCoreTestScene(ImageTest):
    pass

DLSCLuxCoreTestScene = AddTests(DLSCLuxCoreTestScene, TestDLSCLuxCoreTestScene, GetTestCases())

################################################################################
# Food scene test
################################################################################

def TestFoodLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/Food/LuxCoreScene/render.cfg"
	filmWidth = 300
	filmHeight = 300

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "FoodLuxCoreTestScene", additionalProps)

class FoodLuxCoreTestScene(ImageTest):
    pass

FoodLuxCoreTestScene = AddTests(FoodLuxCoreTestScene, TestFoodLuxCoreTestScene, GetTestCases())

################################################################################
# HallBench scene test
################################################################################

def TestHallBenchLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/HallBench/LuxCoreScene/render.cfg"
	filmWidth = 360
	filmHeight = 500

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "HallBenchLuxCoreTestScene", additionalProps)

class HallBenchLuxCoreTestScene(ImageTest):
    pass

HallBenchLuxCoreTestScene = AddTests(HallBenchLuxCoreTestScene, TestHallBenchLuxCoreTestScene, GetTestCases())

################################################################################
# LuxBall scene test
################################################################################

def TestLuxBallLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/LuxBall/LuxCoreScene/render.cfg"
	filmWidth = 400
	filmHeight = 400

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "LuxBallLuxCoreTestScene", additionalProps)

class LuxBallLuxCoreTestScene(ImageTest):
    pass

LuxBallLuxCoreTestScene = AddTests(LuxBallLuxCoreTestScene, TestLuxBallLuxCoreTestScene, GetTestCases())

################################################################################
# LuxCore21Benchmark scene test
################################################################################

def TestLuxCore21BenchmarkLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/LuxCore2.1Benchmark/LuxCoreScene/render.cfg"
	filmWidth = 294
	filmHeight = 400

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "LuxCore21BenchmarkLuxCoreTestScene", additionalProps)

class LuxCore21BenchmarkLuxCoreTestScene(ImageTest):
    pass

LuxCore21BenchmarkLuxCoreTestScene = AddTests(LuxCore21BenchmarkLuxCoreTestScene, TestLuxCore21BenchmarkLuxCoreTestScene, GetTestCases())

################################################################################
# LuxCoreRenderWallpaper scene test
################################################################################

def TestLuxCoreRenderWallpaperLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/LuxCoreRender-Wallpaper/render.cfg"
	filmWidth = 355
	filmHeight = 200

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "LuxCoreRenderWallpaperLuxCoreTestScene", additionalProps)

class LuxCoreRenderWallpaperLuxCoreTestScene(ImageTest):
    pass

LuxCoreRenderWallpaperLuxCoreTestScene = AddTests(LuxCoreRenderWallpaperLuxCoreTestScene, TestLuxCoreRenderWallpaperLuxCoreTestScene, GetTestCases())

################################################################################
# LuxMarkHotel scene test
################################################################################

def TestLuxMarkHotelLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/LuxMark-Hotel/render.cfg"
	filmWidth = 512
	filmHeight = 288

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "LuxMarkHotelLuxCoreTestScene", additionalProps)

class LuxMarkHotelLuxCoreTestScene(ImageTest):
    pass

LuxMarkHotelLuxCoreTestScene = AddTests(LuxMarkHotelLuxCoreTestScene, TestLuxMarkHotelLuxCoreTestScene, GetTestCases())

################################################################################
# LuxMarkLuxBall scene test
################################################################################

def TestLuxMarkLuxBallLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/LuxMark-LuxBall/render.cfg"
	filmWidth = 400
	filmHeight = 400

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "LuxMarkLuxBallLuxCoreTestScene", additionalProps)

class LuxMarkLuxBallLuxCoreTestScene(ImageTest):
    pass

LuxMarkLuxBallLuxCoreTestScene = AddTests(LuxMarkLuxBallLuxCoreTestScene, TestLuxMarkLuxBallLuxCoreTestScene, GetTestCases())

################################################################################
# LuxMarkMic scene test
################################################################################

def TestLuxMarkMicLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/LuxMark-Mic/render.cfg"
	filmWidth = 400
	filmHeight = 400

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "LuxMarkMicLuxCoreTestScene", additionalProps)

class LuxMarkMicLuxCoreTestScene(ImageTest):
    pass

LuxMarkMicLuxCoreTestScene = AddTests(LuxMarkMicLuxCoreTestScene, TestLuxMarkMicLuxCoreTestScene, GetTestCases())

################################################################################
# PointinessExamples scene test
################################################################################

def TestPointinessExamplesLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/PointinessExamples/LuxCoreScene/render.cfg"
	filmWidth = 525
	filmHeight = 140

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "PointinessExamplesLuxCoreTestScene", additionalProps)

class PointinessExamplesLuxCoreTestScene(ImageTest):
    pass

PointinessExamplesLuxCoreTestScene = AddTests(PointinessExamplesLuxCoreTestScene, TestPointinessExamplesLuxCoreTestScene, GetTestCases())

################################################################################
# Prism scene test
################################################################################

def TestPrismLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/Prism/render.cfg"
	filmWidth = 525
	filmHeight = 140

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "PrismLuxCoreTestScene", additionalProps)

class PrismLuxCoreTestScene(ImageTest):
    pass

PrismLuxCoreTestScene = AddTests(PrismLuxCoreTestScene, TestPrismLuxCoreTestScene, GetTestCases())

################################################################################
# ProceduralLeaves scene test
################################################################################

def TestProceduralLeavesLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/ProceduralLeaves/LuxCoreScene/render.cfg"
	filmWidth = 512
	filmHeight = 288

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "ProceduralLeavesLuxCoreTestScene", additionalProps)

class ProceduralLeavesLuxCoreTestScene(ImageTest):
    pass

ProceduralLeavesLuxCoreTestScene = AddTests(ProceduralLeavesLuxCoreTestScene, TestProceduralLeavesLuxCoreTestScene, GetTestCases())

################################################################################
# RainbowColorsAndPrism scene test
################################################################################

def TestRainbowColorsAndPrismLuxCoreTestScene(cls, params):
	cfgFile = LuxCoreTest.customConfigProps.Get("luxcoretestscenes.directory").Get()[0] + "/scenes/RainbowColorsAndPrism/LuxCoreScene/render.cfg"
	filmWidth = 480
	filmHeight = 270

	additionalProps = pyluxcore.Properties()
	additionalProps.Set(pyluxcore.Property("film.width", filmWidth))
	additionalProps.Set(pyluxcore.Property("film.height", filmHeight))

	StandardSceneTest(cls, params, cfgFile, "RainbowColorsAndPrismLuxCoreTestScene", additionalProps)

class RainbowColorsAndPrismLuxCoreTestScene(ImageTest):
    pass

RainbowColorsAndPrismLuxCoreTestScene = AddTests(RainbowColorsAndPrismLuxCoreTestScene, TestRainbowColorsAndPrismLuxCoreTestScene, GetTestCases())
