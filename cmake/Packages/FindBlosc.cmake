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

FIND_PATH(BLOSC_INCLUDE_PATH NAMES blosc.h PATHS
	${BLOSC_ROOT}/include)
IF (NOT BLOSC_INCLUDE_PATH)
	FIND_PATH(BLOSC_INCLUDE_PATH NAMES blosc.h PATHS
		/usr/include
		/usr/local/include
		/opt/local/include)
ENDIF()

FIND_LIBRARY(BLOSC_LIBRARY NAMES libblosc libblosc.a PATHS
	${BLOSC_ROOT}/lib/x64
	${BLOSC_ROOT}/lib
	${BLOSC_ROOT}/build
	NO_DEFAULT_PATH)
IF (NOT BLOSC_LIBRARY)
	FIND_LIBRARY(BLOSC_LIBRARY NAMES libblosc libblosc.a PATHS
		/usr/lib 
		/usr/lib64
		/usr/local/lib 
		/opt/local/lib)
ENDIF()

IF (BLOSC_INCLUDE_PATH AND BLOSC_LIBRARY)
	SET(BLOSC_LIBRARY ${BLOSC_LIBRARY} ${BLOSC_LIBRARY})
	SET(BLOSC_FOUND TRUE)
ENDIF()

MARK_AS_ADVANCED(
	BLOSC_INCLUDE_PATH
	BLOSC_LIBRARY
)

