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

from pyluxcoreunittests.tests.utils import *
from pyluxcoreunittests.tests.render import *

IMAGES_DIR = "images"
IMAGE_REFERENCE_DIR = "referenceimages"

class ImageTest(LuxCoreTest):
	pass

def CheckResult(testCase, image, name, isDeterministic, frame = -1):
	if frame >= 0:
		imageName = (name + "-%04d.png") % frame
	else:
		imageName = name + ".png"
		
	# Save the rendering
	resultImageName = IMAGES_DIR + "/" + imageName
	image.save(resultImageName)

	refImageName = IMAGE_REFERENCE_DIR + "/" + imageName
	
	CompareImageFiles(testCase, resultImageName, refImageName, isDeterministic)

def StandardImageTest(testCase, name, config, isDeterministic):
	session = DoRenderSession(config)
	image = GetImagePipelineImage(session.GetFilm())

	CheckResult(testCase, image, name, isDeterministic)

def StandardSceneTest(cls, params, cfgName, testName):
	engineType = params[0]
	samplerType = params[1]
	renderConfigAdditionalProps = params[2]
	isDeterministic = params[3]

	# Create the rendering configuration
	props = pyluxcore.Properties()
	props.SetFromFile("resources/scenes/" + cfgName)
	props.Set(renderConfigAdditionalProps)
	props.Set(LuxCoreTest.customConfigProps)
	
	# Set the rendering engine
	props.Set(pyluxcore.Property("renderengine.type", engineType))
	props.Set(pyluxcore.Property("sampler.type", samplerType))

	config = pyluxcore.RenderConfig(props)

	# Run the rendering
	StandardImageTest(cls, testName + "_" + engineType + ("" if not samplerType else ("_" + samplerType)), config, isDeterministic)

def StandardAnimTest(testCase, name, config, frameCount, isDeterministic):
	session = pyluxcore.RenderSession(config)

	i = 0
	print("\nFrame 0...")
	session.Start()

	while True:
		session.WaitForDone()

		# This is done also to update the Film
		session.UpdateStats()

		image = GetImagePipelineImage(session.GetFilm())

		CheckResult(testCase,image, name, isDeterministic, i)

		i += 1
		if i >= frameCount:
			break

		session.BeginSceneEdit()

		# Edit the scene
		testCase.SceneEdit(session, i)

		# Restart the rendering
		print("Frame %d..." % i)
		session.EndSceneEdit()
	
	session.Stop()
