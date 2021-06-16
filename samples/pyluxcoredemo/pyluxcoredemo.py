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

# Choose the right timing function depending on Python version
def Clock():
	if sys.version_info < (3, 3, 0):
		return time.clock()
	else:
		return time.perf_counter()

################################################################################
## Properties example
################################################################################

def PropertiesTests():
	print("Properties examples...")
	prop = pyluxcore.Property("test1.prop1", 0)
	prop.Set(0, 3705339624)

	prop = pyluxcore.Property("test1.prop1", 3705339624)
	print("test1.prop1 => %s\n" % prop.GetInt(0))

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

	blob = bytearray(100)
	for i in range(0, len(blob)):
		blob[i] = i
	prop = pyluxcore.Property("test.blob", [blob])
	prop.Add([[1, 2, 3]])

	blob2 = prop.GetBlob()
	print("Blob [0]:", end="")
	for i in range(0, len(blob2)):
		print(" %d" % blob2[i], end="")
	print("")

	blob2 = prop.GetBlob(1)
	print("Blob [1]:", end="")
	for i in range(0, len(blob2)):
		print(" %d" % blob2[i], end="")
	print("\n")

	a = array('f', [1.0, 2.0, 3.0])
	prop = pyluxcore.Property("test.array", [])
	prop.AddAllFloat(a)
	print("Array: ", end="")
	for i in range(3):
		print(" %f" % prop.GetFloat(i), end="")
	print("")

	a =[1.0, 2.0, 3.0]
	prop = pyluxcore.Property("test.array", [])
	prop.AddAllFloat(a)
	print("List: ", end="")
	for i in range(3):
		print(" %f" % prop.GetFloat(i), end="")
	print("")

	print("")
	size = 2000000
	a = array('f', [i for i in range(size)])
	prop = pyluxcore.Property("test.array", [])
	
	start = Clock()
	for i in range(size):
		prop.Add([a[i]])
	end = Clock()
	print("Add test: %.2gs" % (end-start))

	prop = pyluxcore.Property("test.array", [])
	
	start = Clock()
	prop.AddAllFloat(a)
	end = Clock()
	print("AddAll test: %.2gs" % (end-start))
	
	prop = pyluxcore.Property("test.array", [])
	
	start = Clock()
	prop.AddAllFloat(a, 3, 1)
	end = Clock()
	print("AddAllStride test: %.2gs" % (end-start))

################################################################################
## LuxRays device information example
################################################################################

def LuxRaysDeviceTests():
	print("LuxRays device information example......")
	deviceList = pyluxcore.GetOpenCLDeviceList()
	print("OpenCL device list: %s\n" % str(deviceList))

################################################################################
## RenderConfig and RenderSession example
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

################################################################################
## Film getOutput() example
################################################################################

def GetOutputTest():
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
		imageBufferUChar = buffer(array('B', [0] * (filmWidth * filmHeight * 4)))
	else:
		imageBufferFloat = array('f', [0.0] * (filmWidth * filmHeight * 3))
		imageBufferUChar = array('B', [0] * (filmWidth * filmHeight * 4))

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
		session.GetFilm().GetOutputFloat(pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, imageBufferFloat)
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
## Extracting Film configuration example
################################################################################

def ExtractConfiguration():
	print("Extracting Film configuration example (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr-comp.cfg")

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	# Extract the RenderConfig properties (redundant here)
	props = session.GetRenderConfig().GetProperties()
	
	ids = set()
	for i in props.GetAllUniqueSubNames("film.outputs"):
		if props.Get(i + ".type").GetString() == "MATERIAL_ID_MASK":
			ids.add(props.Get(i + ".id").GetInt())

	for i in ids:
		print("MATERIAL_ID_MASK ID => %d" % i)

	print("Done.")

################################################################################
## Strands render example
################################################################################

def BuildPlane(objectName, materialName):
	prefix = "scene.objects." + objectName + "."
	props = pyluxcore.Properties()
	props.SetFromString(
	prefix + "material = " + materialName + "\n" +
	prefix + "vertices = -1.0 -1.0 0.0  -1.0 1.0 0.0  1.0 1.0 0.0  1.0 -1.0 0.0\n" +
	prefix + "faces = 0 1 2  2 3 0\n" +
	prefix + "uvs = 0.0 0.0  0.0 1.0  1.0 1.0  1.0 0.0\n"
	)
	
	return props

def StrandsRender():
	print("Strands example...")

	# Create the rendering configuration
	cfgProps = pyluxcore.Properties()
	cfgProps.SetFromString("""
		film.width = 640
		film.height = 480
		""")

	# Set the rendering engine
	cfgProps.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	# Create the scene properties
	scnProps = pyluxcore.Properties()
	
	# Set the camera position
	scnProps.SetFromString("""
		scene.camera.lookat.orig = 0.0 5.0 2.0
		scene.camera.lookat.target = 0.0 0.0 0.0
		""")
	
	# Define a white matte material
	scnProps.SetFromString("""
		scene.materials.whitematte.type = matte
		scene.materials.whitematte.kd = 0.75 0.75 0.75
		""")

	# Add a plane
	scnProps.Set(BuildPlane("plane1", "whitematte"))
	
	# Add a distant light source
	scnProps.SetFromString("""
		scene.lights.distl.type = sharpdistant
		scene.lights.distl.color = 1.0 1.0 1.0
		scene.lights.distl.gain = 2.0 2.0 2.0
		scene.lights.distl.direction = 1.0 1.0 -1.0
		""")
	
	# Create the scene
	#resizePolicyProps = pyluxcore.Properties()
	#resizePolicyProps.SetFromString("""
	#	scene.images.resizepolicy.type = "MIPMAPMEM"
	#	scene.images.resizepolicy.scale = 1.0
	#	scene.images.resizepolicy.minsize = 64
	#	""")
	#scene = pyluxcore.Scene(scnProps, resizePolicyProps)

	scene = pyluxcore.Scene()
	scene.Parse(scnProps)

	# Add strands
	points = []
	segments = []
	strandsCount = 30
	for i in range(strandsCount):
		x = random.random() * 2.0 - 1.0
		y = random.random() * 2.0 - 1.0
		points.append((x , y, 0.0))
		points.append((x , y, 1.0))
		segments.append(1)

	scene.DefineStrands("strands_shape", strandsCount, 2 * strandsCount, points, segments,
		0.025, 0.0, (1.0, 1.0, 1.0), None, "ribbon",
		0, 0, 0, False, False, True)
		
	strandsProps = pyluxcore.Properties()
	strandsProps.SetFromString("""
		scene.objects.strands_obj.material = whitematte
		scene.objects.strands_obj.shape = strands_shape
		""")
	scene.Parse(strandsProps)

	# Save the strand mesh (just for testing)
	scene.SaveMesh("strands_shape", "strands_shape.ply")

	# Create the render config
	config = pyluxcore.RenderConfig(cfgProps, scene)
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

################################################################################
## Image pipeline editing example
################################################################################

def ImagePipelineEdit():
	print("Image pipeline editing examples (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	imageSaved = False
	while True:
		time.sleep(1)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		print("[Elapsed time: %3d/10sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))

		if elapsedTime > 5.0 and not imageSaved:
			session.GetFilm().Save()
			os.rename("normal.png", "normal-edit1.png")
			
			# Define the new image pipeline
			props = pyluxcore.Properties()
			props.SetFromString("""
				film.imagepipeline.0.type = TONEMAP_REINHARD02
				film.imagepipeline.1.type = CAMERA_RESPONSE_FUNC
				film.imagepipeline.1.name = Ektachrome_320TCD
				film.imagepipeline.2.type = GAMMA_CORRECTION
				film.imagepipeline.2.value = 2.2
				""")
			session.Parse(props)

			# Change radiance group scale
#			props = pyluxcore.Properties()
#			props.SetFromString("""
#				film.radiancescales.0.rgbscale = 1.0 0.0 0.0
#				""")
#			session.Parse(props)

			imageSaved = True

		if elapsedTime > 10.0:
			# Time to stop the rendering
			break

	session.Stop()

	# Save the rendered image
	session.GetFilm().Save()
	os.rename("normal.png", "normal-edit2.png")

	print("Done.")

################################################################################
## Save and Resume example (with multiple files)
################################################################################

def SaveResumeRenderingM():
	print("Save and Resume example (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	for i in range(0, 3):
		time.sleep(1)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		print("[Elapsed time: %3d/3sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
	
	session.Pause()
	
	session.GetFilm().SaveOutput("test-save.png", pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())

	# Save the film
	session.GetFilm().SaveFilm("test.flm")
	
	# Save the render state
	renderState = session.GetRenderState()
	renderState.Save("test.rst")
	
	# Save the rendered image
	session.GetFilm().Save()
	
	# Stop the rendering
	session.Stop()
	
	# Resume rendering
	print("Resume rendering")
	
	session = pyluxcore.RenderSession(config, "test.rst", "test.flm")
	
	session.Start()

	startTime = time.time()
	for i in range(0, 3):
		time.sleep(1)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		print("[Elapsed time: %3d/3sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
	
	session.GetFilm().SaveOutput("test-resume.png", pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())
	session.Stop()
	
	print("Done.")

################################################################################
## Save and Resume example (with a single file)
################################################################################

def SaveResumeRenderingS():
	print("Save and Resume example (requires scenes directory)...")

	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

	# Change the render engine to PATHCPU
	props.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	for i in range(0, 3):
		time.sleep(1)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		print("[Elapsed time: %3d/3sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
	
	session.Pause()
	
	session.GetFilm().SaveOutput("test-save.png", pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())

	# Save the session resume file
	session.SaveResumeFile("test.rsm")

	# Save the rendered image
	session.GetFilm().Save()
	
	# Stop the rendering
	session.Stop()
	
	# Resume rendering
	print("Resume rendering")

	(config, startState, startFilm) = pyluxcore.RenderConfig.LoadResumeFile("test.rsm")
	session = pyluxcore.RenderSession(config, startState, startFilm)
	
	session.Start()

	startTime = time.time()
	for i in range(0, 3):
		time.sleep(1)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		print("[Elapsed time: %3d/3sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))
	
	session.GetFilm().SaveOutput("test-resume.png", pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())
	session.Stop()
	
	print("Done.")

################################################################################

def main():
	pyluxcore.Init()

	print("LuxCore %s" % pyluxcore.Version())
	#print("OS:", os.name)
	
	#PropertiesTests()
	#LuxRaysDeviceTests()
	#SimpleRender()
	#GetOutputTest()
	#ExtractConfiguration()
	StrandsRender()
	#ImagePipelineEdit()
	#SaveResumeRenderingM()
	#SaveResumeRenderingS()

	#if (os.name == "posix"):
	#	print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
	
if __name__ == '__main__':
	main()
