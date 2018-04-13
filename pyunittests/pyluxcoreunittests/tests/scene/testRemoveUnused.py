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

import time
import unittest
import pyluxcore

from pyluxcoreunittests.tests.utils import *
from pyluxcoreunittests.tests.imagetest import *

class TestRemoveUnused(unittest.TestCase):
	def RunRemoveUnusedTest(self, name, removeImageMaps, removeUnusedTexs, removeUnusedMats, removeUnusedMeshes, editProps):
		# Load the configuration from file
		props = pyluxcore.Properties("resources/scenes/simple/texture-imagemap.cfg")

		# Change the render engine to PATHCPU
		props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))
		props.Set(pyluxcore.Property("sampler.type", ["RANDOM"]))
		props.Set(GetDefaultEngineProperties("PATHCPU"))
		# Delete halt condition
		props.Delete("batch.haltthreshold")

		config = pyluxcore.RenderConfig(props)
		session = pyluxcore.RenderSession(config)

		session.Start()
		time.sleep(2.0)
		session.BeginSceneEdit()

		scene = config.GetScene()
		scene.Parse(editProps)

		if (removeImageMaps):
			scene.RemoveUnusedImageMaps();
		if (removeUnusedTexs):
			scene.RemoveUnusedTextures();
		if (removeUnusedMats):
			scene.RemoveUnusedMaterials();
		if (removeUnusedMeshes):
			scene.RemoveUnusedMeshes();

		session.EndSceneEdit()
		time.sleep(5.0)
		session.Stop()
		
		image = GetImagePipelineImage(session.GetFilm())


		CheckResult(self, image, name, False)

	def test_Scene_RemoveUnusedImageMaps(self):
		editProps = pyluxcore.Properties()
		editProps.SetFromString("""
			scene.textures.imagemap.type = constfloat3
			scene.textures.imagemap.value = 0.75 0.0 0.0
			""")

		self.RunRemoveUnusedTest("Scene_RemoveUnusedImageMaps", True, False, False, False, editProps)

	def test_Scene_RemoveUnusedTextures(self):
		editProps = pyluxcore.Properties()
		editProps.SetFromString("""
			scene.textures.imagemap.type = constfloat3
			scene.textures.imagemap.value = 0.0 0.75 0.0
			""")

		self.RunRemoveUnusedTest("Scene_RemoveUnusedTextures", False, True, False, False, editProps)

	def test_Scene_RemoveUnusedMaterials(self):
		editProps = pyluxcore.Properties()
		editProps.SetFromString("""
			scene.materials.mat_sphere.type = matte
			scene.materials.mat_sphere.kd = 0.0 0.0 0.75
			""")

		self.RunRemoveUnusedTest("Scene_RemoveUnusedMaterials", False, False, True, False, editProps)

	def test_Scene_RemoveUnusedMeshes(self):
		editProps = pyluxcore.Properties()
		editProps.SetFromString("""
			scene.objects.box2.ply = resources/scenes/simple/simple-mat-cube1.ply
			scene.objects.box2.material = mat_sphere
			""")

		self.RunRemoveUnusedTest("Scene_RemoveUnusedMeshes", False, False, False, True, editProps)

	def test_Scene_RemoveUnusedAll(self):
		editProps = pyluxcore.Properties()
		editProps.SetFromString("""
			scene.textures.imagemap.type = constfloat3
			scene.textures.imagemap.value = 0.0 0.75 0.0
			scene.materials.mat_sphere.type = matte
			scene.materials.mat_sphere.kd = 0.0 0.0 0.75
			scene.objects.box2.ply = resources/scenes/simple/simple-mat-cube1.ply
			scene.objects.box2.material = mat_sphere
			""")

		self.RunRemoveUnusedTest("Scene_RemoveUnusedAll", True, True, True, True, editProps)

