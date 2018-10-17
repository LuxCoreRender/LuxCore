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
import subprocess
import shutil
from array import *
import unittest
from PIL import Image, ImageChops

import pyluxcore

import logging
logger = logging.getLogger("pyunittests")

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
		film.width = 150
		film.height = 100
		batch.haltthreshold = 0.075
		"""),
	"BIDIRCPU" : pyluxcore.Properties().SetFromString(
		"""
		film.width = 150
		film.height = 100
		batch.haltthreshold = 0.075
		"""),
	"TILEPATHCPU" : pyluxcore.Properties().SetFromString(
		"""
		film.width = 150
		film.height = 100
		batch.haltthreshold = 0.075
		tilepath.sampling.aa.size = 2
		"""),
	"PATHOCL" : pyluxcore.Properties().SetFromString(
		"""
		film.width = 150
		film.height = 100
		batch.haltthreshold = 0.075
		"""),
	"TILEPATHOCL" : pyluxcore.Properties().SetFromString(
		"""
		film.width = 150
		film.height = 100
		batch.haltthreshold = 0.075
		tilepath.sampling.aa.size = 2
		"""),
}

def GetDefaultEngineProperties(engineType):
	return engineProperties[engineType]

# The tuple is:
#  (<engine name>, <sampler name>, <render config additional properties>)
USE_SUBSET = False
def GetTestCases():
	el = [
		("PATHCPU", "RANDOM", GetDefaultEngineProperties("PATHCPU")),
	]

	if (not USE_SUBSET):
		el += [
			("BIDIRCPU", "RANDOM", GetDefaultEngineProperties("BIDIRCPU")),
			("TILEPATHCPU", "TILEPATHSAMPLER", GetDefaultEngineProperties("TILEPATHCPU")),
			("PATHCPU", "SOBOL", GetDefaultEngineProperties("PATHCPU")),
			("PATHCPU", "METROPOLIS", GetDefaultEngineProperties("PATHCPU")),
			("BIDIRCPU", "SOBOL", GetDefaultEngineProperties("BIDIRCPU")),
			("BIDIRCPU", "METROPOLIS", GetDefaultEngineProperties("BIDIRCPU")),
		]
	
		if LuxCoreHasOpenCL():
			el += [
			("PATHOCL", "RANDOM", GetDefaultEngineProperties("PATHOCL")),
			("PATHOCL", "SOBOL", GetDefaultEngineProperties("PATHOCL")),
			("PATHOCL", "METROPOLIS", GetDefaultEngineProperties("PATHOCL")),
			("TILEPATHOCL", "TILEPATHSAMPLER", GetDefaultEngineProperties("TILEPATHOCL"))
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
	
def CompareImageFiles(testCase, resultImageName, refImageName):
	if os.path.isfile(refImageName):
		sameImage = (subprocess.call("../deps/perceptualdiff-master/perceptualdiff --down-sample 2 \'" + resultImageName + "\' \'" + refImageName + "\'", shell=True) == 0)

		if not sameImage:
			# Read the result image
			resultImage = Image.open(resultImageName)
			# Read the reference image
			refImage = Image.open(refImageName)

			# Check if there is a difference
			(_, _, diffImage) = CompareImage(resultImage, refImage)

			# Save the differences
			(head, tail) = os.path.split(resultImageName)
			diffImage.save(head + "/diff-" + tail)

			testCase.fail(refImageName + " differs from " + resultImageName)
	else:
		# Copy the current image as reference
		logger.info("\nWARNING: missing reference image \"" + refImageName + "\". Copying the current result as reference.")
		shutil.copyfile(resultImageName, refImageName)
