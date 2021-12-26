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
		self.filmMergeThreadEvent = threading.Event()
		self.previousFilmSampleCount = 0

	#---------------------------------------------------------------------------
	# Start/Stop the work
	#---------------------------------------------------------------------------

	def Start(self):
		# Start the film merge thread
		self.filmMergeThread = threading.Thread(target=functools.partial(RenderFarmFilmMerger.__FilmMergeThread, self))
		self.filmMergeThread.name = "FilmMergeThread"
		
		self.filmMergeThreadEvent.clear()
		self.filmMergeThreadEventStop = False
		self.filmMergeThreadEventForceFilmMerge = False
		self.filmMergeThreadEventForceFilmMergePeriod = False

		self.filmMergeThread.start()

	def Stop(self):
		# Stop the merge film thread
		self.filmMergeThreadEventStop = True
		self.filmMergeThreadEvent.set()
		self.filmMergeThread.join()

	def ForceFilmMerge(self):
		self.filmMergeThreadEventForceFilmMerge = True
		self.filmMergeThreadEvent.set()

	def ForceFilmMergePeriod(self):
		self.filmMergeThreadEventForceFilmMergePeriod = True
		self.filmMergeThreadEvent.set()

	#---------------------------------------------------------------------------
	# Films merge related methods
	#---------------------------------------------------------------------------

	def MergeAllFilms(self):
		# Merge all NodeThreadFilms
		film = None
		
		# Check if I have a previous film to use
		if self.renderFarmJob.previousFilmFileName:
			logger.info("Loaded previous film: " + self.renderFarmJob.previousFilmFileName)
			film = pyluxcore.Film(self.renderFarmJob.previousFilmFileName)
			
			stats = film.GetStats()
			self.previousFilmSampleCount = stats.Get("stats.film.total.samplecount").GetFloat()
		else:
			self.previousFilmSampleCount = 0
			logger.info("No previous film to load")
		
		# Get a copy of nodeThreads in order to be thread safe
		nodeThreadsCopy = self.renderFarmJob.GetNodeThreadsList()

		logger.info("Number of films to merge: " + str(len(nodeThreadsCopy)))
		for nodeThread in nodeThreadsCopy:
			with nodeThread.lock:
				filmThreadFileName = nodeThread.GetNodeFilmFileName()
				# Check if the file exist
				if (not os.path.isfile(filmThreadFileName)):
					logger.info("Film file not yet available: " + nodeThread.thread.name + " (" + filmThreadFileName + ")")
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
		self.renderFarmJob.SetSamplesPixel(spp)

		dt = time.time() - self.renderFarmJob.GetStartTime()
		logger.info("  Rendering time: " + time.strftime("%H:%M:%S", time.gmtime(dt)) + "/" + time.strftime("%H:%M:%S", time.gmtime(filmHaltTime)))
		
		totalSamples = stats.Get("stats.film.total.samplecount").GetFloat()
		samplesSec = 0.0 if dt <= 0.0 else (totalSamples - self.previousFilmSampleCount) / dt
		logger.info("  Samples/sec: %.1fM samples/sec" % (samplesSec / 1000000.0))
		self.renderFarmJob.SetSamplesSec(samplesSec)

		if (filmHaltSPP > 0 and spp > filmHaltSPP) or (filmHaltTime > 0 and dt > filmHaltTime):
			return True
		else:
			return False

	def __FilmMergeThread(self):
		logger.info("Film merge thread started")

		while True:
			doMerge = False
			if (not self.filmMergeThreadEvent.wait(self.renderFarmJob.filmUpdatePeriod)):
				# Time out, time to do a merge
				doMerge = True
			self.filmMergeThreadEvent.clear()

			if (self.filmMergeThreadEventStop):
				break

			if (self.filmMergeThreadEventForceFilmMergePeriod):
				self.filmMergeThreadEventForceFilmMergePeriod = False
				doMerge = True
	
			if (self.filmMergeThreadEventForceFilmMerge):
				self.filmMergeThreadEventForceFilmMerge = False
				doMerge = True
				
			if len(self.renderFarmJob.GetNodeThreadsList()) == 0:
				continue

			if (doMerge):
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
