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
import argparse
import logging
import functools

import pyluxcore
import pyluxcoretools.renderfarm.renderfarm as renderfarm
import pyluxcoretools.utils.loghandler as loghandler
import pyluxcoretools.utils.netbeacon as netbeacon

logger = logging.getLogger(loghandler.loggerName + ".luxcorenetconsole")

class LuxCoreNetConsole:
	def NodeDiscoveryCallBack(self, ipAddress, port):
		self.renderFarm.DiscoveredNode(ipAddress, port, renderfarm.NodeDiscoveryType.AUTO_DISCOVERED)

	def Exec(self, argv):
		parser = argparse.ArgumentParser(description="PyLuxCoreNetConsole")
		parser.add_argument("fileToRender",
							help=".bcf file to render")

		# Parse command line arguments
		args = parser.parse_args(argv)
		if (not args.fileToRender):
			raise TypeError("File to render must be specified")
		configFileNameExt = os.path.splitext(args.fileToRender)[1]
		if (configFileNameExt != ".bcf"):
			raise TypeError("File to render must a .bcf format")

		# Create the render farm
		self.renderFarm = renderfarm.RenderFarm()

		# Create the render farm job
		renderFarmJob = renderfarm.RenderFarmJob(args.fileToRender)
		self.renderFarm.AddJob(renderFarmJob)

		# Start the beacon receiver
		beacon = netbeacon.NetBeaconReceiver(functools.partial(LuxCoreNetConsole.NodeDiscoveryCallBack, self))
		beacon.Start()

		self.renderFarm.HasDone()

		# Start the beacon receiver
		beacon.Stop()

		logger.info("Done.")

def main(argv):
	try:
		pyluxcore.Init(loghandler.LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())

		netConsole = LuxCoreNetConsole()
		netConsole.Exec(argv[1:])
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
	main(sys.argv)
