#! /usr/bin/python
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

# A simple Blender RenderEngine using LuxCore

bl_info = {
    "name": "LuxCoreDemo Renderer Engine",
    "author": "David 'Dade' Bucciarelli",
	"version": (1, 0),
    "blender": (2, 66, 0),
    "location": "Render engine menu",
    "description": "LuxCoreDemo Renderer Engine",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "support": "NONE",
    "category": "Render"}

import time
from array import *
import sys
# The writing of your own code carries some privilege...
sys.path.append("/home/david/projects/luxrender-dev/luxrays/lib")

import bpy
from mathutils import Matrix, Vector

import pyluxcore

class LuxCoreDemoRenderEngine(bpy.types.RenderEngine):
	# These three members are used by blender to set up the
	# RenderEngine; define its internal name, visible name and capabilities.
	bl_idname = "luxcoredemo_renderer"
	bl_label = "LuxCoreDemo Renderer"
	bl_use_preview = True
	
	# This is the only method called by blender, in this example
	# we use it to detect preview rendering and call the implementation
	# in another method.
	def render(self, blScene):
		print("LuxCore render call")
		
		pyluxcore.Init()
		
		########################################################################
		# RenderConfig properties
		########################################################################
		
		cfgProps = pyluxcore.Properties()
		
		scale = blScene.render.resolution_percentage / 100.0
		filmWidth = int(blScene.render.resolution_x * scale)
		filmHeight = int(blScene.render.resolution_y * scale)
		
		cfgProps.Set(pyluxcore.Property("renderengine.type", ["PATHCPU"]))
		cfgProps.Set(pyluxcore.Property("film.width", [filmWidth]))
		cfgProps.Set(pyluxcore.Property("film.height", [filmHeight]))
		
		########################################################################
		# Create the scene
		########################################################################

		scene = pyluxcore.Scene()
		sceneProps = pyluxcore.Properties()

		########################################################################
		# Get camera and lookat direction
		########################################################################
		
		camera = blScene.camera
		cameraDir = camera.matrix_world * Vector((0, 0, -1))
		sceneProps.Set(pyluxcore.Property("scene.camera.lookat.target", [cameraDir.x, cameraDir.y, cameraDir.z]));
		
		# Camera.location not always updated, but matrix is
		cameraLoc = camera.matrix_world.to_translation()
		sceneProps.Set(pyluxcore.Property("scene.camera.lookat.orig", [cameraLoc.x, cameraLoc.y, cameraLoc.z]));
		cameraUp = camera.matrix_world.to_3x3() * Vector((0, 1, 0))
		sceneProps.Set(pyluxcore.Property("scene.camera.up", [cameraUp.x, cameraUp.y, cameraUp.z]));
		
		cameraFOV = camera.data.angle * 180.0 / 3.1415926536
		sceneProps.Set(pyluxcore.Property("scene.camera.fieldofview", [cameraFOV]));
		
		########################################################################
		# Add Sun and Sky
		########################################################################
		
		sceneProps.Set(pyluxcore.Property("scene.skylight.dir", [-0.5, -0.5, 0.5]));
		sceneProps.Set(pyluxcore.Property("scene.skylight.turbidity", [2.2]));
		sceneProps.Set(pyluxcore.Property("scene.skylight.gain", [1.0, 1.0, 1.0]));
		sceneProps.Set(pyluxcore.Property("scene.sunlight.dir", [-0.5, -0.5, 0.5]));
		sceneProps.Set(pyluxcore.Property("scene.sunlight.turbidity", [2.2]));
		sceneProps.Set(pyluxcore.Property("scene.sunlight.gain", [1.0, 1.0, 1.0]));
		
		########################################################################
		# Add dummy material
		########################################################################
		
		sceneProps.Set(pyluxcore.Property("scene.materials.dummymat.type", ["matte"]))
		sceneProps.Set(pyluxcore.Property("scene.materials.dummymat.kd", [0.7, 0.7, 0.7]))
		
		########################################################################
		# Add all objects
		########################################################################
		
		rendertypes = ["MESH", "SURFACE", "META", "TEXT", "CURVE"]
		for obj in blScene.objects:
			objName = obj.name.replace('.', '___')
			if not obj.hide_render and obj.type in rendertypes:
				try:
					if type(obj) == bpy.types.Object:
						print("Object: {}".format(objName))
						mesh = obj.to_mesh(blScene, True, "RENDER")
					else:
						print("Mesh: {}".format(objName))
						mesh = obj
						mesh.update(calc_tessface = True)
				except:
					print("Pass")
					pass
				
				mesh.transform(obj.matrix_world)
				mesh.update(calc_tessface = True)
				
				verts = [v.co[:] for v in mesh.vertices]

				# Split all polygons in triangles
				tris = []
				for poly in mesh.polygons:
					for loopIndex in range(poly.loop_start + 1, poly.loop_start + poly.loop_total - 1):
						tris.append((mesh.loops[poly.loop_start].vertex_index,
							mesh.loops[loopIndex].vertex_index,
							mesh.loops[loopIndex + 1].vertex_index))
			
			# Define a new object
			scene.DefineMesh("Mesh-" + objName, verts, tris, None, None, None, None)
			sceneProps.Set(pyluxcore.Property("scene.objects." + objName + ".material", ["dummymat"]))
			sceneProps.Set(pyluxcore.Property("scene.objects." + objName + ".ply", ["Mesh-" + objName]))
		
		########################################################################
		# Do the rendering
		########################################################################

		imageBufferFloat = array('f', [0.0] * (filmWidth * filmHeight * 3))
		
		scene.Parse(sceneProps)
		config = pyluxcore.RenderConfig(cfgProps, scene)
		session = pyluxcore.RenderSession(config)

		session.Start()

		startTime = time.time()
		while True:
			time.sleep(0.5)

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
			
			# Update the image
			session.GetFilm().GetOutputFloat(pyluxcore.FilmOutputType.RGB_TONEMAPPED, imageBufferFloat)
			
			# Here we write the pixel values to the RenderResult
			result = self.begin_result(0, 0, filmWidth, filmHeight)
			layer = result.layers[0]
			layer.rect = pyluxcore.ConvertFilmChannelOutput_3xFloat_To_3xFloatList(filmWidth, filmHeight, imageBufferFloat)
			self.end_result(result)
			
			if self.test_break():
				# Time to stop the rendering
				break

		session.Stop()

		print("Done.")
	
	def view_update(self, context):
		print("LuxCore view_update call")
	
	def view_draw(self, context):
		print("LuxCore view_draw call")

# Register the RenderEngine
bpy.utils.register_class(LuxCoreDemoRenderEngine)

# RenderEngines also need to tell UI Panels that they are compatible
# Otherwise most of the UI will be empty when the engine is selected.
# In this example, we need to see the main render image button and
# the material preview panel.
from bl_ui import properties_render
properties_render.RENDER_PT_render.COMPAT_ENGINES.add("luxcoredemo_renderer")
del properties_render
