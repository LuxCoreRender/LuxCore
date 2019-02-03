## ======================================================================== ##
## Copyright 2009-2014 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

SET(INTEL_OIDN_HEADERS oidn.h oidn.hpp version.h)
FIND_PATH(INTEL_OIDN_INCLUDE_PATH
	NAMES ${INTEL_OIDN_HEADERS} 
	PATHS ${INTEL_OIDN_ROOT}/include)
IF (NOT INTEL_OIDN_INCLUDE_PATH)
	FIND_PATH(INTEL_OIDN_INCLUDE_PATH NAMES ${INTEL_OIDN_HEADERS} PATHS
		/usr/include
		/usr/local/include
		/opt/local/include)
ENDIF()

FIND_LIBRARY(INTEL_OIDN_LIBRARY NAMES libOpenImageDenoise.so libOpenImageDenoise.so.0 PATHS
	${INTEL_OIDN_ROOT}/lib
	NO_DEFAULT_PATH)
IF (NOT INTEL_OIDN_LIBRARY)
	FIND_LIBRARY(INTEL_OIDN_LIBRARY NAMES libOpenImageDenoise.so libOpenImageDenoise.so.0 PATHS
		/usr/lib 
		/usr/lib64
		/usr/local/lib 
		/opt/local/lib)
ENDIF()

IF (INTEL_OIDN_INCLUDE_PATH AND INTEL_OIDN_LIBRARY)
	SET(INTEL_OIDN_LIBRARY ${INTEL_OIDN_LIBRARY})
	SET(INTEL_OIDN_FOUND TRUE)
ENDIF()

MARK_AS_ADVANCED(
	INTEL_OIDN_INCLUDE_PATH
	INTEL_OIDN_LIBRARY
)
