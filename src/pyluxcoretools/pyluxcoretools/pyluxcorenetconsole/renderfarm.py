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

import collections
import logging
import threading
import enum

import pyluxcoretools.utils.loghandler

logger = logging.getLogger(pyluxcoretools.utils.loghandler.loggerName + ".luxcorenetconsole")

class NodeDiscoveryType(enum.Enum):
	AUTO_DISCOVERED = 0
	MANUALLY_DISCOVERED = 1

class NodeState(enum.Enum):
	UNUSED = 0
	RENDERING = 1
	ERROR = 2
	
RenderFarmNode = collections.namedtuple("RenderFarmNode", ["address", "port", "discoveryType", "state"])

class RenderFarm:
	def __init__(self):
		self.lock = threading.RLock()
		self.nodes = dict()
	
	def DiscoveredNode(self, address, port, discoveryType):
		with self.lock:
			# Expire nodes

			key = str(address) + ":" + str(port)

			# Check if it is a new node
			if (key in self.nodes):
				node = self.nodes[key]
				
				# It is a known node, check the state
				if (node.state == NodeState.UNUSED):
					# Nothing to do
					pass
				elif (node.state == NodeState.RENDERING):
					# Nothing to do
					pass
				elif (node.state == NodeState.ERROR):
					# Time to retry, set to UNUSED
					node.state = NodeState.UNUSED
			else:
				# It is a new node
				self.nodes[key] = RenderFarmNode(address, port, discoveryType, NodeState.UNUSED)
	
	def ToString(self):
		s = ""
		for key in sorted(self.nodes.keys()):
			node = self.nodes[key]

			s += str(node) + "\n"
		
		return s
