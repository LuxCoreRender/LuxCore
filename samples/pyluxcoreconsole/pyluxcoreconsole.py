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

import sys

# For PyInstaller environment
for sysPath in sys.path:
	fileName = sysPath + "/pyluxcoretools.zip"
	if os.path.exists(fileName) and os.path.isfile(fileName):
		sys.path.append(fileName)
		break;
	
# For the develop environment
sys.path.append("./lib")
sys.path.append("./lib/pyluxcoretools.zip")

import pyluxcoretools.pyluxcoreconsole.cmd as cmd

if __name__ == '__main__':
	cmd.main(sys.argv)
