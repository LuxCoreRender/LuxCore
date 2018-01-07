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
import os.path
import time

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
from pyluxcoretools.utils.loghandler import logger

def BatchSimpleMode(config):
	session = pyluxcore.RenderSession(config)

	session.Start()

	startTime = time.time()
	while not session.HasDone():
		time.sleep(1)

		elapsedTime = time.time() - startTime

		# Print some information about the rendering progress

		# Update statistics
		session.UpdateStats()

		stats = session.GetStats();
		logger.info("[Elapsed time: %3d/5sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
				stats.Get("stats.renderengine.time").GetFloat(),
				stats.Get("stats.renderengine.pass").GetInt(),
				(stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0),
				(stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)))

	session.Stop()

	# Save the rendered image
	session.GetFilm().Save()

def LuxCoreConsole():
	parser = argparse.ArgumentParser(description="Python LuxCoreConsole")
	parser.add_argument("fileToRender",
						help=".cfg, .lxs, .bcf or .rsm file to render")
	args = parser.parse_args()

	# Load the file to render
	config = None
	configFileNameExt = os.path.splitext(args.fileToRender)[1]
	if (configFileNameExt == ".lxs"):
		# It is a LuxRender SDL file
		logger.info("Parsing LuxRender SDL file...")
		
		# Parse the LXS file
		configProps = pyluxcore.Properties()
		sceneProps = pyluxcore.Properties()
		ParseLXS(args.fileToRender, configProps, sceneProps)
		
		#configProps.Set(cmdLineProp);
	elif (configFileNameExt == ".cfg"):
		# It is a LuxCore SDL file
		configProps = pyluxcore.Properties(args.fileToRender)
		#configProps.Set(cmdLineProp);
		config = pyluxcore.RenderConfig(configProps)
	else:
		raise TypeError("Unknown file extension: " + args.fileToRender)
	
	BatchSimpleMode(config)
	
	logger.info("Done.")

def main():
	try:
		pyluxcore.Init(loghandler.LuxCoreLogHandler)
		print("LuxCore %s" % pyluxcore.Version())

		LuxCoreConsole()
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == '__main__':
	main()
