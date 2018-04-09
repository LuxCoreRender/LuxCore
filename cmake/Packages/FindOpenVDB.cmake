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

FIND_PATH(OPENVDB_INCLUDE_PATH NAMES openvdb/openvdb.h PATHS
	${OPENVDB_ROOT}/include)
IF (NOT OPENVDB_INCLUDE_PATH)
	FIND_PATH(OPENVDB_INCLUDE_PATH NAMES openvdb/openvdb.h PATHS
		/usr/include
		/usr/local/include
		/opt/local/include)
ENDIF()

FIND_LIBRARY(OPENVDB_LIBRARY NAMES openvdb openvdb.so PATHS
	${OPENVDB_ROOT}/lib/x64
	${OPENVDB_ROOT}/lib
	${OPENVDB_ROOT}/build
	NO_DEFAULT_PATH)
IF (NOT OPENVDB_LIBRARY)
	FIND_LIBRARY(OPENVDB_LIBRARY NAMES openvdb openvdb.so PATHS
		/usr/lib 
		/usr/lib64
		/usr/local/lib 
		/opt/local/lib)
ENDIF()

IF (OPENVDB_INCLUDE_PATH AND OPENVDB_LIBRARY AND OPENVDB_LIBRARY)
	SET(OPENVDB_LIBRARY ${OPENVDB_LIBRARY} ${OPENVDB_LIBRARY})
	SET(OPENVDB_FOUND TRUE)
ENDIF()

MARK_AS_ADVANCED(
	OPENVDB_INCLUDE_PATH
	OPENVDB_LIBRARY
)