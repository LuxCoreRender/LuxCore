# -*- coding: utf-8 -*-
################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
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
from PIL import Image, ImageChops

from pyluxcoreunittests.tests.utils import *

IMAGES_DIR = "images"
IMAGE_REFERENCE_DIR = "referenceimages"

class ImageTest(LuxCoreTest):
	pass

def ConvertToImage(size, imageBufferFloat):
	imageBufferUChar = array('B', [int(v * 255.0 + 0.5) for v in imageBufferFloat])
	return Image.frombuffer("RGB", size, bytes(imageBufferUChar), "raw", "RGB", 0, 1).transpose(Image.FLIP_TOP_BOTTOM)

def ImageDiff(a, b):
	diff = ImageChops.difference(a, b)
	diff = diff.convert('L')
	# Map all no black pixels
	pointTable = ([0] + ([255] * 255))
	diff = diff.point(pointTable)
	new = diff.convert('RGB')
	new.paste(b, mask=diff)
	return new

def ImageEquals(a, b):
	return ImageChops.difference(a, b).getbbox() is None

def StandardImageTest(testCase, name, config):
	size, imageBufferFloat = Render(config)
	image = ConvertToImage(size, imageBufferFloat)

	# Save the rendering
	imageName = IMAGES_DIR + "/" + name + ".png" 
	image.save(imageName)

	# Check if there is a reference image
	refImageName = IMAGE_REFERENCE_DIR + "/" + name + ".png" 
	if os.path.isfile(refImageName):
		# Read the reference image
		refImage = Image.open(refImageName)
		# Check if there is a difference
		if not ImageEquals(image, refImage):
			# Save the differences
			ImageDiff(image, refImage).save(IMAGES_DIR + "/diff-" + name + ".png")
			testCase.fail(refImageName + " differs from " + imageName)
	else:
		# I'm missing the reference image
		print("\nWARNING: missing reference image \"" + refImageName + "\"")
		
