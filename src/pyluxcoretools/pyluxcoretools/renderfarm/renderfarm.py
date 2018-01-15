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

	def NodeThread(self):
		# Connect with the node
		with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as nodeSocket:
			nodeSocket.connect((self.renderFarmNode.address, self.renderFarmNode.port))

			#-------------------------------------------------------------------
			# Send the LuxCore version (they must match)
			#-------------------------------------------------------------------

			socketutils.SendLine(nodeSocket, pyluxcore.Version())
			result = socketutils.RecvLine(nodeSocket)
			if (result.startswith("ERROR")):
				logging.info(result)
				self.renderFarmNode.state = NodeState.ERROR
				return
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

			result = socketutils.RecvLine(nodeSocket)
			if (result != "RENDERING_STARTED"):
				logging.info(result)
				self.renderFarmNode.state = NodeState.ERROR
				return
			socketutils.SendLine(nodeSocket, "OK")

			#-------------------------------------------------------------------
			# Receive stats and film
			#-------------------------------------------------------------------
			
			lastFilmUpdate = time.time()
			while True:
				timeTofilmUpdate = self.renderFarm.filmUpdatePeriod - (time.time() - lastFilmUpdate)
				if (timeTofilmUpdate <= 0.0):
					with self.lock:
						# Time to request a film update
						socketutils.SendLine(nodeSocket, "GET_FILM")
						# TODO add SafeSave
						socketutils.RecvFile(nodeSocket, self.GetNodeFilmFileName(self.renderFarm.currentJob))
						lastFilmUpdate = time.time()
					continue

				time.sleep(min(timeTofilmUpdate, self.renderFarm.statsPeriod))

				# Print some rendering node statistic
				socketutils.SendLine(nodeSocket, "GET_STATS")
				result = socketutils.RecvLine(nodeSocket)
				if (result.startswith("ERROR")):
					logging.info(result)
					self.renderFarmNode.state = NodeState.ERROR
					return
				logger.info("Node rendering statistics: " + result)

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

	def SetStatsPeriod(self, t):
		with self.lock:
			self.statsPeriod = t

	def SetFilmUpdatePeriod(self, t):
		with self.lock:
			self.filmUpdatePeriod = t

	#---------------------------------------------------------------------------
	# Render farm job
	#---------------------------------------------------------------------------

	def AddJob(self, job):
		with self.lock:
			if (self.currentJob):
				self.jobQueue.append(job)
			else:
				self.currentJob = job
				# Start the film merge thread
				self.filmMergeThread = threading.Thread(target=functools.partial(RenderFarm.FilmMergeThread, self))
				self.filmMergeThread.name = "FilmMergeThread"
				self.filmMergeThread.start()

	def HasDone(self):
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
	# Film merge thread
	#---------------------------------------------------------------------------
	
	def FilmMergeThread(self):
		while True:
			time.sleep(self.filmUpdatePeriod)
			
			logger.info("Merging node films")

			# Merge all NodeThreadFilms
			film = None
			for nodeThread in self.nodeThreads.values():
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

			if (film):
				# Print some film statistics
				stats = film.GetStats()
				logger.info("Merged film statistics:")
				logger.info("  Samples per pixel: " + stats.Get("stats.film.spp").GetString())

				# Save the merged film
				filmName = self.currentJob.GetFilmFileName()
				logger.info("Saving merged film: " + filmName)
				# TODO add SafeSave
				film.SaveFilm(filmName)

				# Save the image output
				imageName = self.currentJob.GetImageFileName()
				logger.info("Saving merged film: " + filmName)
				film.SaveOutput(imageName, pyluxcore.FilmOutputType.RGB_IMAGEPIPELINE, pyluxcore.Properties())

	def __str__(self):
		with self.lock:
			s = "RenderFarm[\n"
			for key in sorted(self.nodes.keys()):
				node = self.nodes[key]

				s += str(node) + "\n"
			s += "]"

			return s
