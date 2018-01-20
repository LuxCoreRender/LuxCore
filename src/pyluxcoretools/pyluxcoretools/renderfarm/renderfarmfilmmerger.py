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

import os
import time
import logging
import threading
import functools 

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler

logger = logging.getLogger(loghandler.loggerName + ".renderfarm")

class RenderFarmFilmMerger:
	def __init__(self, renderFarmJob):
		self.renderFarmJob = renderFarmJob
		self.filmMergeThread = None
		self.filmMergeThreadStopEvent = threading.Event()

	#---------------------------------------------------------------------------
	# Start/Stop the work
	#---------------------------------------------------------------------------

	def Start(self):
		# Start the film merge thread
		self.filmMergeThread = threading.Thread(target=functools.partial(RenderFarmFilmMerger.FilmMergeThread, self))
		self.filmMergeThread.name = "FilmMergeThread"
		self.filmMergeThreadStopEvent.clear()
		self.filmMergeThread.start()

	def Stop(self):
		# Stop the merge film thread
		self.filmMergeThreadStopEvent.set()
		self.filmMergeThread.join()
			
	#---------------------------------------------------------------------------
	# Merge all films
	#---------------------------------------------------------------------------

	def MergeAllFilms(self):
		# Merge all NodeThreadFilms
		film = None
		# Get a copy of nodeThreads in order to be thread safe
		nodeThreadsCopy = self.renderFarmJob.GetNodeThreadsList()

		logger.info("Number of films to merge: " + str(len(nodeThreadsCopy)))
		for nodeThread in nodeThreadsCopy:
			with nodeThread.lock:
				filmThreadFileName = nodeThread.GetNodeFilmFileName()
				# Check if the file exist
				if (not os.path.isfile(filmThreadFileName)):
					logger.info("Film file not yet avilable: " + nodeThread.thread.name + " (" + filmThreadFileName + ")")
					continue

				logger.info("Merging film: " + nodeThread.thread.name + " (" + filmThreadFileName + ")")
				filmThread = pyluxcore.Film(filmThreadFileName)

				if filmThread:
					stats = filmThread.GetStats()
					spp = stats.Get("stats.film.spp").GetFloat()
					logger.info("  Samples per pixel: " + "%.1f" % (spp))

				if film:
					# Merge the film
					film.AddFilm(filmThread)
				else:
					# Read the first film
					film = filmThread

		return film
	
	def SaveMergedFilm(self, film):
		# Save the merged film
		filmName = self.renderFarmJob.GetFilmFileName()
		logger.info("Saving merged film: " + filmName)
		# TODO add SafeSave
		film.SaveFilm(filmName)

		# Save the image output
		imageName = self.renderFarmJob.GetImageFileName()
		logger.info("Saving merged image output: " + imageName)
		film.SaveOutput(imageName, pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())

		# Get the halt conditions
		filmHaltSPP = self.renderFarmJob.GetFilmHaltSPP()
		filmHaltTime = self.renderFarmJob.GetFilmHaltTime()

		# Print some film statistics
		stats = film.GetStats()
		logger.info("Merged film statistics:")
		spp = stats.Get("stats.film.spp").GetFloat()
		logger.info("  Samples per pixel: " + "%.1f" % (spp) + "/" + str(filmHaltSPP))
		dt = time.time() - self.renderFarmJob.GetStartTime()
		logger.info("  Rendering time: " + time.strftime("%H:%M:%S", time.gmtime(dt)) + "/" + time.strftime("%H:%M:%S", time.gmtime(filmHaltTime)))

		if (filmHaltSPP > 0 and spp > filmHaltSPP) or (filmHaltTime > 0 and dt > filmHaltTime):
			return True
		else:
			return False

	def FilmMergeThread(self):
		logger.info("Film merge thread started")

		while True:
			self.filmMergeThreadStopEvent.wait(self.renderFarmJob.renderFarm.filmUpdatePeriod)
			if (self.filmMergeThreadStopEvent.is_set()):
				break
			
			if len(self.renderFarmJob.GetNodeThreadsList()) == 0:
				continue

			logger.info("Merging node films")

			# Merge all NodeThreadFilms
			film = self.MergeAllFilms()

			if (film):
				# Save the merged film
				timeToStop = self.SaveMergedFilm(film)
				if (timeToStop):
					self.renderFarmJob.Stop(stopFilmMerger = False, lastUpdate = True)
					self.renderFarmJob.renderFarm.CurrentJobDone()
					break
		
		logger.info("Film merge thread done")
