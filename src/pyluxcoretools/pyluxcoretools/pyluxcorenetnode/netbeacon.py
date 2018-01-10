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

import logging
import socket
import threading
from functools import partial

import pyluxcoretools.utils.loghandler

logger = logging.getLogger(pyluxcoretools.utils.loghandler.loggerName + ".luxcorenetnode")

BROADCAST_PORT = 18019

class NetBeacon:
	def __init__(self, ipAddress, port, broadCastAddress, period=3.0):
		self.soc = None
		self.thread = None
		self.ipAddress = ipAddress
		self.port = port
		self.broadCastAddress = broadCastAddress
		self.period = period
		
	def Start(self):
		# Create the socket
		self.soc = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.soc.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

		# Create the thread
		self.thread = threading.Thread(target=partial(NetBeacon.BecaonThread, self))
		self.thread.name = "NetBeaconThread"

		# Run the thread
		self.stopEvent = threading.Event()
		self.thread.start()

	def Stop(self):
		self.stopEvent.set()
		self.thread.join(5.0)

		self.soc.close()
	
	def BecaonThread(self):
		pingMsg = bytearray((
				"LUXPING\n" +
				str(self.ipAddress) + "\n" +
				str(self.port) + "\n"
			).encode("utf-8"))

		while not self.stopEvent.is_set():
			logging.info("NetBeacon LUXPING sent")
			self.soc.sendto(pingMsg, (self.broadCastAddress, BROADCAST_PORT))
			self.stopEvent.wait(self.period)
		
		logging.info("NetBeacon thread done")
