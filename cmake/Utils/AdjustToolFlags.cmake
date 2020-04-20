################################################################################
# Copyright 1998-2020 by authors (see AUTHORS.txt)
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

# Generally useful routines:

if (AdjustToolFlags_MODULE_INCLUDED)
	return()
else()
	set(AdjustToolFlags_MODULE_INCLUDED TRUE)
endif()

function(AdjustToolFlags) # ...vars [ADDITIONS ...val][REMOVALS ...regex]

	# Parse arguments:

	set(variables "")
	set(additions "")
	set(removals "")
	set(dstlist "variables")
	foreach (arg IN ITEMS ${ARGN})

		if (arg STREQUAL "ADDITIONS")
			set(dstlist additions)
		elseif (arg STREQUAL "REMOVALS")
			set(dstlist removals)
		else()
			list(APPEND ${dstlist} ${arg})
		endif()

	endforeach()

	# Whitespace-separated strings to cmake lists:
	string(REGEX REPLACE "[ \t]+" ";" removals "${removals}")
	string(REGEX REPLACE "[ \t]+" ";" additions "${additions}")

	foreach (variable IN LISTS variables)

		string(REGEX REPLACE "[ \t]+" ";" sclist "${${variable}}")

		foreach (pattern IN LISTS removals)
			list(FILTER sclist EXCLUDE REGEX ${pattern})
		endforeach()

		list(APPEND sclist ${additions})
		list(REMOVE_DUPLICATES sclist)

		# Join list with whitespace:
		string(REPLACE ";" " " readable "${sclist}")

		# Figure out whether the variable is cached yet and act accordingly
		# (if we don't cache it in, 'set' only creates a local that shadows
		# the cache variable, and in that case won't see it in the GUI):
		get_property(helpstring CACHE ${variable} PROPERTY HELPSTRING)
		if (helpstring)
			set(${variable} ${readable} CACHE STRING "${helpstring}" FORCE)
		else()
			set(${variable} ${readable} PARENT_SCOPE)
		endif()

	endforeach()

endfunction()
