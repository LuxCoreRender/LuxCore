#!/usr/bin/python
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

#import os
#import resource
import random
import time
import sys
import os
from array import *
sys.path.append("./lib")

import pyluxcore

# First version: "scenes/simple/simple.cfg"
# Film save (2048 x 2048)
# - time: 9.173918 secs
# - size: 106 Mbytes
# Film load (2048 x 2048)
# - time: 4.278447 secs
#
# Same results with boost::serialization::make_array() in GenericFrameBuffer class
#
# Same results with explicit loop in GenericFrameBuffer class

# First version: "scenes/luxball/luxball-hdr.cfg"
# Film save (2048 x 2048)
# - time: 4.4472999999999985 secs
# - size: 52604 Kbytes
# Film load (2048 x 2048)
# - time: 2.0244070000000107 secs
#
# Removing the ConvTest:
# Film save (2048 x 2048)
# - time: 4.073249000000004 secs
# - size: 52208 Kbytes
# Film load (2048 x 2048)
# - time: 1.6987780000000043 secs

################################################################################
## Film save example
################################################################################

def SaveFilm():
	print("Film save example (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/simple/simple.cfg")
	#props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")
	# To test large films
	#props.Set(pyluxcore.Property("film.width", 2048))
	#props.Set(pyluxcore.Property("film.height", 2048))

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	while True:
		time.sleep(.5)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		print("[Elapsed time: %3d/5sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))

		if elapsedTime > 5.0:
			# Time to stop the rendering
			break

	session.Stop()

	# Save the rendered image
	t1 = time.perf_counter()
	session.GetFilm().SaveFilm("simple.flm")
	t2 = time.perf_counter()
	print("Film save time: %s secs" % (t2 - t1))

	print("Done.")
	
################################################################################
## Film load example
################################################################################

def LoadFilm():
	print("Film loading...")
	t1 = time.perf_counter()
	film = pyluxcore.Film("simple.flm")
	t2 = time.perf_counter()
	print("Film load time: %s secs" % (t2 - t1))

	# Define the new image pipeline
	props = pyluxcore.Properties()
	props.SetFromString("""
		film.imagepipeline.0.type = TONEMAP_LINEAR
		film.imagepipeline.0.scale = 1.0
		film.imagepipeline.1.type = CAMERA_RESPONSE_FUNC
		film.imagepipeline.1.name = Ektachrome_320TCD
		film.imagepipeline.2.type = GAMMA_CORRECTION
		film.imagepipeline.2.value = 2.2
		""")
	film.Parse(props)

	# Save the tonemapped AOV
	print("RGB_TONEMAPPED saving...")
	film.SaveOutput("simple.png", pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())
	
	# Animate radiance groups
	for i in range(0, 20):
		props = pyluxcore.Properties()
		props.SetFromString("""
			film.radiancescales.0.globalscale = """ + str(0.025 + (19 - i) * (3.0 / 19.0)) + """
			film.radiancescales.1.globalscale = """ + str(0.025 + i * (3.0 / 19.0)) + """
			""")
		film.Parse(props)
		
		print("Frame " + str(i) + " saving...")
		film.SaveOutput("simple" + str(i) + ".png", pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())
	
	print("Film saving...")
	t1 = time.perf_counter()
	film.SaveFilm("simple2.flm")
	t2 = time.perf_counter()
	print("Film save time: %s secs" % (t2 - t1))

	print("Done.")
	

################################################################################

def main():
	pyluxcore.Init()

	print("LuxCore %s" % pyluxcore.Version())
	#print("OS:", os.name)
	
	SaveFilm()
	LoadFilm()

	#if (os.name == "posix"):
	#	print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
	
if __name__ == '__main__':
	main()
