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

import argparse
import os
import time
import logging

import pyluxcore
import pyluxcoretools.utils.loghandler

logger = logging.getLogger(pyluxcoretools.utils.loghandler.loggerName + ".luxcoreconsole")

def BatchRendering(config, startState = None, startFilm = None):
	session = pyluxcore.RenderSession(config, startState, startFilm)

	haltTime = config.GetProperty("batch.halttime").GetInt();
	haltSpp = config.GetProperty("batch.haltspp").GetInt();

	session.Start()

	try:
		while not session.HasDone():
			time.sleep(1.0)

			# Print some information about the rendering progress

			# Update statistics
			session.UpdateStats()

			stats = session.GetStats();
			elapsedTime = stats.Get("stats.renderengine.time").GetFloat();
			currentPass = stats.Get("stats.renderengine.pass").GetInt();
			# Convergence test is update inside UpdateFilm()
			convergence = stats.Get("stats.renderengine.convergence").GetFloat();

			logger.info("[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
					elapsedTime, haltTime,
					currentPass, haltSpp,
					100.0 * convergence,
					stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0,
					stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0))
	except KeyboardInterrupt:
		pass

	session.Stop()

	return session

def LuxCoreConsole(argv):
	parser = argparse.ArgumentParser(description="PyLuxCoreConsole")
	parser.add_argument("fileToRender",
						help=".cfg, .lxs, .bcf or .rsm file to render")
	parser.add_argument("-f", "--scene", metavar="FILE_NAME", nargs=1,
						help="scene file name")
	parser.add_argument("-w", "--film-width", metavar="WIDTH", nargs=1, type=int,
						help="film width")
	parser.add_argument("-e", "--film-height", metavar="HEIGHT", nargs=1,
						type=int, help="film height")
	parser.add_argument("-D", "--define", metavar=("PROP_NAME", "VALUE"), nargs=2, action="append",
						help="assign a value to a property")
	parser.add_argument("-d", "--current-dir", metavar="DIR_NAME", nargs=1,
						help="current directory path")
	parser.add_argument("-c", "--remove-unused", action="store_true",
						help="remove all unused meshes, materials, textures and image maps")
	parser.add_argument("-t", "--camera-shutter", metavar="CAMERA_SHUTTER", nargs=2,
						type=float, help="camera shutter open/close")

	# Parse command line arguments
	args = parser.parse_args(argv)
	cmdLineProp = pyluxcore.Properties()
	if (args.scene):
		cmdLineProp.Set(pyluxcore.Property("scene.file", args.scene))
	if (args.film_width):
		cmdLineProp.Set(pyluxcore.Property("film.width", args.film_width))
	if (args.film_height):
		cmdLineProp.Set(pyluxcore.Property("film.height", args.film_height))
	if (args.define):
		for (name, value) in args.define:
			cmdLineProp.Set(pyluxcore.Property(name, value))
	if (args.current_dir):
		os.chdir(args.current_dir[0])
	removeUnused = args.remove_unused

	if (not args.fileToRender):
		raise TypeError("File to render must be specified")

	# Load the file to render
	config = None
	startState = None
	startFilm = None
	configFileNameExt = os.path.splitext(args.fileToRender)[1]
	if (configFileNameExt == ".lxs"):
		# It is a LuxRender SDL file
		logger.info("Parsing LuxRender SDL file...")
		
		# Parse the LXS file
		configProps = pyluxcore.Properties()
		sceneProps = pyluxcore.Properties()
		pyluxcore.ParseLXS(args.fileToRender, configProps, sceneProps)
		configProps.Set(cmdLineProp);

		scene = pyluxcore.Scene(configProps.Get("images.scale", [1.0]).GetFloat())
		scene.Parse(sceneProps)

		config = pyluxcore.RenderConfig(configProps, scene)
	elif (configFileNameExt == ".cfg"):
		# It is a LuxCore SDL file
		configProps = pyluxcore.Properties(args.fileToRender)
		configProps.Set(cmdLineProp);
		config = pyluxcore.RenderConfig(configProps)
	elif (configFileNameExt == ".bcf"):
		# It is a LuxCore RenderConfig binary archive
		config = pyluxcore.RenderConfig(args.fileToRender)
		config.Parse(cmdLineProp);
	elif (configFileNameExt == ".rsm"):
		# It is a rendering resume file
		(config, startState, startFilm) = pyluxcore.RenderConfig.LoadResumeFile(args.fileToRender)
		config.Parse(cmdLineProp);
	else:
		raise TypeError("Unknown file extension: " + args.fileToRender)
	
	if (removeUnused):
		config.GetScene().RemoveUnusedMeshes()
		config.GetScene().RemoveUnusedImageMaps()
		config.GetScene().RemoveUnusedMaterials()
		config.GetScene().RemoveUnusedTextures()

	# Overwrite camera shutter open/close
	if (args.camera_shutter):
		cameraProps = config.GetScene().ToProperties().GetAllProperties("scene.camera")
		cameraProps.Set(pyluxcore.Property("scene.camera.shutteropen", [args.camera_shutter[0]]))
		cameraProps.Set(pyluxcore.Property("scene.camera.shutterclose", [args.camera_shutter[1]]))
		config.GetScene().Parse(cameraProps)

	# Force the film update at 2.5secs (mostly used by PathOCL)
	# Skip in case of a FILESAVER
	isFileSaver = (config.GetProperty("renderengine.type").GetString() == "FILESAVER")
	if (not isFileSaver):
		config.Parse(pyluxcore.Properties().Set(pyluxcore.Property("screen.refresh.interval", 2500)))

	session = BatchRendering(config, startState, startFilm)

	if (not isFileSaver):
		# Save the rendered image
		session.GetFilm().Save()
		
	logger.info("Done.")

def main(argv):
	try:
		pyluxcore.Init(pyluxcoretools.utils.loghandler.LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())

		pyluxcore.AddFileNameResolverPath(".")

		LuxCoreConsole(argv[1:])
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
	main(sys.argv)
