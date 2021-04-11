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

FIND_PATH(OIDN_INCLUDE_PATH NAMES OpenImageDenoise/version.h OpenImageDenoise/config.h PATHS
	${OIDN_ROOT}/include)
IF (NOT OIDN_INCLUDE_PATH)
	FIND_PATH(OIDN_INCLUDE_PATH NAMES OpenImageDenoise/version.h OpenImageDenoise/config.h PATHS
		/usr/include
		/usr/local/include
		/opt/local/include)
ENDIF()

FIND_LIBRARY(OIDN_LIBRARY NAMES OpenImageDenoise libOpenImageDenoise.so.0 PATHS
	${OIDN_ROOT}/lib/x64
	${OIDN_ROOT}/lib
	${OIDN_ROOT}/build
	NO_DEFAULT_PATH)
IF (NOT OIDN_LIBRARY)
	FIND_LIBRARY(OIDN_LIBRARY NAMES OpenImageDenoise libOpenImageDenoise.so.0 PATHS
		/usr/lib 
		/usr/lib64
		/usr/local/lib 
		/opt/local/lib)
ENDIF()

IF (OIDN_INCLUDE_PATH AND OIDN_LIBRARY)
	SET(OIDN_LIBRARY ${OIDN_LIBRARY})
	SET(OIDN_FOUND TRUE)
ENDIF()

MARK_AS_ADVANCED(
	OIDN_INCLUDE_PATH
	OIDN_LIBRARY
)
