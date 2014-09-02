#!/usr/bin/python
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

#import os
#import resource
import time
import sys
from array import *
sys.path.append("./lib")

import pyluxcore

################################################################################
## Properties examples
################################################################################

def PropertiesTests():
	print("Properties examples...")
	prop = pyluxcore.Property("test1.prop1", "aa")
	print("test1.prop1 => %s\n" % prop.GetString(0))

	prop.Clear().Add([0, 2]).Add([3])
	prop.Set(0, 1)
	print("[%s]\n" % prop.ToString())

	print("[%s]\n" % pyluxcore.Property("test1.prop1").Add([1, 2, 3]).Add([1.0, 2.0, 3.0]))

	prop.Set([3, 2, 1])
	print("[%s]\n" % prop.ToString())

	pyvariable = 999
	prop = pyluxcore.Property("test1.prop1", [True, 1, 2.0, "aa", pyvariable])
	print("[%s]" % prop)
	print("Size: %d" % prop.GetSize())
	print("List: %s" % str(prop.Get()))
	print("[0]: %s" % prop.GetString())
	print("[1]: %d\n" % prop.GetInt(1))

	props = pyluxcore.Properties()
	props.SetFromString("test1.prop1 = 1 2.0 aa \"quoted\"\ntest2.prop2 = 1 2.0 'quoted' bb\ntest2.prop3 = 1")
	print("[\n%s]\n" % props)

	print("%s" % props.GetAllNames())
	print("%s" % props.GetAllNames("test1"))
	print("%s\n" % props.GetAllUniqueSubNames("test2"))

	props0 = pyluxcore.Properties()
	props1 = pyluxcore.Properties() \
		.Set(pyluxcore.Property("test1.prop1", [True, 1, 2.0, "aa"])) \
		.Set(pyluxcore.Property("test2.prop1", ["bb"]));

	props0.Set(props1, "prefix.")
	print("[\n%s]\n" % props0)

	print("Get: %s" % props0.Get("prefix.test1.prop1"))
	print("Get default: %s\n" % props0.Get("doesnt.exist", ["default_value0", "default_value1"]))

################################################################################
## LuxRays device information examples
################################################################################

def LuxRaysDeviceTests():
	print("LuxRays device information example......")
	deviceList = pyluxcore.GetOpenCLDeviceList()
	print("OpenCL device list: %s\n" % str(deviceList))

################################################################################
## RenderConfig and RenderSession examples
################################################################################

def SimpleRender():
	print("RenderConfig and RenderSession examples (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	while True:
		time.sleep(1)

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
	session.GetFilm().Save()

	print("Done.")

def getOutputTest():
	################################################################################
	## Film getOutput() examples
	################################################################################

	print("Film getOutput() example (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	# NOTICE THE DIFFERENT BEHAVIOR REQUIRED BY PYTHON 3.3, 2.7 and 2.6
	filmWidth, filmHeight = config.GetFilmSize()[:2]
	if sys.version_info < (2,7,0):
		imageBufferFloat = bytearray(filmWidth * filmHeight * 3 * 4)
		imageBufferUChar = bytearray(filmWidth * filmHeight * 4)
	elif sys.version_info < (3,0,0):
		imageBufferFloat = buffer(array('f', [0.0] * (filmWidth * filmHeight * 3)))
		imageBufferUChar = buffer(array('b', [0] * (filmWidth * filmHeight * 4)))
	else:
		imageBufferFloat = array('f', [0.0] * (filmWidth * filmHeight * 3))
		imageBufferUChar = array('b', [0] * (filmWidth * filmHeight * 4))

	session.Start()

	startTime = time.time()
	imageIndex = 0
	while True:
		time.sleep(1)

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

		# This is mostly for testing the PyLuxCore functionality, save an image every second

		# Update the image
		session.GetFilm().GetOutputFloat(pyluxcore.FilmOutputType.RGB_TONEMAPPED, imageBufferFloat)
		pyluxcore.ConvertFilmChannelOutput_3xFloat_To_4xUChar(filmWidth, filmHeight,
			imageBufferFloat, imageBufferUChar, False)

		# Save the imageBufferUChar buffer to a PPM file
		imageFileName = "image" + str(imageIndex) + ".ppm"
		print("Saving image file: " + imageFileName)
		f = open(imageFileName, "w")
		f.write("P3\n")
		f.write(str(filmWidth) + " " + str(filmHeight) + "\n")
		f.write("255\n")
		for i in range(0, len(imageBufferUChar), 4):
			f.write(str(imageBufferUChar[i + 2]) + " " + str(imageBufferUChar[i + 1]) + " " + str(imageBufferUChar[i]) + "\n")
		f.close()
		imageIndex += 1

		if elapsedTime > 5.0:
			# Time to stop the rendering
			break

	session.Stop()

	print("Done.")

################################################################################

def main():
	pyluxcore.Init()

	print("LuxCore %s" % pyluxcore.Version())
	#print("OS:", os.name)
	
	PropertiesTests()
	LuxRaysDeviceTests()
	SimpleRender()
	getOutputTest()

	#if (os.name == "posix"):
	#	print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
	
if __name__ == '__main__':
	main()
