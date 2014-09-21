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
from PIL import Image

import pyluxcore

class ImageTest(object):
	pass

def AddTests(cls, testFunc, opts):
	for input in opts:
		test = lambda self, i=input: testFunc(i)
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

def ConvertToImage(size, imageBufferFloat):
	imageBufferUChar = array('B', [int(v * 255.0 + 0.5) for v in imageBufferFloat])
	return Image.frombuffer("RGB", size, bytes(imageBufferUChar), "raw", "RGB", 0, 1).transpose(Image.FLIP_TOP_BOTTOM)

def StandardTest(name, config):
	size, imageBufferFloat = Render(config)
	image = ConvertToImage(size, imageBufferFloat)
	image.save("images/" + name + ".png")
