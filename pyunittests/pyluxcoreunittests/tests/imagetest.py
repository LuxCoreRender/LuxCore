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

import os
import shutil
from PIL import Image, ImageChops

from pyluxcoreunittests.tests.utils import *

IMAGES_DIR = "images"
IMAGE_REFERENCE_DIR = "referenceimages"

class ImageTest(LuxCoreTest):
	pass

def ConvertToImage(size, imageBufferFloat):
	imageBufferUChar = array('B', [int(v * 255.0 + 0.5) for v in imageBufferFloat])
	return Image.frombuffer("RGB", size, bytes(imageBufferUChar), "raw", "RGB", 0, 1).transpose(Image.FLIP_TOP_BOTTOM)

def CompareImage(a, b):
	diff = ImageChops.difference(a, b)
	diff = diff.convert('L')
	# Map all no black pixels
	pointTable = ([0] + ([255] * 255))
	diff = diff.point(pointTable)
	
	(width, height) = diff.size
	count = 0
	for y in range(height):
		for x in range(width):
			if diff.getpixel((x, y)) != 0:
				count += 1

	if count > 0:
		new = diff.convert('RGB')
		new.paste(b, mask=diff)
		return False, count, new
	else:
		return True, 0, None
	

def CompareResult(testCase, image, name, isDeterministic, frame = -1):
	if frame >= 0:
		imageName = (name + "-%04d.png") % frame
	else:
		imageName = name + ".png"
		
	# Save the rendering
	resultImageName = IMAGES_DIR + "/" + imageName
	image.save(resultImageName)

	refImageName = IMAGE_REFERENCE_DIR + "/" + imageName
	if os.path.isfile(refImageName):
		# Read the reference image
		refImage = Image.open(refImageName)
		# Check if there is a difference
		(sameImage, diffCount, diffImage) = CompareImage(image, refImage)
		if not sameImage:
			# Save the differences
			diffImage.save(IMAGES_DIR + "/diff-" + imageName)
			
			# Fire the error only for deterministic renderings
			if isDeterministic:
				testCase.fail(refImageName + " differs from " + resultImageName + " in " + str(diffCount) + " pixels")
			else:
				# Fire the warning only if the difference is really huge
				if diffCount > (image.size[0] * image.size[1]) / 2:
					print("\nWARNING: " + str(diffCount) +" different pixels from reference image in: \"" + resultImageName + "\"")
	else:
		# Copy the current image as reference
		print("\nWARNING: missing reference image \"" + refImageName + "\". Copying the current result as reference.")
		shutil.copyfile(resultImageName, refImageName)

def StandardImageTest(testCase, name, config, isDeterministic):
	size, imageBufferFloat = Render(config)
	image = ConvertToImage(size, imageBufferFloat)

	CompareResult(testCase, image, name, isDeterministic)

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

		size, imageBufferFloat = GetRendering(session)
		image = ConvertToImage(size, imageBufferFloat)

		CompareResult(testCase,image, name, isDeterministic, i)

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
