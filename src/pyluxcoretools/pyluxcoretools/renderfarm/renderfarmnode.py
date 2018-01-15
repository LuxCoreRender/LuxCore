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

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.utils.socket as socketutils
import pyluxcoretools.utils.netbeacon as netbeacon

logger = logging.getLogger(loghandler.loggerName + ".renderfarmnode")

class RenderFarmNode:
	def __init__(self, address, port, broadcastAddress, broadcastPeriod):
		self.address = address
		self.port = port
		self.broadcastAddress = broadcastAddress
		self.broadcastPeriod = broadcastPeriod
		
	def Run(self):
		# Start the broadcast beacon sender
		beacon = netbeacon.NetBeaconSender(self.address, self.port, self.broadcastAddress, self.broadcastPeriod)
		beacon.Start()

		# Listen for connection
		with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as nodeSocket:
			nodeSocket.bind((self.address, self.port))
			nodeSocket.listen(0)

			while True:
				logger.info("Waiting for a new connection")
				clientSocket, addr = nodeSocket.accept()

				with clientSocket:
					logger.info("Received connection from: " + str(addr))

					#---------------------------------------------------------------
					# Check pyluxcore version
					#---------------------------------------------------------------

					remoteVersion = socketutils.RecvLine(clientSocket)
					logger.info("Remote pyluxcore version: " + remoteVersion)
					logger.info("Local pyluxcore version: " + pyluxcore.Version())
					if (remoteVersion != pyluxcore.Version()):
						logger.info("No matching pyluxcore versions !")
						socketutils.SendLine(clientSocket, "ERROR: wrong pyluxcore version" + pyluxcore.Version())
						continue
					socketutils.SendLine(clientSocket, "OK")

					#---------------------------------------------------------------
					# Receive the RenderConfig serialized file
					#---------------------------------------------------------------

					logging.info("Receiving RenderConfig serialized file: " + "test.bcf")
					socketutils.RecvFile(clientSocket, "test.bcf")

		# Stop the broadcast beacon sender
		beacon.Stop()
