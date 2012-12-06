###########################################################################
#   Copyright (C) 1998-2011 by authors (see AUTHORS.txt )                 #
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

# Try to find OpenCL library and include path.
# Once done this will define
#
# OPENCL_FOUND
# OPENCL_INCLUDE_DIR
# OPENCL_LIBRARIES
# 

# Lookup user provide path first
SET(OPENCL_INC_SUFFIXES "" include/opencl cuda/include include Include Headers Dist Source)
FIND_PATH(OPENCL_INCLUDE_DIR
	NAMES CL/cl.hpp OpenCL/cl.hpp
	PATHS ${OPENCL_ROOT}
	PATH_SUFFIXES ${OPENCL_INC_SUFFIXES}
	DOC "The directory where CL/cl.hpp resides")
FIND_PATH(OPENCL_INCLUDE_DIR
	NAMES CL/cl.hpp OpenCL/cl.hpp
	PATHS /usr /usr/local /sw /opt/local $ENV{ATISTREAMSDKROOT} $ENV{AMDAPPSDKROOT} $ENV{CUDA_PATH} $ENV{INTELOCLSDKROOT}
	PATH_SUFFIXES ${OPENCL_INC_SUFFIXES}
	DOC "The directory where CL/cl.hpp resides")
FIND_PATH(OPENCL_C_INCLUDE_DIR
	NAMES CL/opencl.h OpenCL/opencl.h
	PATHS $ENV{ATISTREAMSDKROOT} $ENV{AMDAPPSDKROOT} $ENV{CUDA_PATH} $ENV{INTELOCLSDKROOT}
	PATH_SUFFIXES ${OPENCL_INC_SUFFIXES}
	DOC "The directory where CL/opencl.h resides")
SET(OPENCL_NAMES_REL opencl OpenCL)
SET(OPENCL_LIB_SUFFIXES "" lib/x86_64 lib/x86 lib64 lib Lib lib/opencl Libs Dist Release Debug lib/x64 lib/Win32)
SET(OPENCL_LIB_SUFFIXES_REL)
SET(OPENCL_LIB_SUFFIXES_DBG)
FOREACH(i ${OPENCL_LIB_SUFFIXES})
	SET(OPENCL_LIB_SUFFIXES_REL ${OPENCL_LIB_SUFFIXES_REL}
		"${i}" "${i}/release" "${i}/relwithdebinfo" "${i}/minsizerel" "${i}/dist")
	SET(OPENCL_LIB_SUFFIXES_DBG ${OPENCL_LIB_SUFFIXES_DBG}
		"${i}" "${i}/debug" "${i}/dist")
ENDFOREACH(i)
SET(OPENCL_NAMES_DBG)
FOREACH(i ${OPENCL_NAMES_REL})
	SET(OPENCL_NAMES_DBG ${OPENCL_NAMES_DBG} "${i}d" "${i}D" "${i}_d" "${i}_D" "${i}_debug")
ENDFOREACH(i)
FIND_LIBRARY(OPENCL_LIBRARY_REL
	NAMES ${OPENCL_NAMES_REL}
	PATHS "${OPENCL_ROOT}"
	PATH_SUFFIXES ${OPENCL_LIB_SUFFIXES_REL}
	NO_DEFAULT_PATH
	DOC "The OpenCL release library"
)
FIND_LIBRARY(OPENCL_LIBRARY_REL
	NAMES ${OPENCL_NAMES_REL}
	PATHS /usr/local /usr /sw /opt/local /opt/csw /opt $ENV{ATISTREAMSDKROOT} $ENV{AMDAPPSDKROOT} $ENV{CUDA_PATH} $ENV{INTELOCLSDKROOT}
	PATH_SUFFIXES ${OPENCL_LIB_SUFFIXES_REL}
	DOC "The OpenCL release library"
)
FIND_LIBRARY(OPENCL_LIBRARY_DBG
	NAMES ${OPENCL_NAMES_DBG}
	PATHS "${OPENCL_ROOT}"
	PATH_SUFFIXES ${OPENCL_LIB_SUFFIXES_DBG}
	NO_DEFAULT_PATH
	DOC "The OpenCL debug library"
)
FIND_LIBRARY(OPENCL_LIBRARY_DBG
	NAMES ${OPENCL_NAMES_DBG}
	PATHS /usr/local /usr /sw /opt/local /opt/csw /opt $ENV{ATISTREAMSDKROOT} $ENV{AMDAPPSDKROOT} $ENV{CUDA_PATH} $ENV{INTELOCLSDKROOT}
	PATH_SUFFIXES ${OPENCL_LIB_SUFFIXES_DBG}
	DOC "The OpenCL debug library"
)
IF (OPENCL_LIBRARY_REL AND OPENCL_LIBRARY_DBG)
	SET(OPENCL_LIBRARIES
		optimized ${OPENCL_LIBRARY_REL}
		debug ${OPENCL_LIBRARY_DBG})
ELSEIF (OPENCL_LIBRARY_REL)
	SET(OPENCL_LIBRARIES ${OPENCL_LIBRARY_REL})
ELSEIF (OPENCL_LIBRARY_DBG)
	SET(OPENCL_LIBRARIES ${OPENCL_LIBRARY_DBG})
ENDIF (OPENCL_LIBRARY_REL AND OPENCL_LIBRARY_DBG)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENCL  DEFAULT_MSG  OPENCL_LIBRARIES OPENCL_INCLUDE_DIR OPENCL_C_INCLUDE_DIR)

MARK_AS_ADVANCED(OPENCL_LIBRARIES OPENCL_INCLUDE_DIR OPENCL_C_INCLUDE_DIR)
