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
	def __init__(self, renderConfigFile):
		self.lock = threading.RLock()

		logging.info("New render farm job: " + renderConfigFile)
		self.renderConfigFile = renderConfigFile
		# Compute the MD5 of the renderConfigFile
		self.renderConfigFileMD5 = md5utils.md5sum(renderConfigFile)
		logging.info("Job file md5: " + self.renderConfigFileMD5)

		self.workDirectory = os.path.splitext(renderConfigFile)[0] + "-netrendering"
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

class RenderFarm:
	def __init__(self):
		self.lock = threading.RLock()
		self.nodes = dict()
		self.nodeThreads = dict()
		self.jobQueue = collections.deque()
		self.currentJob = None
		self.hasDone = threading.Event()

	#---------------------------------------------------------------------------
	# Render farm job
	#---------------------------------------------------------------------------

	def AddJob(self, job):
		with self.lock:
			if (self.currentJob):
				self.jobQueue.append(job)
			else:
				self.currentJob = job

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
				self.nodes[key] = RenderFarmNode(address, port, discoveryType)

				if (self.currentJob):
					# Put the new node at work
					self.StartNodeThread(self.nodes[key])
	
	#---------------------------------------------------------------------------
	# Node thread
	#---------------------------------------------------------------------------
	
	def StartNodeThread(self, renderFarmNode):
		with self.lock:
			try:
				renderFarmNode.state = NodeState.RENDERING
				
				key = renderFarmNode.GetKey()
				thread = threading.Thread(target=functools.partial(RenderFarm.NodeThread, self, renderFarmNode))
				thread.name = "RenderFarmNodeThread-" + key
				
				self.nodeThreads[key] = thread
				
				thread.start()
			except:
				renderFarmNode.state = NodeState.ERROR
				logging.exception("Error while initializing")

	def NodeThread(self, renderFarmNode):
		# Connect with the node
		with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as nodeSocket:
			nodeSocket.connect((renderFarmNode.address, renderFarmNode.port))

			#-------------------------------------------------------------------
			# Send the LuxCore version (they must match)
			#-------------------------------------------------------------------

			socketutils.SendLine(nodeSocket, pyluxcore.Version())
			result = socketutils.RecvLine(nodeSocket)
			if (result.startswith("ERROR")):
				logging.info(result)
				renderFarmNode.state = NodeState.ERROR
				return
			logging.info("Remote node has the same pyluxcore verison")
			
			#-------------------------------------------------------------------
			# Send the RenderConfig serialized file
			#-------------------------------------------------------------------

			socketutils.SendFile(nodeSocket, self.currentJob.renderConfigFile)

	def __str__(self):
		with self.lock:
			s = "RenderFarm[\n"
			for key in sorted(self.nodes.keys()):
				node = self.nodes[key]

				s += str(node) + "\n"
			s += "]"

			return s
