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

from array import *
import time
import pyluxcore

from pyluxcoreunittests.tests.utils import *

def GetImagePipelineBuffer(film):
	# Get the rendering result
	imageBufferFloat = array('f', [0.0] * (film.GetWidth() * film.GetHeight() * 3))
	film.GetOutputFloat(pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, imageBufferFloat)

	return imageBufferFloat

def GetImagePipelineImage(film):
	size = (film.GetWidth(), film.GetHeight())
	image = ConvertToImage(size, GetImagePipelineBuffer(film))

	return image

def DoRenderSessionWaitForDone(session):
	lastPrint = time.time()
	while not session.HasDone():
		time.sleep(0.2)
		# Halt conditions are checked inside UpdateStats
		session.UpdateStats()
		
		if time.time() - lastPrint > 5.0:
			stats = session.GetStats();
			elapsedTime = stats.Get("stats.renderengine.time").GetFloat();
			currentPass = stats.Get("stats.renderengine.pass").GetInt();
			# Convergence test is update inside UpdateFilm()
			convergence = stats.Get("stats.renderengine.convergence").GetFloat();

			print("[Elapsed time: %3dsec][Samples %4d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
					elapsedTime,
					currentPass,
					100.0 * convergence,
					stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0,
					stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0))
			lastPrint = time.time()

def DoRenderSession(config):
	session = pyluxcore.RenderSession(config)

	session.Start()

	print("")
	DoRenderSessionWaitForDone(session)

	session.Stop()

	return session
