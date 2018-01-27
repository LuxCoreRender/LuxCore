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

import pyluxcore
import pyluxcoretools.renderfarm.renderfarm as renderfarm
import pyluxcoretools.renderfarm.renderfarmnode as renderfarmnode
import pyluxcoretools.utils.loghandler as loghandler

logger = logging.getLogger(loghandler.loggerName + ".luxcorenetnode")

def LuxCoreNetNode(argv):
	parser = argparse.ArgumentParser(description="PyLuxCoreNetNode")
	parser.add_argument("-b", "--broadcast-address", metavar="BROADCAST_ADDRESS", default="<broadcast>",
						help="broadcast address to use ('none' to disable)")
	parser.add_argument("-t", "--broadcast-period", metavar="SECS", type=float,
						default=3.0,
						help="broadcast period")
	parser.add_argument("-a", "--address", metavar="IP_ADDRESS", default="",
						help="ip address to use")
	parser.add_argument("-p", "--port", metavar="PORT", type=int,
						default=renderfarm.DEFAULT_PORT,
						help="port to use")
	parser.add_argument("-D", "--define", metavar=("PROP_NAME", "VALUE"), nargs=2, action="append",
						help="assign a value to a property")

	# Parse command line arguments
	args = parser.parse_args(argv)

	cmdLineProp = pyluxcore.Properties()
	if (args.define):
		for (name, value) in args.define:
			cmdLineProp.Set(pyluxcore.Property(name, value))

	renderFarmNode = renderfarmnode.RenderFarmNode(args.address, args.port, args.broadcast_address, args.broadcast_period, cmdLineProp)
	renderFarmNode.Run()
	
	logger.info("Done.")

def main(argv):
	try:
		pyluxcore.Init(loghandler.LuxCoreLogHandler)
		logger.info("LuxCore %s" % pyluxcore.Version())

		LuxCoreNetNode(argv[1:])
	finally:
		pyluxcore.SetLogHandler(None)

if __name__ == "__main__":
	main(sys.argv)
