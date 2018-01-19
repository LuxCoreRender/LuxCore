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
import logging
import socket

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.utils.socket as socketutils
import pyluxcoretools.utils.netbeacon as netbeacon

logger = logging.getLogger(loghandler.loggerName + ".renderfarmnode")

class RenderFarmNode:
	def __init__(self, address, port, broadcastAddress, broadcastPeriod, customProperties):
		self.address = address
		self.port = port
		self.broadcastAddress = broadcastAddress
		self.broadcastPeriod = broadcastPeriod
		self.customProperties = customProperties
	
	def HandleConnection(self, clientSocket, addr):
		renderConfigFile = "renderfarmnode-tmpfile.bcf"
		filmFile = "renderfarmnode-tmpfile.flm"
		try:
			logger.info("Received connection from: " + str(addr))

			#-----------------------------------------------------------
			# Check pyluxcore version
			#-----------------------------------------------------------

			remoteVersion = socketutils.RecvLine(clientSocket)
			logger.info("Remote pyluxcore version: " + remoteVersion)
			logger.info("Local pyluxcore version: " + pyluxcore.Version())
			if (remoteVersion != pyluxcore.Version()):
				logger.info("No matching pyluxcore versions !")
				socketutils.SendLine(clientSocket, "ERROR: wrong pyluxcore version" + pyluxcore.Version())
				return
			socketutils.SendLine(clientSocket, "OK")

			#-----------------------------------------------------------
			# Receive the RenderConfig serialized file
			#-----------------------------------------------------------

			logging.info("Receiving RenderConfig serialized file: " + renderConfigFile)
			socketutils.RecvFile(clientSocket, renderConfigFile)

			#-----------------------------------------------------------
			# Receive the seed
			#-----------------------------------------------------------

			seed = socketutils.RecvLine(clientSocket)
			logging.info("Received seed: " + seed)
			seed = int(seed)

			#-----------------------------------------------------------
			# Read the RenderConfig serialized file
			#-----------------------------------------------------------

			logging.info("Reading RenderConfig serialized file: " + renderConfigFile)
			config = pyluxcore.RenderConfig(renderConfigFile)
			# Sanitize the RenderConfig
			# TODO
			config.Parse(self.customProperties);

			#-----------------------------------------------------------
			# Start the rendering
			#-----------------------------------------------------------

			session = pyluxcore.RenderSession(config)
			session.Start()

			try:
				socketutils.SendLine(clientSocket, "RENDERING_STARTED")
				result = socketutils.RecvLine(clientSocket)
				if (result.startswith("ERROR")):
					logging.info(result)
					return

				statsLine = "Not yet avilable"
				while True:
					result = socketutils.RecvLine(clientSocket)
					logger.info("Received command: " + result)

					#-------------------------------------------------------
					# Update statistics
					#-------------------------------------------------------

					session.UpdateStats()

					stats = session.GetStats();
					elapsedTime = stats.Get("stats.renderengine.time").GetFloat();
					currentPass = stats.Get("stats.renderengine.pass").GetInt();

					statsLine = "[Elapsed time: %3dsec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]" % (
							elapsedTime, currentPass,
							stats.Get("stats.renderengine.total.samplesec").GetFloat()  / 1000000.0,
							stats.Get("stats.dataset.trianglecount").GetFloat() / 1000.0)

					#-------------------------------------------------------
					# Execute the command
					#-------------------------------------------------------

					if (result.startswith("ERROR")):
						logging.info(result)
						return
					elif (result == "GET_STATS"):
						socketutils.SendLine(clientSocket, statsLine)
					elif (result == "GET_FILM"):
						# Save the film to a file
						session.GetFilm().SaveFilm(filmFile)

						# Transmit the film file
						socketutils.SendFile(clientSocket, filmFile)
					elif (result == "DONE"):
						socketutils.SendOk(clientSocket)
						break
					else:
						raise SyntaxError("Unknow command: " + result)

					#-------------------------------------------------------
					# Print some information about the rendering progress
					#-------------------------------------------------------

					logger.info(statsLine)
			finally:
				session.Stop()
		except KeyboardInterrupt:
			raise
		except Exception as e:
			logging.exception(e)
		finally:
			try:
				os.remove(filmFile)
			except OSError:
				pass
			try:
				os.remove(filmFile + ".bak")
			except OSError:
				pass
			try:
				os.remove(renderConfigFile)
			except OSError:
				pass

			try:
				clientSocket.shutdown(socket.SHUT_RDWR)
			except:
				pass

			try:
				clientSocket.close()
			except:
				pass

			logger.info("Connection done: " + str(addr))

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
				try :
					clientSocket, addr = nodeSocket.accept()

					self.HandleConnection(clientSocket, addr)
				except KeyboardInterrupt:
					logger.info("KeyboardInterrupt received")
					break

		# Stop the broadcast beacon sender
		beacon.Stop()
