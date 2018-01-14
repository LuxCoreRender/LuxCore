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

import argparse
import logging
import time

import pyluxcore
import pyluxcoretools.utils.loghandler
import pyluxcoretools.pyluxcorenetnode.netbeacon as netbeacon

logger = logging.getLogger(pyluxcoretools.utils.loghandler.loggerName + ".luxcorenetnode")

DEFAULT_PORT=18018

def LuxCoreNetNode(argv):
	parser = argparse.ArgumentParser(description="PyLuxCoreNetNode")
	parser.add_argument("-b", "--broadcast-address", metavar="BROADCAST_ADDRESS", default="<broadcast>",
						help="broadcast address to use")
	parser.add_argument("-t", "--broadcast-period", metavar="SECS", type=float,
						default=3.0,
						help="broadcast period")
	parser.add_argument("-a", "--address", metavar="IP_ADDRESS", default="",
						help="ip address to use")
	parser.add_argument("-p", "--port", metavar="PORT", type=int,
						default=DEFAULT_PORT,
						help="port to use")

	# Parse command line arguments
	args = parser.parse_args(argv)

	# Start the broadcast beacon sender
	beacon = netbeacon.NetBeaconSender(args.address, args.port, args.broadcast_address, args.broadcast_period)
	beacon.Start()

	while True:
		time.sleep(1.0)

	# Stop the broadcast beacon sender
	beacon.Stop()
	
	logger.info("Done.")

def main(argv):
	try:
		pyluxcore.Init(pyluxcoretools.utils.loghandler.LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())

		LuxCoreNetNode(argv[1:])
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
	main(sys.argv)
