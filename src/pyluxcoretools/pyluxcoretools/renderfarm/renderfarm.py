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
import enum
import collections
import logging
import socket
import threading
import functools 

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.utils.socket as socketutils
import pyluxcoretools.utils.md5 as md5utils

logger = logging.getLogger(loghandler.loggerName + ".renderfarm")

DEFAULT_PORT=18018

class RenderFarmJob:
	def __init__(self, renderConfigFileName):
		self.lock = threading.RLock()

		logging.info("New render farm job: " + renderConfigFileName)
		self.renderConfigFileName = renderConfigFileName
		# Compute the MD5 of the renderConfigFile
		self.renderConfigFileMD5 = md5utils.md5sum(renderConfigFileName)
		logging.info("Job file md5: " + self.renderConfigFileMD5)

		baseName = os.path.splitext(renderConfigFileName)[0]
		self.filmFileName = baseName + ".flm"
		self.imageFileName = baseName + ".png"
		self.workDirectory = baseName + "-netrendering"
		self.seed = 1

		# Check the work directory 
		if (not os.path.exists(self.workDirectory)):
			# Create the work directory
			os.makedirs(self.workDirectory)
		elif (not os.path.isdir(self.workDirectory)):
			raise ValueError("Can not use " + self.workDirectory + " as work directory")
		
	def GetSeed(self):
		with self.lock:
			seed = self.seed
			self.seed += 1
			return self.seed

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

class NodeDiscoveryType(enum.Enum):
	AUTO_DISCOVERED = 0
	MANUALLY_DISCOVERED = 1

class NodeState(enum.Enum):
	FREE = 0
	RENDERING = 1
	ERROR = 2

class RenderFarmNode:
	def __init__(self, address, port, discoveryType):
		self.address = address
		self.port = port
		self.discoveryType = discoveryType
		self.state = NodeState.FREE
		self.lastContactTime = time.time()

	@staticmethod
	def Key(address, port):
		return str(address) + ":" + str(port)

	def GetKey(self):
		return RenderFarmNode.Key(self.address, self.port)

	def __str__(self):
		return "RenderFarmNode[" + str(self.address) + ", " + str(self.port) + ", " + \
			str(self.discoveryType) + ", " + str(self.state) + ", " + \
			time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(self.lastContactTime)) + "]"

class RenderFarmNodeThread:
	def __init__(self, renderFarm, renderFarmNode):
		self.lock = threading.RLock()
		self.renderFarm = renderFarm
		self.renderFarmNode = renderFarmNode
		self.thread = None
		self.stopEvent = threading.Event()

	def GetNodeFilmFileName(self, currentJob):
		return currentJob.GetWorkDirectory() + "/" + self.thread.name.replace(":", "_") + ".flm"
		
	def Start(self):
		with self.lock:
			try:
				self.renderFarmNode.state = NodeState.RENDERING
				
				key = self.renderFarmNode.GetKey()
				self.thread = threading.Thread(target=functools.partial(RenderFarmNodeThread.NodeThread, self))
				self.thread.name = "RenderFarmNodeThread-" + key
				
				self.thread.start()
			except:
				self.renderFarmNode.state = NodeState.ERROR
				logging.exception("Error while initializing")

	def Stop(self):
		self.stopEvent.set()

	def Join(self):
		self.thread.join()

	def NodeThread(self):
		logger.info("Node thread started")

		# Connect with the node
		nodeSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			nodeSocket.connect((self.renderFarmNode.address, self.renderFarmNode.port))

			#-------------------------------------------------------------------
			# Send the LuxCore version (they must match)
			#-------------------------------------------------------------------

			socketutils.SendLine(nodeSocket, pyluxcore.Version())
			socketutils.RecvOk(nodeSocket)
			logging.info("Remote node has the same pyluxcore verison")

			#-------------------------------------------------------------------
			# Send the RenderConfig serialized file
			#-------------------------------------------------------------------

			socketutils.SendFile(nodeSocket, self.renderFarm.currentJob.GetRenderConfigFileName())

			#-------------------------------------------------------------------
			# Send the seed
			#-------------------------------------------------------------------

			seed = str(self.renderFarm.currentJob.GetSeed())
			logging.info("Sending seed: " + seed)
			socketutils.SendLine(nodeSocket, seed)

			#-------------------------------------------------------------------
			# Receive the rendering start
			#-------------------------------------------------------------------

			logging.info("Waiting for node rendering start")
			result = socketutils.RecvLine(nodeSocket)
			if (result != "RENDERING_STARTED"):
				logging.info(result)
				raise RuntimeError("Error while waiting for the rendering start")
			socketutils.SendOk(nodeSocket)

			#-------------------------------------------------------------------
			# Receive stats and film
			#-------------------------------------------------------------------

			lastFilmUpdate = time.time()
			while True:
				timeTofilmUpdate = self.renderFarm.GetFilmUpdatePeriod() - (time.time() - lastFilmUpdate)
				if (timeTofilmUpdate <= 0.0):
					with self.lock:
						# Time to request a film update
						socketutils.SendLine(nodeSocket, "GET_FILM")
						# TODO add SafeSave
						socketutils.RecvFile(nodeSocket, self.GetNodeFilmFileName(self.renderFarm.currentJob))
						lastFilmUpdate = time.time()
					continue

				self.stopEvent.wait(min(timeTofilmUpdate, self.renderFarm.GetStatsPeriod()))
				if (self.stopEvent.is_set()):
					# Do the last update
					socketutils.SendLine(nodeSocket, "GET_FILM")
					# TODO add SafeSave
					socketutils.RecvFile(nodeSocket, self.GetNodeFilmFileName(self.renderFarm.currentJob))

					socketutils.SendLine(nodeSocket, "DONE")
					socketutils.RecvOk(nodeSocket)
					break

				# Print some rendering node statistic
				socketutils.SendLine(nodeSocket, "GET_STATS")
				result = socketutils.RecvLine(nodeSocket)
				if (result.startswith("ERROR")):
					logging.info(result)
					raise RuntimeError("Error while waiting for the rendering statistics")
					return
				logger.info("Node rendering statistics: " + result)
		except Exception as e:
			logging.exception(e)
			self.renderFarmNode.state = NodeState.ERROR
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

class RenderFarm:
	def __init__(self):
		self.lock = threading.RLock()

		self.nodes = dict()
		self.nodeThreads = dict()
		self.jobQueue = collections.deque()
		self.currentJob = None
		self.hasDone = threading.Event()

		self.statsPeriod = 10.0
		self.filmUpdatePeriod = 10.0 * 60.0
		self.filmMergeThread = None
		self.filmMergeThreadStopEvent = threading.Event()
		
		self.filmHaltSPP = 0
		self.filmHaltTime = 0
#		self.filmHaltConvThreshold = 3.0 / 256.0

	def SetStatsPeriod(self, t):
		with self.lock:
			self.statsPeriod = t
	def GetStatsPeriod(self):
		with self.lock:
			return self.statsPeriod

	def SetFilmUpdatePeriod(self, t):
		with self.lock:
			self.filmUpdatePeriod = t
	def GetFilmUpdatePeriod(self):
		with self.lock:
			return self.filmUpdatePeriod

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

	def GetNodeThreadsList(self):
		with self.lock:
			return list(self.nodeThreads.values())

	#---------------------------------------------------------------------------
	# Start/Stop the work
	#---------------------------------------------------------------------------

	def Start(self):
		# Nothing to do
		pass

	def Stop(self):
		with self.lock:
			self.filmMergeThreadStopEvent.set()
			self.StopCurrentJob()
			

	#---------------------------------------------------------------------------
	# Render farm job
	#---------------------------------------------------------------------------

	def AddJob(self, job):
		with self.lock:
			if (self.currentJob):
				self.jobQueue.append(job)
			else:
				self.currentJob = job
				self.currentJobStartTime = time.time()
				# Start the film merge thread
				self.filmMergeThread = threading.Thread(target=functools.partial(RenderFarm.FilmMergeThread, self))
				self.filmMergeThread.name = "FilmMergeThread"
				self.filmMergeThreadStopEvent.clear()
				self.filmMergeThread.start()

	def HasDone(self):
		while not self.hasDone.is_set():
			self.hasDone.wait()

	#---------------------------------------------------------------------------
	# Node discovery
	#---------------------------------------------------------------------------

	def DiscoveredNode(self, address, port, discoveryType):
		with self.lock:
			key = RenderFarmNode.Key(address, port)

			# Check if it is a new node
			if (key in self.nodes):
				node = self.nodes[key]
				
				# It is a known node, check the state
				if (node.state == NodeState.FREE):
					# Nothing to do
					pass
				elif (node.state == NodeState.RENDERING):
					# Nothing to do
					pass
				elif (node.state == NodeState.ERROR):
					# Time to retry, set to UNUSED
					#node.state = NodeState.UNUSED
					# Doing nothing for the moment
					pass
				
				# Refresh the lastContactTime
				node.lastContactTime=time.time()
			else:
				# It is a new node
				logger.info("Discovered new node: " + key)
				renderFarmNode = RenderFarmNode(address, port, discoveryType)
				self.nodes[key] = renderFarmNode

				if (self.currentJob):
					# Put the new node at work
					nodeThread = RenderFarmNodeThread(self, renderFarmNode)
					self.nodeThreads[key] = nodeThread
					nodeThread.Start()

	#---------------------------------------------------------------------------
	# Stop current job
	#---------------------------------------------------------------------------

	def StopCurrentJob(self):
		with self.lock:
			logging.info("Stop current job")

			if (not self.currentJob):
				logging.info("No current job to stop")
				return

			# Tell the threads to do a last update
			nodeThreadsCopy = self.GetNodeThreadsList()
			for nodeThread in nodeThreadsCopy:
				nodeThread.Stop()
			for nodeThread in nodeThreadsCopy:
				logging.info("Waiting for ending of: " + nodeThread.renderFarmNode.GetKey())
				nodeThread.Join()

			# The final merge
			film = self.MergeAllFilms()
			if (film):
				# Save the merged film
				self.SaveMergedFilm(film)

			logging.info("Current job stopped")
			
			if (len(self.jobQueue) == 0):
				self.hasDone.set()

	#---------------------------------------------------------------------------
	# Film merge thread
	# TODO: move to a dedicated file
	#---------------------------------------------------------------------------
	
	def MergeAllFilms(self):
		# Merge all NodeThreadFilms
		film = None
		# Get a copy of nodeThreads in order to be thread safe
		nodeThreadsCopy = self.GetNodeThreadsList()

		for nodeThread in nodeThreadsCopy:
			with nodeThread.lock:
				filmThreadFileName = nodeThread.GetNodeFilmFileName(self.currentJob)
				# Check if the file exist
				if (not os.path.isfile(filmThreadFileName)):
					continue

				logger.info("Merging film: " + nodeThread.thread.name + " (" + filmThreadFileName + ")")
				filmThread = pyluxcore.Film(filmThreadFileName)
				if (film):
					# Merge the film
					film.AddFilm(filmThread)
				else:
					# Read the first film
					film = filmThread

		return film
	
	def SaveMergedFilm(self, film):
		# Save the merged film
		filmName = self.currentJob.GetFilmFileName()
		logger.info("Saving merged film: " + filmName)
		# TODO add SafeSave
		film.SaveFilm(filmName)

		# Save the image output
		imageName = self.currentJob.GetImageFileName()
		logger.info("Saving merged image output: " + imageName)
		film.SaveOutput(imageName, pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())

		# Get the halt conditions
		filmHaltSPP = self.GetFilmHaltSPP()
		filmHaltTime = self.GetFilmHaltTime()

		# Print some film statistics
		stats = film.GetStats()
		logger.info("Merged film statistics:")
		spp = stats.Get("stats.film.spp").GetFloat()
		logger.info("  Samples per pixel: " + "%.1f" % (spp) + "/" + str(filmHaltSPP))
		dt = time.time() - self.currentJobStartTime
		logger.info("  Rendering time: " + time.strftime("%H:%M:%S", time.gmtime(dt)) + "/" + time.strftime("%H:%M:%S", time.gmtime(filmHaltTime)))

		if (filmHaltSPP > 0 and spp > filmHaltSPP) or (filmHaltTime > 0 and dt > filmHaltTime):
			return True
		else:
			return False

	def FilmMergeThread(self):
		logger.info("Film merge thread started")

		while True:
			self.filmMergeThreadStopEvent.wait(self.filmUpdatePeriod)
			if (self.filmMergeThreadStopEvent.is_set()):
				break
			
			logger.info("Merging node films")

			# Merge all NodeThreadFilms
			film = self.MergeAllFilms()

			if (film):
				# Save the merged film
				timeToStop = self.SaveMergedFilm(film)
				if (timeToStop):
					# Time to stop
					break
		
		self.StopCurrentJob()
		logger.info("Film merge thread done")

	def __str__(self):
		with self.lock:
			s = "RenderFarm[\n"
			for key in sorted(self.nodes.keys()):
				node = self.nodes[key]

				s += str(node) + "\n"
			s += "]"

			return s
