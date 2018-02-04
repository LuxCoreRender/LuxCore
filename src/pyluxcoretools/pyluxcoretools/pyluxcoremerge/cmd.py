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
import logging

import pyluxcore
import pyluxcoretools.utils.loghandler
import pyluxcoretools.utils.args as argsutils

logger = logging.getLogger(pyluxcoretools.utils.loghandler.loggerName + ".luxcoremerge")

def LoadFilm(filmFileName, hasPixelNormalizedChannel, hasScreenNormalizedChannel):
	fileExt = os.path.splitext(filmFileName)[1]
	if (fileExt == ".cfg"):
		# It is a properties file

		# At least, one of the 2 options must be enabled
		if (not hasPixelNormalizedChannel) and (not hasScreenNormalizedChannel):
			raise TypeError("At least CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED or CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED must be enabled."
							"The film would have no content: " + filmFileName)

		props = pyluxcore.Properties(filmFileName)
		return pyluxcore.Film(props, hasPixelNormalizedChannel, hasScreenNormalizedChannel)
	elif (fileExt == ".flm"):
		# It is a stand alone film
		return pyluxcore.Film(filmFileName)
	elif (fileExt == ".rsm"):
		# It is a resume rendering file
		(config, startState, startFilm) = pyluxcore.RenderConfig.LoadResumeFile(filmFileName)
		return startFilm
	else:
		raise TypeError("Unknown film file type: " + filmFileName)

def LuxCoreMerge(argv):
	# Prepare the Film options parser
	filmParser = argparse.ArgumentParser(description="Film Options", add_help=False)
	# Film options
	filmParser.add_argument("fileFilm",
							help=".cfg, .flm or .rsm files with a film")
	filmParser.add_argument("-p", "--pixel-normalized-channel", action = "store_true", default=False,
							help = "the film will have CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED (required by all render engines)")
	filmParser.add_argument("-s", "--screen-normalized-channel", action = "store_true", default=False,
							help = "the film will have CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED (required by BIDIRCPU and LIGHTCPU render engines)")
	filmParser.add_argument("-r", "--region", metavar=("SRC_OFFSET_X", "SRC_OFFSET_Y",
							"SRC_WIDTH", "SRC_HEIGHT", "DST_OFFSET_X", "DST_OFFSET_Y"),
							nargs=6, type=int,
							help = "define the origin and the size of the region in the source film and the placement in the destination film where the it will be merged")

	# Prepare the general options parser
	generalParser = argparse.ArgumentParser(description="PyLuxCoreMerge", add_help=False)
	# General options
	generalParser.add_argument("-o", "--image-output", metavar="FILE_NAME", nargs=1,
							   help="Save the RGB_IMAGEPIPELINE film output to a file")
	generalParser.add_argument("-f", "--film-output", metavar="FILE_NAME", nargs=1,
							   help="Save the merge film to a file")
	generalParser.add_argument("-h", "--help", action = "store_true",
							   help="Show this help message and exit.")

	# Parse the general options
	(generalArgs, filmArgv) = generalParser.parse_known_args(argv)

	if (generalArgs.help):
		generalParser.print_help()
		filmParser.print_help()
		return

	filmOutput = None
	if (generalArgs.film_output):
		filmOutput = generalArgs.film_output[0]
	imageOutput = None
	if (generalArgs.image_output):
		imageOutput = generalArgs.image_output[0]

	# Split the arguments based of film files
	filmsArgv = list(argsutils.ArgvSplitter(filmArgv, [".cfg", ".flm", ".rsm"]))
	if not filmsArgv:
		generalParser.print_help()
		filmParser.print_help()
		return

	baseFilm = None
	for filmArgs in filmsArgv:
		# Parse carguments
		filmArgs = filmParser.parse_args(filmArgs)

		filmFileName = filmArgs.fileFilm
		if not baseFilm:
			logger.info("Processing the base Film: " + filmFileName)
		else:
			logger.info("Merging the Film: " + filmFileName)
		
		# Load the film
		film = LoadFilm(filmFileName, filmArgs.pixel_normalized_channel, filmArgs.screen_normalized_channel)
		if not baseFilm:
			# Set the base film
			baseFilm = film
		else:
			# Add the current film to the base film
			if (filmArgs.region):
				baseFilm.AddFilm(film, *filmArgs.region)
			else:
				baseFilm.AddFilm(film)

	# Print some film statistics
	stats = baseFilm.GetStats()
	logger.info("Resulting film statistics:")
	logger.info("  Samples per pixel: " + stats.Get("stats.film.spp").GetString())
	logger.info("  Radiance group count: " + stats.Get("stats.film.radiancegorup.count").GetString())

	# Save the merged film if required
	if (filmOutput):
		baseFilm.SaveFilm(filmOutput)
	# Save the RGB_IMAGEPIPELINE if required
	if (imageOutput):
		baseFilm.SaveOutput(imageOutput, pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())

	logger.info("Done.")

def main(argv):
	try:
		pyluxcore.Init(pyluxcoretools.utils.loghandler.LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())

		LuxCoreMerge(argv[1:])
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
	main(sys.argv)
