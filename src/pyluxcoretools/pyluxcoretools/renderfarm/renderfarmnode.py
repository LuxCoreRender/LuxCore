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
import uuid
import logging
import socket
import platform
import threading

import pyluxcore
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.utils.socket as socketutils
import pyluxcoretools.utils.netbeacon as netbeacon
import pyluxcoretools.utils.md5 as md5utils

logger = logging.getLogger(loghandler.loggerName + ".renderfarmnode")

class RenderFarmNode:
	def __init__(self, address, port, broadcastAddress, broadcastPeriod, customProperties):
		if (address == ""):
			self.address = socket.gethostbyname(socket.gethostname())
		else:
			self.address = address
		self.port = port
		self.broadcastAddress = broadcastAddress
		self.broadcastPeriod = broadcastPeriod
		self.customProperties = customProperties
		
	def Start(self):
		self.threadStop = False
		self.renderFarmNodeThread = threading.Thread(target=self.__ThreadRun)
		self.renderFarmNodeThread.start()
	
	def Wait(self):
		try :
			self.renderFarmNodeThread.join()
		except KeyboardInterrupt:
			logger.info("KeyboardInterrupt received")
			
			self.Stop()

	def Stop(self):
		self.threadStop = True
		self.renderFarmNodeThread.join()
	
	def __SanitizeRenderConfig(self, config):
		# First sanitize the render engine type

		if pyluxcore.GetPlatformDesc().Get("compile.LUXRAYS_DISABLE_OPENCL").GetBool():
			# OpenCL is not available
			logger.info("OpenCL render engines not available")

			props = config.GetProperties()		
	
			# Check render engine
			if props.IsDefined("renderengine.type"):
				renderEngine = props.Get("renderengine.type").GetString()
				if renderEngine == "PATHOCL":
					# Switch to PATHCPU
					renderEngine = "PATHCPU"
					logger.info("rendernegine.type set to PATHOCL, switching to PATHCPU")
				elif renderEngine == "TILEPATHOCL":
					# Switch to TILEPATHCPU
					renderEngine = "TILEPATHCPU"
					logger.info("rendernegine.type set to TILEPATHOCL, switching to TILEPATHCPU")
				elif renderEngine == "RTPATHOCL":
					# Switch to RTPATHCPU
					renderEngine = "RTPATHCPU"
					logger.info("rendernegine.type set to RTPATHOCL, switching to RTPATHCPU")
				
				config.Parse(pyluxcore.Properties().SetFromString("renderengine.type = " + renderEngine))
		else:
			# OpenCL is available
			logger.info("OpenCL render engines available")

		defaultProps = """
			# Disable halt conditions
			batch.haltthreshold = -1
			batch.halttime = 0
			batch.haltspp = 0
			tile.multipass.enable = 1
			tile.multipass.convergencetest.threshold = 0.75

			# Disable any periodic saving
			periodicsave.resumerendering.period = 0
			periodicsave.film.period = 0
			periodicsave.film.outputs.period = 0

			# Disable any OpenCL settings (including film settings)
			opencl.platform.index = -1
			opencl.cpu.use = 0
			opencl.gpu.use = 1
			opencl.gpu.workgroup.size = 64
			opencl.devices.select = ""
			
			# Force the film update at 2.5secs (mostly used by PathOCL)
			screen.refresh.interval = 2500
		"""
		
		# There is a problem with opencl.cpu.workgroup.size because MacOS
		# requires 1 as default while other OS 0
		if platform.system().lower() == "darwin":
			defaultProps += "opencl.cpu.workgroup.size = 1\n"
		else:
			defaultProps += "opencl.cpu.workgroup.size = 0\n"
		
		config.Parse(pyluxcore.Properties().SetFromString(defaultProps))			

	def __HandleConnection(self, clientSocket, addr):
		id = str(uuid.uuid4())
		renderConfigFile = "renderfarmnode-" + id + ".bcf"
		filmFile = "renderfarmnode-" + id + ".flm"
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

			logger.info("Receiving RenderConfig serialized file: " + renderConfigFile)
			socketutils.RecvFile(clientSocket, renderConfigFile)
			logger.info("Receiving RenderConfig serialized MD5: " + md5utils.md5sum(renderConfigFile))

			#-----------------------------------------------------------
			# Receive the seed
			#-----------------------------------------------------------

			seed = socketutils.RecvLine(clientSocket)
			logger.info("Received seed: " + seed)
			seed = int(seed)

			#-----------------------------------------------------------
			# Read the RenderConfig serialized file
			#-----------------------------------------------------------

			logger.info("Reading RenderConfig serialized file: " + renderConfigFile)
			config = pyluxcore.RenderConfig(renderConfigFile)
			# Sanitize the RenderConfig
			self.__SanitizeRenderConfig(config)
			config.Parse(self.customProperties);

			#-----------------------------------------------------------
			# Start the rendering
			#-----------------------------------------------------------

			session = pyluxcore.RenderSession(config)
			session.Start()

			try:
				socketutils.SendLine(clientSocket, "RENDERING_STARTED")

				statsLine = "Not yet available"
				while not self.threadStop:
					result = socketutils.RecvLineWithTimeOut(clientSocket, 0.2)
					# Check if there was the timeout
					if result == None:
						continue
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
						logger.info(result)
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
						raise SyntaxError("Unknown command: " + result)

					#-------------------------------------------------------
					# Print some information about the rendering progress
					#-------------------------------------------------------

					logger.info(statsLine)
			finally:
				session.Stop()
		except KeyboardInterrupt:
			raise
		except Exception as e:
			logger.exception(e)
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

	def __ThreadRun(self):
		# Start the broadcast beacon sender
		if self.broadcastAddress != "none":
			beacon = netbeacon.NetBeaconSender(self.address , self.port, self.broadcastAddress, self.broadcastPeriod)
			beacon.Start()
		else:
			beacon = None

		try:
			# Listen for connection
			with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as nodeSocket:
				nodeSocket.bind((self.address, self.port))
				nodeSocket.settimeout(0.2)
				nodeSocket.listen(0)

				logger.info("Waiting for a new connection")

				while not self.threadStop:
					try :
						clientSocket, addr = nodeSocket.accept()

						self.__HandleConnection(clientSocket, addr)

						logger.info("Waiting for a new connection")
					except socket.timeout:
						pass
					except KeyboardInterrupt:
						logger.info("KeyboardInterrupt received")
						break
		finally:
			if beacon:
				# Stop the broadcast beacon sender
				beacon.Stop()
