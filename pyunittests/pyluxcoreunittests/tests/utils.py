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

from array import *
import unittest
import pyluxcore

class LuxCoreTest(unittest.TestCase):
	customConfigProps = pyluxcore.Properties()

def AddTests(cls, testFunc, opts):
	for input in opts:
		test = lambda self, i=input: testFunc(self, i)
		test.__name__ = "test_%s_%s" % (cls.__name__, input)
		setattr(cls, test.__name__, test)
	return cls

def Render(config):
	session = pyluxcore.RenderSession(config)

	session.Start()
	session.WaitForDone()
	session.Stop()

	# Get the rendering result
	film = session.GetFilm()
	imageBufferFloat = array('f', [0.0] * (film.GetWidth() * film.GetHeight() * 3))
	session.GetFilm().GetOutputFloat(pyluxcore.FilmOutputType.RGB_TONEMAPPED, imageBufferFloat)

	return (film.GetWidth(), film.GetHeight()), imageBufferFloat

def GetEngineList():
	return ["PATHCPU", "BIDIRCPU", "BIASPATHCPU", "PATHOCL", "BIASPATHOCL"]

engineProperties = {
	"PATHCPU" : pyluxcore.Properties().SetFromString(
		"""renderengine.type = PATHCPU
		batch.haltdebug = 1"""),
	"BIDIRCPU" : pyluxcore.Properties().SetFromString(
		"""renderengine.type = BIDIRCPU
		batch.haltdebug = 1"""),
	"BIASPATHCPU" : pyluxcore.Properties().SetFromString(
		# NOTE: native.threads.count = 1 is required otherwise BIASPATHCPU is not deterministic
		"""renderengine.type = BIASPATHCPU
		batch.haltdebug = 1
		native.threads.count = 1
		"""),
	"PATHOCL" : pyluxcore.Properties().SetFromString(
		# NOTE: path.pixelatomics.enable = 1 is required otherwise PATHOCL is not deterministic
		"""renderengine.type = PATHOCL
		batch.haltdebug = 64
		path.pixelatomics.enable = 1"""),
	"BIASPATHOCL" : pyluxcore.Properties().SetFromString(
		# NOTE: BIASPATHOCL is deterministic only when used with a single OpenCL device
		"""renderengine.type = BIASPATHOCL
		batch.haltdebug = 1"""),
}

def GetEngineProperties(engineType):
	return engineProperties[engineType]
