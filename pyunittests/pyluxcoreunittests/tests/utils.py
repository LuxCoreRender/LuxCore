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

from array import *
import unittest
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

def GetRendering(session):
	# Get the rendering result
	film = session.GetFilm()
	imageBufferFloat = array('f', [0.0] * (film.GetWidth() * film.GetHeight() * 3))
	session.GetFilm().GetOutputFloat(pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, imageBufferFloat)

	return (film.GetWidth(), film.GetHeight()), imageBufferFloat

def Render(config):
	session = pyluxcore.RenderSession(config)

	session.Start()
	session.WaitForDone()
	session.Stop()

	return GetRendering(session)

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
