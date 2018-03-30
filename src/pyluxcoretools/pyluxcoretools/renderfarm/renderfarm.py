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

import time
import enum
import collections
import logging
import threading

import pyluxcoretools.utils.loghandler as loghandler

logger = logging.getLogger(loghandler.loggerName + ".renderfarm")

DEFAULT_PORT=18018

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
		self.jobQueue = collections.deque()
		self.doneJobQueue = collections.deque()
		self.currentJob = None
		self.hasDone = threading.Event()

		self.jobsUpdateCallBack = None
		self.nodesUpdateCallBack = None

	#---------------------------------------------------------------------------
	# Start/Stop the work
	#---------------------------------------------------------------------------

	def Start(self):
		# Nothing to do
		pass

	def Stop(self):
		with self.lock:
			# Remove all pending jobs
			self.RemovePendingJobs()
			# Stop the current job
			self.StopCurrentJob()

	#---------------------------------------------------------------------------
	# Render farm nodes
	#---------------------------------------------------------------------------

	def SetNodesUpdateCallBack(self, callBack):
		self.nodesUpdateCallBack = callBack

	def GetNodesList(self):
		with self.lock:
			return list(self.nodes.values())

	def GetNodesListCount(self):
		with self.lock:
			return len(self.nodes.values())

	def SetNodeState(self, node, state):
		# This method must not use self.lock in order to avoid
		# a dead lock. It assumes the lock has been already acquired.
		node.state = state

		if self.nodesUpdateCallBack:
			self.nodesUpdateCallBack()

	#---------------------------------------------------------------------------
	# Render farm jobs
	#---------------------------------------------------------------------------

	def SetJobsUpdateCallBack(self, callBack):
		self.jobsUpdateCallBack = callBack

	def GetQueuedJobList(self):
		with self.lock:
			jobList = [self.currentJob] if self.currentJob else []
			jobList.extend(self.jobQueue)
			
			return jobList

	def GetQueuedJobCount(self):
		with self.lock:
			return len(self.jobQueue) + 1 if self.currentJob else 0

	def AddJob(self, job):
		with self.lock:
			if (self.currentJob):
				self.jobQueue.append(job)
			else:
				self.currentJob = job
				self.currentJob.Start()

			if self.jobsUpdateCallBack:
				self.jobsUpdateCallBack()

	def RemovePendingJobs(self):
		with self.lock:
			self.jobQueue.clear()

	def StopCurrentJob(self):
		with self.lock:
			logger.info("Stop current job")

			if (not self.currentJob):
				logger.info("No current job to stop")
				return

			self.currentJob.Stop(lastUpdate = True)
			
			self.doneJobQueue.append(self.currentJob)
			self.currentJob = None
			
			if (len(self.jobQueue) == 0):
				self.hasDone.set()
			else:
				self.currentJob = self.jobQueue.popleft()
				self.currentJob.Start()
			
			if self.jobsUpdateCallBack:
				self.jobsUpdateCallBack()

	def CurrentJobDone(self):
		with self.lock:
			logger.info("Current job done")

			self.doneJobQueue.append(self.currentJob)
			self.currentJob = None

			# Look for another job to do
			if (len(self.jobQueue) == 0):
				self.hasDone.set()
			else:
				self.currentJob = self.jobQueue.popleft()
				self.currentJob.Start()
			
			if self.jobsUpdateCallBack:
				self.jobsUpdateCallBack()

	#---------------------------------------------------------------------------
	# HasDone
	#---------------------------------------------------------------------------

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
					# Time to retry, set to FREE
					node.state = NodeState.FREE

					if self.currentJob:
						logger.info("Retrying node: " + key)
						self.currentJob.NewNodeStatus(node)
				
				# Refresh the lastContactTime
				node.lastContactTime = time.time()
			else:
				# It is a new node
				logger.info("Discovered new node: " + key)
				node = RenderFarmNode(address, port, discoveryType)
				self.nodes[key] = node

				if (self.currentJob):
					# Put the new node at work
					self.currentJob.NewNodeStatus(node)

			if self.nodesUpdateCallBack:
				self.nodesUpdateCallBack()

	#---------------------------------------------------------------------------
	# Conversion to string support
	#---------------------------------------------------------------------------

	def __str__(self):
		with self.lock:
			s = "RenderFarm[\n"
			for key in sorted(self.nodes.keys()):
				node = self.nodes[key]

				s += str(node) + "\n"
			s += "]"

			return s
