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

FIND_PATH(TBB_INCLUDE_PATH NAMES tbb/tbb.h PATHS
	${TBB_ROOT}/include)
IF (NOT TBB_INCLUDE_PATH)
	FIND_PATH(TBB_INCLUDE_PATH NAMES tbb/tbb.h PATHS
		/usr/include
		/usr/local/include
		/opt/local/include)
ENDIF()

FIND_LIBRARY(TBB_LIBRARY NAMES libtbb tbb tbb.so PATHS
	${TBB_ROOT}/lib/x64
	${TBB_ROOT}/lib
	${TBB_ROOT}/build
	NO_DEFAULT_PATH)
IF (NOT TBB_LIBRARY)
	FIND_LIBRARY(TBB_LIBRARY NAMES libtbb tbb tbb.so PATHS
		/usr/lib 
		/usr/lib64
		/usr/local/lib 
		/opt/local/lib)
ENDIF()

IF (TBB_INCLUDE_PATH AND TBB_LIBRARY)
	SET(TBB_LIBRARY ${TBB_LIBRARY} ${TBB_LIBRARY})
	SET(TBB_FOUND TRUE)
ENDIF()

MARK_AS_ADVANCED(
	TBB_INCLUDE_PATH
	TBB_LIBRARY
)
