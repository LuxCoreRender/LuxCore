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

logger = logging.getLogger(pyluxcoretools.utils.loghandler.loggerName + ".luxcoremerge")

def ArgvSplitter(argv):
	result = []
	for arg in argv:
		result.append(arg)
		# Check if it is a .cfg, .flm or .rsm file
		if len(arg) > 0 and arg[0] != '-' and os.path.splitext(arg)[1] in [".cfg", ".flm", ".rsm"]:
			yield result
			result = []

	if len(result) > 0:
		raise SyntaxError("Unused command line arguments: " + str(result))

def LoadFilm(filmFileName):
	fileExt = os.path.splitext(filmFileName)[1]
	if (fileExt == ".flm"):
		return pyluxcore.Film(filmFileName)
	else:
		raise TypeError("Unknoen film file type: " + filmFileName)

def LuxCoreMerge(argv):
	# Split the arguments based of film files
	filmsArgv = list(ArgvSplitter(argv))

	# Prepare the agument parser
	parser = argparse.ArgumentParser(description="Python LuxCoreMerge")
	parser.add_argument("fileFilm",
						help=".cfg, .flm or .rsm files with a film")

	baseFilm = None
	for filmArgs in filmsArgv:
		# Parse carguments
		args = parser.parse_args(filmArgs)

		filmFileName = args.fileFilm
		if not baseFilm:
			logger.info("Processing the base Film: " + filmFileName)
		else:
			logger.info("Merging the Film: " + filmFileName)
		
		# Load the film
		film = LoadFilm(filmFileName)
		if not baseFilm:
			baseFilm = film
		else:
			# Add the current film to the base film
			# TODO
			pass

	logger.info("Done.")

def main(argv):
	try:
		pyluxcore.Init(pyluxcoretools.utils.loghandler.LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())

		LuxCoreMerge(argv[1:])
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == '__main__':
	main(sys.argv)
