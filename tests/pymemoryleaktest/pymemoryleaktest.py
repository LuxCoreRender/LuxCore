#!/usr/bin/python
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

import os
import gc
import resource
import time
import sys
from array import *
sys.path.append("./lib")

import pyluxcore

def PropertiesOps():
	prop = pyluxcore.Property("test1.prop1", "aa")

	prop.Clear().Add([0, 2]).Add([3])
	prop.Set(0, 1)
	prop.Set([3, 2, 1])

	pyvariable = 999
	prop = pyluxcore.Property("test1.prop1", [True, 1, 2.0, "aa", pyvariable])

	props = pyluxcore.Properties()
	props.SetFromString("test1.prop1 = 1 2.0 aa \"quoted\"\ntest2.prop2 = 1 2.0 'quoted' bb\ntest2.prop3 = 1")

	props0 = pyluxcore.Properties()
	props1 = pyluxcore.Properties() \
		.Set(pyluxcore.Property("test1.prop1", [True, 1, 2.0, "aa"])) \
		.Set(pyluxcore.Property("test2.prop1", ["bb"]));

	props0.Set(props1, "prefix.")

def SceneOps():
	scene = pyluxcore.Scene("scenes/luxball/luxball-hdr.scn", 1.0)

def RenderConfigOps():
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")
	config = pyluxcore.RenderConfig(props)

def SimpleRender():
	# Load the configuration from file
	props = pyluxcore.Properties("scenes/luxball/luxball-hdr.cfg")

	# Change the render engine
	props.Set(pyluxcore.Property("renderengine.type", ["PATHOCL"]))
	props.Set(pyluxcore.Property("opencl.devices.select", ["01000"]))
	props.Set(pyluxcore.Property("film.hw.enable", ["0"]))
	props.Set(pyluxcore.Property("opencl.native.threads.count", [0]))

	config = pyluxcore.RenderConfig(props)
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	while True:
		time.sleep(1)

		elapsedTime = time.time() - startTime

		if elapsedTime > 1.0:
			# Time to stop the rendering
			break

	session.Stop()

	# Save the rendered image
	#session.GetFilm().Save()


################################################################################

def MemoryUsage():
    import psutil
    process = psutil.Process(os.getpid())
    mem = process.memory_info()[0]
    return mem

def PropertiesTest():
	print("-------------------------------------------------------------------------------------")
	print("PropertiesTest")
	print("-------------------------------------------------------------------------------------")

	startMemory = MemoryUsage()
	print("Start memory usage:", startMemory / 1024, "Kbytes")

	for i in range(1, 101):
		PropertiesOps()
		gc.collect()
		endMemory = MemoryUsage()
		if (i % 10 == 0):
			print("Step", i, " memory usage:", endMemory / 1024, "Kbytes [Delta:", str(endMemory - startMemory) + "]")
	
	print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)

def SceneTest():
	print("-------------------------------------------------------------------------------------")
	print("SceneTest")
	print("-------------------------------------------------------------------------------------")

	startMemory = MemoryUsage()
	print("Start memory usage:", startMemory / 1024, "Kbytes")

	for i in range(1, 101):
		SceneOps()
		gc.collect()
		endMemory = MemoryUsage()
		if (i % 10 == 0):
			print("Step", i, " memory usage:", endMemory / 1024, "Kbytes [Delta:", str(endMemory - startMemory) + "]")
	
	print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)

def RenderConfigTest():
	print("-------------------------------------------------------------------------------------")
	print("RenderConfigTest")
	print("-------------------------------------------------------------------------------------")

	startMemory = MemoryUsage()
	print("Start memory usage:", startMemory / 1024, "Kbytes")

	for i in range(1, 101):
		RenderConfigOps()
		gc.collect()
		endMemory = MemoryUsage()
		if (i % 10 == 0):
			print("Step", i, " memory usage:", endMemory / 1024, "Kbytes [Delta:", str(endMemory - startMemory) + "]")
	
	print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)

def SimpleRenderTest():
	print("-------------------------------------------------------------------------------------")
	print("SimpleRenderTest")
	print("-------------------------------------------------------------------------------------")

	startMemory = MemoryUsage()
	print("Start memory usage:", startMemory / 1024, "Kbytes")

	for i in range(1, 51):
		SimpleRender()
		gc.collect()
		endMemory = MemoryUsage()
		print("Step", i, " memory usage:", endMemory / 1024, "Kbytes [Delta:", str(endMemory - startMemory) + "]")
		
		print("Max. memory usage:", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)


################################################################################

def LogHandler(msg):
	#print(msg)
	# Nothing to do
	pass

def main():
	pyluxcore.Init(LogHandler)

	print("LuxCore %s" % pyluxcore.Version())

	#PropertiesTest()
	#SceneTest()
	#RenderConfigTest()
	SimpleRenderTest()

if __name__ == '__main__':
	main()
