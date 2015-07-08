################################################################################
# Copyright 1998-2015 by authors (see AUTHORS.txt)
#
#   This file is part of LuxRender.
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

# Try to find OpenSubdiv library and include path.

FIND_PATH(OPENSUBDIV_INCLUDE_PATH opensubdiv/version.h
	${OPENSUBDIV_ROOT}
	/usr/include
	/usr/local/include
	/sw/include
	/opt/local/include
	DOC "The directory where opensubdiv/version.h resides")
FIND_LIBRARY(OPENSUBDIV_LIBRARY
	NAMES osdCPU
	PATHS
	${OPENSUBDIV_LIBRARYDIR}
	/usr/lib64
	/usr/lib
	/usr/local/lib64
	/usr/local/lib
	/sw/lib
	/opt/local/lib
	DOC "The OpenSubdiv library")

IF (OPENSUBDIV_INCLUDE_PATH)
	SET(OPENSUBDIV_FOUND 1 CACHE STRING "Set to 1 if OpenSubdiv is found, 0 otherwise")
ELSE (OPENSUBDIV_INCLUDE_PATH)
	SET(OPENSUBDIV_FOUND 0 CACHE STRING "Set to 1 if OpenSubdiv is found, 0 otherwise")
ENDIF (OPENSUBDIV_INCLUDE_PATH)

MARK_AS_ADVANCED(OPENSUBDIV_FOUND)
