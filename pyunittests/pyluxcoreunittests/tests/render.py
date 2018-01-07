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
import unittest
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

def DoRenderSession(config):
	session = pyluxcore.RenderSession(config)

	session.Start()
	session.WaitForDone()
	session.Stop()

	return session
