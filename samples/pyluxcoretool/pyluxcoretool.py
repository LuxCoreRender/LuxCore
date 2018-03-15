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
import sys
import argparse

# To avoid .pyc files
sys.dont_write_bytecode = True

# For PyInstaller environment
for sysPath in sys.path:
	fileName = sysPath + "/pyluxcoretools.zip"
	if os.path.exists(fileName) and os.path.isfile(fileName):
		sys.path.append(fileName)
		break;
	
# For the develop environment
sys.path.append("./lib")
sys.path.append("./lib/pyluxcoretools.zip")

import pyluxcoretools.pyluxcoreconsole.cmd as consoleCmd
import pyluxcoretools.pyluxcoremerge.cmd as mergeCmd
import pyluxcoretools.pyluxcorenetconsole.cmd as netConsoleCmd
import pyluxcoretools.pyluxcorenetnode.cmd as netNodeCmd
import pyluxcoretools.pyluxcorenetnode.ui as netNodeUI
import pyluxcoretools.pyluxcoremenu.cmd as menuCmd

if __name__ == '__main__':
	# Prepare the render configuration options parser
	generalParser = argparse.ArgumentParser(description="PyLuxCoreTool", add_help=False)
	generalParser.add_argument("commandToExecute", default="menu", nargs='?',
							help="help, console, merge, netconsole, netnode or netnodeui")

	# Parse the general options
	(generalArgs, cmdArgv) = generalParser.parse_known_args()

	cmdArgv.insert(0, generalArgs.commandToExecute)

	if generalArgs.commandToExecute == "help":
		generalParser.print_help()
	elif generalArgs.commandToExecute == "console":
		consoleCmd.main(cmdArgv)
	elif generalArgs.commandToExecute == "merge":
		mergeCmd.main(cmdArgv)
	elif generalArgs.commandToExecute == "netconsole":
		netConsoleCmd.main(cmdArgv)
	elif generalArgs.commandToExecute == "netnode":
		netNodeCmd.main(cmdArgv)
	elif generalArgs.commandToExecute == "netnodeui":
		netNodeUI.main(cmdArgv)
	elif generalArgs.commandToExecute == "menu":
		menuCmd.main(cmdArgv)
	else:
		raise TypeError("Unknown command to execute: " + generalArgs.commandToExecute)
