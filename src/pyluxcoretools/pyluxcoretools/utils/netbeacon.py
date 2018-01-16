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
import functools

import pyluxcoretools.utils.loghandler as loghandler

logger = logging.getLogger(loghandler.loggerName + ".netbeacon")

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
		self.thread = threading.Thread(target=functools.partial(NetBeaconSender.BecaonThread, self))
		self.thread.name = "NetBeaconSenderThread"

		# Run the thread
		self.stopEvent = threading.Event()
		self.thread.start()

	def Stop(self):
		self.stopEvent.set()
		self.thread.join(5.0)

		self.socket.close()
	
	def BecaonThread(self):
		logging.info("NetBeaconSender thread started.")

		pingMsg = bytearray((
				"LUXNETPING\n" +
				str(self.ipAddress) + "\n" +
				str(self.port) + "\n"
			).encode("utf-8"))

		while not self.stopEvent.is_set():
			logging.debug("NetBeaconSender LUXNETPING sent: " + str(pingMsg))

			self.socket.sendto(pingMsg, (self.broadCastAddress, BROADCAST_PORT))
			self.stopEvent.wait(self.period)
		
		logging.info("NetBeaconSender thread done.")

class NetBeaconReceiver:
	def __init__(self, callback):
		self.socket = None
		self.thread = None
		self.callback = callback
		
	def Start(self):
		# Create the socket
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
		self.socket.bind(('', BROADCAST_PORT))

		# Create the thread
		self.thread = threading.Thread(target=functools.partial(NetBeaconReceiver.BecaonThread, self))
		self.thread.name = "NetBeaconReceiverThread"

		# Run the thread
		self.stopEvent = threading.Event()
		self.thread.start()

	def Stop(self):
		# Required to wakeup the socket.recvfrom()
		self.socket.close()

		self.stopEvent.set()
		self.thread.join(5.0)

	def BecaonThread(self):
		logging.info("NetBeaconReceiver thread started.")

		try:
			while not self.stopEvent.is_set():
				data, whereFrom = self.socket.recvfrom(4096)
				if (not data):
					break

				logging.debug("NetBeaconReceiver LUXNETPING received from " + str(whereFrom) + ": " + str(data))
				tag, ipAddress, port, _ = data.decode("utf-8").split("\n")

				if (tag != "LUXNETPING"):
					continue
				if (ipAddress == ""):
					ipAddress = str(whereFrom[0])

				self.callback(ipAddress, int(port))
		except Exception as e:
			logging.info("BecaonThread exception: " + e)
		
		logging.info("NetBeaconReceiver thread done.")
