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

import os
import shutil
from array import *
import unittest
from PIL import Image, ImageChops

import pyluxcore

class LuxCoreTest(unittest.TestCase):
	customConfigProps = pyluxcore.Properties()

def LuxCoreHasOpenCL():
	return not pyluxcore.GetPlatformDesc().Get("compile.LUXRAYS_DISABLE_OPENCL").GetBool()

def AddTests(cls, testFunc, opts):
	for input in opts:
		test = lambda self, i=input: testFunc(self, i)
		if isinstance(input, tuple):
			paramName = ""
			for i in [input[0], input[1]]:
				if i:
					paramName += ("" if paramName == "" else "_") + str(i)
		else:
			paramName = str(input)
		test.__name__ = "test_%s_%s" % (cls.__name__, paramName)
		setattr(cls, test.__name__, test)
	return cls

engineProperties = {
	"PATHCPU" : pyluxcore.Properties().SetFromString(
		"""
		native.threads.count = 1
		batch.haltdebug = 8
		"""),
	"BIDIRCPU" : pyluxcore.Properties().SetFromString(
		"""
		native.threads.count = 1
		batch.haltdebug = 8
		"""),
	"TILEPATHCPU" : pyluxcore.Properties().SetFromString(
		"""
		native.threads.count = 1
		batch.haltdebug = 1
		tilepath.sampling.aa.size = 3
		"""),
	"PATHOCL" : pyluxcore.Properties().SetFromString(
		"""
		batch.haltdebug = 16
		opencl.cpu.use = 1
		opencl.gpu.use = 0
		"""),
	"TILEPATHOCL" : pyluxcore.Properties().SetFromString(
		"""
		batch.haltdebug = 1
		tilepath.sampling.aa.size = 3
		opencl.cpu.use = 1
		opencl.gpu.use = 0
		"""),
}

def GetDefaultEngineProperties(engineType):
	return engineProperties[engineType]

# The tuple is:
#  (<engine name>, <sampler name>, <render config additional properties>,
#   <deterministic rendering>)
def GetTestCases():
	el = [
		("PATHCPU", "RANDOM", GetDefaultEngineProperties("PATHCPU"), False),
		("PATHCPU", "SOBOL", GetDefaultEngineProperties("PATHCPU"), False),
		("PATHCPU", "METROPOLIS", GetDefaultEngineProperties("PATHCPU"), False),
		("BIDIRCPU", "RANDOM", GetDefaultEngineProperties("BIDIRCPU"), False),
		("BIDIRCPU", "SOBOL", GetDefaultEngineProperties("BIDIRCPU"), False),
		("BIDIRCPU", "METROPOLIS", GetDefaultEngineProperties("BIDIRCPU"), False),
		("TILEPATHCPU", "TILEPATHSAMPLER", GetDefaultEngineProperties("TILEPATHCPU"), False)
	]
	
	if LuxCoreHasOpenCL():
		el += [
		("PATHOCL", "RANDOM", GetDefaultEngineProperties("PATHOCL"), False),
		("PATHOCL", "SOBOL", GetDefaultEngineProperties("PATHOCL"), False),
		("PATHOCL", "METROPOLIS", GetDefaultEngineProperties("PATHOCL"), False),
		("TILEPATHOCL", "TILEPATHSAMPLER", GetDefaultEngineProperties("TILEPATHOCL"), False)
		]

	return el

def ConvertToImage(size, imageBufferFloat):
	imageBufferUChar = array('B', [max(0, min(255, int(v * 255.0 + 0.5))) for v in imageBufferFloat])
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
	
def CompareImageFiles(testCase, resultImageName, refImageName, isDeterministic = False):
	if os.path.isfile(refImageName):
		# Read the result image
		resultImage = Image.open(resultImageName)
		# Read the reference image
		refImage = Image.open(refImageName)

		# Check if there is a difference
		(sameImage, diffCount, diffImage) = CompareImage(resultImage, refImage)

		if not sameImage:
			# Save the differences
			(head, tail) = os.path.split(resultImageName)
			diffImage.save(head + "/diff-" + tail)
			
			# Fire the error only for deterministic renderings
			if isDeterministic:
				testCase.fail(refImageName + " differs from " + resultImageName + " in " + str(diffCount) + " pixels")
			else:
				# Fire the warning only if the difference is really huge
				if diffCount > (resultImage.size[0] * resultImage.size[1]) / 2:
					print("\nWARNING: " + str(diffCount) +" different pixels from reference image in: \"" + resultImageName + "\"")
	else:
		# Copy the current image as reference
		print("\nWARNING: missing reference image \"" + refImageName + "\". Copying the current result as reference.")
		shutil.copyfile(resultImageName, refImageName)
