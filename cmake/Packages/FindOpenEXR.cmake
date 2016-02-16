###########################################################################
#   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  #
#                                                                         #
#   This file is part of Lux.                                             #
#                                                                         #
#   Lux is free software; you can redistribute it and/or modify           #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 3 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   Lux is distributed in the hope that it will be useful,                #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program.  If not, see <http://www.gnu.org/licenses/>. #
#                                                                         #
#   Lux website: http://www.luxrender.net                                 #
###########################################################################

# - Try to find OpenEXR
# Library is first searched for in OPENEXR_ROOT
# Once done, this will define
#
#  OPENEXR_FOUND - system has OpenEXR
#  OPENEXR_INCLUDE_DIRS - the OpenEXR include directories
#  OPENEXR_LIBRARIES - link these to use OpenEXR

# Lookup user specified path first
SET(OpenEXR_TEST_HEADERS ImfXdr.h OpenEXRConfig.h IlmBaseConfig.h)
SET(OpenEXR_INC_SUFFIXES include/OpenEXR include Include Headers)
FIND_PATH(OPENEXR_INCLUDE_DIRS
	NAMES ${OpenEXR_TEST_HEADERS}
	PATHS "${OPENEXR_ROOT}"
	PATH_SUFFIXES ${OpenEXR_INC_SUFFIXES}
	NO_DEFAULT_PATH
	DOC "The directory where IlmBaseConfig.h resides"
)
FIND_PATH(OPENEXR_INCLUDE_DIRS
	NAMES ${OpenEXR_TEST_HEADERS}
	PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
	PATH_SUFFIXES ${OpenEXR_INC_SUFFIXES}
	DOC "The directory where IlmBaseConfig.h resides"
)

IF (OPENEXR_INCLUDE_DIRS)
# Lookup additional headers in case they are in subdirectories
SET(OpenEXR_MODULES Iex Imf half Imath IlmThread)
FOREACH(i ${OpenEXR_MODULES})
	FIND_PATH(OpenEXR_${i}_INCLUDE_DIR
		NAMES ${i}.h ${i}Header.h ${i}Math.h
		PATHS "${OPENEXR_INCLUDE_DIRS}" "${OPENEXR_INCLUDE_DIRS}/${i}" "${OPENEXR_INCLUDE_DIRS}/Ilm${i}"
		NO_DEFAULT_PATH
		DOC "The directory where ${i}.h resides"
	)
ENDFOREACH(i)
FOREACH(i ${OpenEXR_MODULES})
	IF (NOT OpenEXR_${i}_INCLUDE_DIR)
		SET(OpenEXR_${i}_INCLUDE_DIR)
	ENDIF (NOT OpenEXR_${i}_INCLUDE_DIR)
	IF ("${OpenEXR_${i}_INCLUDE_DIR}" STREQUAL "${OPENEXR_INCLUDE_DIRS}")
		SET(OpenEXR_${i}_INCLUDE_DIR "")
	ENDIF ("${OpenEXR_${i}_INCLUDE_DIR}" STREQUAL "${OPENEXR_INCLUDE_DIRS}")
ENDFOREACH(i)
FOREACH(i ${OpenEXR_MODULES})
	SET(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIRS} ${OpenEXR_${i}_INCLUDE_DIR})
ENDFOREACH(i)
ENDIF(OPENEXR_INCLUDE_DIRS)

SET(OpenEXR_LIBRARY_MODULES Iex IlmImf Half Imath IlmThread)
SET(OpenEXR_LIB_SUFFIXES lib64 lib Lib lib/OpenEXR Libs)
SET(OpenEXR_LIB_SUFFIXES_REL)
SET(OpenEXR_LIB_SUFFIXES_DBG)
FOREACH(i ${OpenEXR_LIB_SUFFIXES})
	SET(OpenEXR_LIB_SUFFIXES_REL ${OpenEXR_LIB_SUFFIXES_REL}
		"${i}" "${i}/release" "${i}/relwithdebinfo" "${i}/minsizerel" "${i}/dist")
	SET(OpenEXR_LIB_SUFFIXES_DBG ${OpenEXR_LIB_SUFFIXES_DBG}
		"${i}" "${i}/debug" "${i}/dist")
ENDFOREACH(i)
SET(OPENEXR_LIBRARIES)
FOREACH(i ${OpenEXR_LIBRARY_MODULES})
	FIND_LIBRARY(OpenEXR_${i}_LIBRARY_REL
		NAMES ${i}
		PATHS "${OPENEXR_ROOT}"
		PATH_SUFFIXES ${OpenEXR_LIB_SUFFIXES_REL}
		NO_DEFAULT_PATH
		DOC "The ${i} release library"
	)
	FIND_LIBRARY(OpenEXR_${i}_LIBRARY_REL
		NAMES ${i}
		PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
		PATH_SUFFIXES ${OpenEXR_LIB_SUFFIXES_REL}
		DOC "The ${i} release library"
	)
	FIND_LIBRARY(OpenEXR_${i}_LIBRARY_DBG
		NAMES "${i}d" "${i}D" "${i}_d" "${i}_D" "${i}_debug"
		PATHS "${OPENEXR_ROOT}"
		PATH_SUFFIXES ${OpenEXR_LIB_SUFFIXES_DBG}
		NO_DEFAULT_PATH
		DOC "The ${i} debug library"
	)
	FIND_LIBRARY(OpenEXR_${i}_LIBRARY_DBG
		NAMES "${i}d" "${i}D" "${i}_d" "${i}_D" "${i}_debug"
		PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
		PATH_SUFFIXES ${OpenEXR_LIB_SUFFIXES_DBG}
		DOC "The ${i} debug library"
	)
	IF (OpenEXR_${i}_LIBRARY_REL AND OpenEXR_${i}_LIBRARY_DBG)
		SET(OPENEXR_LIBRARIES ${OPENEXR_LIBRARIES}
			optimized ${OpenEXR_${i}_LIBRARY_REL}
			debug ${OpenEXR_${i}_LIBRARY_DBG})
	ELSEIF (OpenEXR_${i}_LIBRARY_REL)
		SET(OPENEXR_LIBRARIES ${OPENEXR_LIBRARIES}
			${OpenEXR_${i}_LIBRARY_REL})
	ELSEIF (OpenEXR_${i}_LIBRARY_DBG)
		SET(OPENEXR_LIBRARIES ${OPENEXR_LIBRARIES}
			${OpenEXR_${i}_LIBRARY_DBG})
	ENDIF (OpenEXR_${i}_LIBRARY_REL AND OpenEXR_${i}_LIBRARY_DBG)
ENDFOREACH (i)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENEXR  DEFAULT_MSG  OPENEXR_LIBRARIES OPENEXR_INCLUDE_DIRS)

MARK_AS_ADVANCED(OPENEXR_INCLUDE_DIRS OPENEXR_LIBRARIES)

