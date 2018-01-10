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

class NetBeaconSender:
	def __init__(self, ipAddress, port, broadCastAddress, period=3.0):
		self.socket = None
		self.thread = None
		self.ipAddress = ipAddress
		self.port = port
		self.broadCastAddress = broadCastAddress
		self.period = period
		
	def Start(self):
		# Create the socket
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

		# Create the thread
		self.thread = threading.Thread(target=partial(NetBeaconSender.BecaonThread, self))
		self.thread.name = "NetBeaconSenderThread"

		# Run the thread
		self.stopEvent = threading.Event()
		self.thread.start()

	def Stop(self):
		self.stopEvent.set()
		self.thread.join(5.0)

		self.socket.close()
	
	def BecaonThread(self):
		pingMsg = bytearray((
				"LUXPING\n" +
				str(self.ipAddress) + "\n" +
				str(self.port) + "\n"
			).encode("utf-8"))

		while not self.stopEvent.is_set():
			logging.info("NetBeaconSender LUXPING sent: " + str(pingMsg))

			self.socket.sendto(pingMsg, (self.broadCastAddress, BROADCAST_PORT))
			self.stopEvent.wait(self.period)
		
		logging.info("NetBeaconSender thread done")

class NetBeaconReceiver:
	def __init__(self):
		self.socket = None
		self.thread = None
		
	def Start(self):
		# Create the socket
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

		# Create the thread
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.socket.bind(('', BROADCAST_PORT))

		self.thread = threading.Thread(target=partial(NetBeaconReceiver.BecaonThread, self))
		self.thread.name = "NetBeaconReceiverThread"

		# Run the thread
		self.stopEvent = threading.Event()
		self.thread.start()

	def Stop(self):
		# Required to wakeup the socket.recvfrom()
		self.socket.shutdown()
		self.stopEvent.set()
		self.thread.join(5.0)

		self.socket.close()
	
	def BecaonThread(self):
		while not self.stopEvent.is_set():
			data, whereFrom = self.socket.recvfrom(4096)
			logging.info("NetBeaconReceiver LUXPING received from " + str(whereFrom) + ": " + str(data))
		
		logging.info("NetBeaconReceiver thread done")
