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
import socket
import threading
import functools 

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.utils.socket as socketutils
import pyluxcoretools.utils.md5 as md5utils
import pyluxcoretools.utils.filesystem as fsutils
import pyluxcoretools.renderfarm.renderfarmfilmmerger as filmmerger
import pyluxcoretools.renderfarm.renderfarm as renderfarm

logger = logging.getLogger(loghandler.loggerName + ".renderfarm")

class RenderFarmJobSingleImage:
	def __init__(self, renderFarm, renderConfigFileName):
		self.lock = threading.RLock()
		self.renderFarm = renderFarm
		self.nodeThreads = list()

		self.samplesSec = 0.0
		self.samplesPixel = 0.0
		self.jobUpdateCallBack = None

		logger.info("New render farm job: " + renderConfigFileName)
		self.renderConfigFileName = renderConfigFileName
		# Compute the MD5 of the renderConfigFile
		self.renderConfigFileMD5 = md5utils.md5sum(renderConfigFileName)
		logger.info("Job file md5: " + self.renderConfigFileMD5)

		baseName = os.path.splitext(renderConfigFileName)[0]
		self.previousFilmFileName = None
		self.filmFileName = baseName + ".flm"
		self.imageFileName = baseName + ".png"
		self.workDirectory = baseName + "-netrendering"
		self.seed = 1

		self.md5FileName = self.workDirectory + "/render.md5"
		self.seedFileName = self.workDirectory + "/render.seed"

		# Check the work directory
		if (os.path.exists(self.workDirectory)):
			if (not os.path.isdir(self.workDirectory)):
				raise ValueError("Can not use " + self.workDirectory + " as work directory")
			else:
				# The directory already exists, check the md5 of the original scene
				if self.__CheckWorkDirectoryMD5AndSeed():
					# The directory is valid, I can use all films there
					logger.info("Merging all previous films")

					# Merge all existing film files in a single one

					film = None
					for filmName in [f for f in os.listdir(self.workDirectory) if f.endswith(".flm")]:
						fullFileName = self.workDirectory + "/" + filmName
						logger.info("  Merging film: " + fullFileName)
						filmThread = pyluxcore.Film(fullFileName)

						if filmThread:
							stats = filmThread.GetStats()
							spp = stats.Get("stats.film.spp").GetFloat()
							logger.info("    Samples per pixel: " + "%.1f" % (spp))

						if film:
							# Merge the film
							film.AddFilm(filmThread)
						else:
							# Read the first film
							film = filmThread
							
					# Delete old films
					for filmName in [f for f in os.listdir(self.workDirectory) if f.endswith(".flm")]:
						filePath = os.path.join(self.workDirectory, filmName)
						os.unlink(filePath)
			
					if film:
						# TODO add SafeSave
						self.previousFilmFileName =self.workDirectory + "/previous.flm"
						film.SaveFilm(self.previousFilmFileName)
						# Print some film statistics
						stats = film.GetStats()
						logger.info("Merged film statistics:")
						spp = stats.Get("stats.film.spp").GetFloat()
						logger.info("  Samples per pixel: " + "%.1f" % (spp))
				else:
					# The directory is not valid, erase all the films and the MD5s
					self.__ClearWorkDirectory()
					# Write the current MD5
					self.__CreateWorkDirectory()
		else:
			self.__CreateWorkDirectory()

		self.filmHaltSPP = 0
		self.filmHaltTime = 0
#		self.filmHaltConvThreshold = 3.0 / 256.0

		self.statsPeriod = 10
		self.filmUpdatePeriod = 10 * 60

		self.filmMerger = None

	def __CreateWorkDirectory(self):
		# Create the work directory
		if (not os.path.exists(self.workDirectory)):
			os.makedirs(self.workDirectory)

		# Write the scene MD5
		fsutils.WriteFileLine(self.md5FileName, self.renderConfigFileMD5)
	
	def __CheckWorkDirectoryMD5AndSeed(self):
		try:
			# Check the MD5
			oldMD5 = fsutils.ReadFileLine(self.md5FileName)

			logger.info("Previous scene MD5: " + oldMD5)
			logger.info("Current scene MD5:  " + self.renderConfigFileMD5)
			if self.renderConfigFileMD5 == oldMD5:
				logger.info(self.workDirectory + " exists and is valid. Continue the rendering")
			else:
				# The scene is changed
				logger.info(self.workDirectory + " exists but it has been used for a different scene")
				return False
			
			# Read the seed
			oldSeed = fsutils.ReadFileLine(self.seedFileName)
			self.seed = int(oldSeed)

			return True
		except Exception as e:
			logger.exception(e)
			return False

	def __ClearWorkDirectory(self):
		logger.info("Deleting all films and MD5s in: " + self.workDirectory)
		for filmName in [f for f in os.listdir(self.workDirectory) if f.endswith(".flm") or f.endswith(".md5")]:
			filePath = os.path.join(self.workDirectory, filmName)
			os.unlink(filePath)

	def SetFilmHaltSPP(self, v):
		with self.lock:
			self.filmHaltSPP = v
	def GetFilmHaltSPP(self):
		with self.lock:
			return self.filmHaltSPP

	def SetFilmHaltTime(self, v):
		with self.lock:
			self.filmHaltTime = v
	def GetFilmHaltTime(self):
		with self.lock:
			return self.filmHaltTime

#	def SetFilmHaltConvThreshold(self, v):
#		with self.lock:
#			self.filmHaltConvThreshold = v
#	def GetFilmHaltConvThreshold(self, v):
#		with self.lock:
#			return self.filmHaltConvThreshold

	def SetStatsPeriod(self, t):
		with self.lock:
			self.statsPeriod = t
	def GetStatsPeriod(self):
		with self.lock:
			return self.statsPeriod

	def SetFilmUpdatePeriod(self, t):
		with self.lock:
			self.filmUpdatePeriod = t
			if (self.filmMerger):
				self.filmMerger.ForceFilmMergePeriod()
	def GetFilmUpdatePeriod(self):
		with self.lock:
			return self.filmUpdatePeriod

	def GetSeed(self):
		with self.lock:
			seed = self.seed
			self.seed += 1

			# Write the seed to file
			fsutils.WriteFileLine(self.seedFileName, str(self.seed))

			return seed

	def GetRenderConfigFileName(self):
		with self.lock:
			return self.renderConfigFileName

	def GetFilmFileName(self):
		with self.lock:
			return self.filmFileName

	def GetImageFileName(self):
		with self.lock:
			return self.imageFileName

	def GetWorkDirectory(self):
		with self.lock:
			return self.workDirectory

	def GetStartTime(self):
		with self.lock:
			return self.jobStartTime

	def GetNodeThreadsList(self):
		with self.lock:
			return list(self.nodeThreads)

	def SetSamplesSec(self, samplesSec):
		with self.lock:
			self.samplesSec = samplesSec

		if self.jobUpdateCallBack:
			self.jobUpdateCallBack()

	def GetSamplesSec(self):
		with self.lock:
			return self.samplesSec

	def SetSamplesPixel(self, samplesPixel):
		with self.lock:
			self.samplesPixel = samplesPixel

		if self.jobUpdateCallBack:
			self.jobUpdateCallBack()

	def GetSamplesPixel(self):
		with self.lock:
			return self.samplesPixel

	def SetJobUpdateCallBack(self, callBack):
		self.jobUpdateCallBack = callBack

	#---------------------------------------------------------------------------
	# Start/Stop
	#---------------------------------------------------------------------------

	def Start(self):
		with self.lock:
			logger.info("-------------------------------------------------------")
			logger.info("Job started: " + self.renderConfigFileName)
			logger.info("-------------------------------------------------------")

			self.jobStartTime = time.time()
			
			# Put all nodes at work
			nodesList = list(self.renderFarm.nodes.values())
			for node in nodesList:
				# Put the new node at work
				self.NewNodeStatus(node)
			
			# Start the film merger
			self.filmMerger = filmmerger.RenderFarmFilmMerger(self)
			self.filmMerger.Start()

	def Stop(self, stopFilmMerger = True, lastUpdate = False):
		with self.lock:
			logger.info("-------------------------------------------------------")
			logger.info("Job stopped: " + self.renderConfigFileName)
			logger.info("-------------------------------------------------------")
		
			if stopFilmMerger:
				# Stop the film merger
				self.filmMerger.Stop()
			
			if lastUpdate:
				# Tell the node threads to do an update
				for nodeThread in self.nodeThreads:
					nodeThread.UpdateFilm()

				# Tell the threads to stop
				for nodeThread in self.nodeThreads:
					logger.info("Waiting for ending of: " + nodeThread.thread.name)
					nodeThread.Stop()

				film = self.filmMerger.MergeAllFilms()
				if (film):
					# Save the merged film
					 self.filmMerger.SaveMergedFilm(film)
			else:
				# Tell the threads to stop
				for nodeThread in self.nodeThreads:
					logger.info("Waiting for ending of: " + nodeThread.thread.name)
					nodeThread.Stop()

	#---------------------------------------------------------------------------

	def ForceFilmMerge(self):
		with self.lock:
			if (self.filmMerger):
				self.filmMerger.ForceFilmMerge()
			
	def ForceFilmDownload(self):
		with self.lock:
			# Tell the node threads to do an update
			for nodeThread in self.nodeThreads:
				nodeThread.UpdateFilm()

	def NewNodeStatus(self, node):
		with self.lock:
			if node.state != renderfarm.NodeState.FREE:
				return

			# Put the new node at work
			nodeThread = RenderFarmJobSingleImageThread(self, node)
			self.nodeThreads.append(nodeThread)
			nodeThread.Start()

class RenderFarmJobSingleImageThread:
	def __init__(self, jobSingleImage, renderFarmNode):
		self.lock = threading.RLock()
		self.renderFarmNode = renderFarmNode
		self.jobSingleImage = jobSingleImage
		self.thread = None
		# Get an unique seed
		self.seed = str(self.jobSingleImage.GetSeed())

		self.eventCondition = threading.Condition(self.lock)
		self.eventStop = False
		self.eventUpdateFilm = False
		
		self.workDirectory = self.jobSingleImage.GetWorkDirectory()

	def GetNodeFilmFileName(self):
		return self.workDirectory + "/" + self.thread.name.replace(":", "_") + "-" + self.seed + ".flm"

	def Start(self):
		with self.lock:
			try:
				self.jobSingleImage.renderFarm.SetNodeState(self.renderFarmNode, renderfarm.NodeState.RENDERING)
				
				key = self.renderFarmNode.GetKey()
				self.thread = threading.Thread(target=functools.partial(RenderFarmJobSingleImageThread.NodeThread, self))
				self.thread.name = "RenderFarmNodeThread-" + key
				
				self.thread.start()
			except:
				self.jobSingleImage.renderFarm.SetNodeState(self.renderFarmNode, renderfarm.NodeState.ERROR)
				logger.exception("Error while initializing")

	def UpdateFilm(self):
		with self.eventCondition:
			if (not self.thread.is_alive):
				return

			self.eventUpdateFilm = True
			self.eventCondition.notify()

	def Stop(self):
		with self.eventCondition:
			if (not self.thread.is_alive):
				return

			self.eventStop = True
			self.eventCondition.notify()

		self.thread.join()
		
		self.jobSingleImage.renderFarm.SetNodeState(self.renderFarmNode, renderfarm.NodeState.FREE)

	def NodeThread(self):
		logger.info("Node thread started")

		# Connect with the node
		nodeSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			# Use a timepout of 10 seconds
			nodeSocket.settimeout(10)
			nodeSocket.connect((self.renderFarmNode.address, self.renderFarmNode.port))
			nodeSocket.settimeout(None)
			

			#-------------------------------------------------------------------
			# Send the LuxCore version (they must match)
			#-------------------------------------------------------------------

			socketutils.SendLine(nodeSocket, pyluxcore.Version())
			socketutils.RecvOk(nodeSocket)
			logger.info("Remote node has the same pyluxcore version")

			#-------------------------------------------------------------------
			# Send the RenderConfig serialized file
			#-------------------------------------------------------------------

			socketutils.SendFile(nodeSocket, self.jobSingleImage.GetRenderConfigFileName())

			#-------------------------------------------------------------------
			# Send the seed
			#-------------------------------------------------------------------

			logger.info("Sending seed: " + self.seed)
			socketutils.SendLine(nodeSocket, self.seed)

			#-------------------------------------------------------------------
			# Receive the rendering start
			#-------------------------------------------------------------------

			logger.info("Waiting for node rendering start")
			result = socketutils.RecvLine(nodeSocket)
			if (result != "RENDERING_STARTED"):
				logger.info(result)
				raise RuntimeError("Error while waiting for the rendering start")
			logger.info("Node rendering started")

			#-------------------------------------------------------------------
			# Receive stats and film
			#-------------------------------------------------------------------

			lastFilmUpdate = time.time()
			with self.eventCondition:
				while True:
					timeTofilmUpdate = self.jobSingleImage.filmUpdatePeriod - (time.time() - lastFilmUpdate)

					continueLoop = False
					if ((timeTofilmUpdate <= 0.0) or self.eventUpdateFilm):
						# Time to request a film update
						socketutils.SendLine(nodeSocket, "GET_FILM")
						# TODO add SafeSave
						socketutils.RecvFile(nodeSocket, self.GetNodeFilmFileName())
						lastFilmUpdate = time.time()
						self.eventUpdateFilm = False
						# Check the stop condition before to continue the loop
						continueLoop = True

					if (self.eventStop):
						logger.info("Waiting for node rendering stop")
						socketutils.SendLine(nodeSocket, "DONE")
						socketutils.RecvOk(nodeSocket)
						break

					if continueLoop:
						continue

					# Print some rendering node statistic
					socketutils.SendLine(nodeSocket, "GET_STATS")
					result = socketutils.RecvLine(nodeSocket)
					if (result.startswith("ERROR")):
						logger.info(result)
						raise RuntimeError("Error while waiting for the rendering statistics")
					logger.info("Node rendering statistics: " + result)

					self.eventCondition.wait(min(timeTofilmUpdate, self.jobSingleImage.statsPeriod))
		except Exception as e:
			logger.exception(e)
			self.jobSingleImage.renderFarm.SetNodeState(self.renderFarmNode, renderfarm.NodeState.ERROR)
		else:
			self.jobSingleImage.renderFarm.SetNodeState(self.renderFarmNode, renderfarm.NodeState.FREE)
		finally:
				try:
					nodeSocket.shutdown(socket.SHUT_RDWR)
				except:
					pass

				try:
					nodeSocket.close()
				except:
					pass

		logger.info("Node thread done")
