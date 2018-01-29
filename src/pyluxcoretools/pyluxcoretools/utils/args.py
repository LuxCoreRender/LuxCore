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

def ArgvSplitter(argv, exts):
	result = []
	for arg in argv:
		result.append(arg)
		# Check if it is a .cfg, .flm or .rsm file
		if len(arg) > 0 and arg[0] != "-" and os.path.splitext(arg)[1] in exts:
			yield result
			result = []

	if len(result) > 0:
		raise SyntaxError("Unused command line arguments: " + str(result))
