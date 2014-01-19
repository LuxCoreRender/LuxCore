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

# - Try to find PNG
# Library is first searched for in PNG_ROOT
# Once done, this will define
#
#  PNG_FOUND - system has PNG
#  PNG_INCLUDE_DIRS - the PNG include directories
#  PNG_LIBRARIES - link these to use PNG

# PNG depends on Zlib
# Lookup user specified path first
SET(PNG_INC_SUFFIXES include/libpng include Include Headers)
FIND_PATH(ZLIB_INCLUDE_DIR
	NAMES zlib.h
	PATHS "${PNG_ROOT}" "${ZLIB_ROOT}"
	PATH_SUFFIXES ${PNG_INC_SUFFIXES}
	NO_DEFAULT_PATH
	DOC "The directory where zlib.h resides")
FIND_PATH(ZLIB_INCLUDE_DIR
	NAMES zlib.h
	PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
	"[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]"
	PATH_SUFFIXES ${PNG_INC_SUFFIXES}
	DOC "The directory where zlib.h resides")

SET(ZLIB_NAMES_REL z zlib zdll)
SET(PNG_LIB_SUFFIXES lib64 lib Lib lib/PNG Libs)
SET(PNG_LIB_SUFFIXES_REL)
SET(PNG_LIB_SUFFIXES_DBG)
FOREACH(i ${PNG_LIB_SUFFIXES})
	SET(PNG_LIB_SUFFIXES_REL ${PNG_LIB_SUFFIXES_REL}
		"${i}" "${i}/release" "${i}/relwithdebinfo" "${i}/minsizerel" "${i}/dist")
	SET(PNG_LIB_SUFFIXES_DBG ${PNG_LIB_SUFFIXES_DBG}
		"${i}" "${i}/debug" "${i}/dist")
ENDFOREACH(i)
SET(ZLIB_NAMES_DBG)
FOREACH(i ${ZLIB_NAMES_REL})
	SET(ZLIB_NAMES_DBG ${ZLIB_NAMES_DBG} "${i}d" "${i}D" "${i}_d" "${i}_D" "${i}_debug")
ENDFOREACH(i)
FIND_LIBRARY(ZLIB_LIBRARY_REL
	NAMES ${ZLIB_NAMES_REL}
	PATHS "${PNG_ROOT}" "${ZLIB_ROOT}"
	PATH_SUFFIXES ${PNG_LIB_SUFFIXES_REL}
	NO_DEFAULT_PATH
	DOC "The zlib release library"
)
FIND_LIBRARY(ZLIB_LIBRARY_REL
	NAMES ${ZLIB_NAMES_REL}
	PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
	"[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]"
	PATH_SUFFIXES ${PNG_LIB_SUFFIXES_REL}
	DOC "The zlib release library"
)
FIND_LIBRARY(ZLIB_LIBRARY_DBG
	NAMES ${ZLIB_NAMES_DBG}
	PATHS "${PNG_ROOT}" "${ZLIB_ROOT}"
	PATH_SUFFIXES ${PNG_LIB_SUFFIXES_DBG}
	NO_DEFAULT_PATH
	DOC "The zlib debug library"
)
FIND_LIBRARY(ZLIB_LIBRARY_DBG
	NAMES ${ZLIB_NAMES_DBG}
	PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
	"[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]"
	PATH_SUFFIXES ${PNG_LIB_SUFFIXES_DBG}
	DOC "The zlib debug library"
)
IF (ZLIB_LIBRARY_REL AND ZLIB_LIBRARY_DBG)
	SET(ZLIB_LIBRARIES
		optimized ${ZLIB_LIBRARY_REL}
		debug ${ZLIB_LIBRARY_DBG})
ELSEIF (ZLIB_LIBRARY_REL)
	SET(ZLIB_LIBRARIES ${ZLIB_LIBRARY_REL})
ELSEIF (ZLIB_LIBRARY_DBG)
	SET(ZLIB_LIBRARIES ${ZLIB_LIBRARY_DBG})
ENDIF (ZLIB_LIBRARY_REL AND ZLIB_LIBRARY_DBG)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZLIB  DEFAULT_MSG  ZLIB_LIBRARIES ZLIB_INCLUDE_DIR)

MARK_AS_ADVANCED(ZLIB_LIBRARIES ZLIB_INCLUDE_DIR)

IF (ZLIB_INCLUDE_DIR)
	FIND_PATH(PNG_INCLUDE_DIRS
		NAMES png.h pngconf.h
		PATHS "${PNG_ROOT}"
		PATH_SUFFIXES ${PNG_INC_SUFFIXES}
		NO_DEFAULT_PATH
		DOC "The directory where png.h resides")
	FIND_PATH(PNG_INCLUDE_DIRS
		NAMES png.h pngconf.h
		PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
		PATH_SUFFIXES ${PNG_INC_SUFFIXES}
		DOC "The directory where png.h resides")

	IF (PNG_INCLUDE_DIRS)
		IF (NOT "${PNG_INCLUDE_DIRS}" STREQUAL "${ZLIB_INCLUDE_DIR}")
			SET(PNG_INCLUDE_DIRS ${PNG_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIR})
		ENDIF (NOT "${PNG_INCLUDE_DIRS}" STREQUAL "${ZLIB_INCLUDE_DIR}")
	ENDIF (PNG_INCLUDE_DIRS)
ENDIF(ZLIB_INCLUDE_DIR)

IF (ZLIB_LIBRARIES)
	SET(PNG_NAMES_REL png libpng png14 libpng14 png12 libpng12)
	SET(PNG_NAMES_DBG)
	FOREACH(i ${PNG_NAMES_REL})
		SET(PNG_NAMES_DBG ${PNG_NAMES_DBG} "${i}d" "${i}D" "${i}_d" "${i}_D" "${i}_debug")
	ENDFOREACH(i)
	FIND_LIBRARY(PNG_LIBRARY_REL
		NAMES ${PNG_NAMES_REL}
		PATHS "${PNG_ROOT}"
		PATH_SUFFIXES ${PNG_LIB_SUFFIXES_REL}
		NO_DEFAULT_PATH
		DOC "The png release library"
	)
	FIND_LIBRARY(PNG_LIBRARY_REL
		NAMES ${PNG_NAMES_REL}
		PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
		PATH_SUFFIXES ${PNG_LIB_SUFFIXES_REL}
		DOC "The png release library"
	)
	FIND_LIBRARY(PNG_LIBRARY_DBG
		NAMES ${PNG_NAMES_DBG}
		PATHS "${PNG_ROOT}"
		PATH_SUFFIXES ${PNG_LIB_SUFFIXES_DBG}
		NO_DEFAULT_PATH
		DOC "The png debug library"
	)
	FIND_LIBRARY(PNG_LIBRARY_DBG
		NAMES ${PNG_NAMES_DBG}
		PATHS /usr/local /usr /sw /opt/local /opt/csw /opt
		PATH_SUFFIXES ${PNG_LIB_SUFFIXES_DBG}
		DOC "The png debug library"
	)
	IF (PNG_LIBRARY_REL AND PNG_LIBRARY_DBG)
		SET(PNG_LIBRARIES
			optimized ${ZLIB_LIBRARY_REL}
			debug ${ZLIB_LIBRARY_DBG})
	ELSEIF (PNG_LIBRARY_REL)
		SET(PNG_LIBRARIES ${PNG_LIBRARY_REL})
	ELSEIF (PNG_LIBRARY_DBG)
		SET(PNG_LIBRARIES ${PNG_LIBRARY_DBG})
	ENDIF (PNG_LIBRARY_REL AND PNG_LIBRARY_DBG)

	IF (PNG_LIBRARIES)
		SET(PNG_LIBRARIES ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
	ENDIF (PNG_LIBRARIES)
ENDIF (ZLIB_LIBRARIES)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(PNG  DEFAULT_MSG  PNG_LIBRARIES PNG_INCLUDE_DIRS)

MARK_AS_ADVANCED(PNG_INCLUDE_DIRS PNG_LIBRARIES)
