################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
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

# This file was part of the CMake build system for OGRE

# - Try to find FreeImage
# Once done, this will define
#
#  FreeImage_FOUND - system has FreeImage
#  FreeImage_INCLUDE_DIRS - the FreeImage include directories 
#  FreeImage_LIBRARIES - link these to use FreeImage

include(FindPkgMacros)
findpkg_begin(FreeImage)

# Get path, convert backslashes as ${ENV_${var}}
getenv_path(FREEIMAGE_HOME)

# construct search paths
set(FreeImage_PREFIX_PATH ${FREEIMAGE_HOME} ${ENV_FREEIMAGE_HOME})
create_search_paths(FreeImage)
# redo search if prefix path changed
clear_if_changed(FreeImage_PREFIX_PATH
  FreeImage_LIBRARY_FWK
  FreeImage_LIBRARY_REL
  FreeImage_LIBRARY_DBG
  FreeImage_INCLUDE_DIR
)

set(FreeImage_LIBRARY_NAMES FreeImage freeimage FreeImageLib freeimageLib)
get_debug_names(FreeImage_LIBRARY_NAMES)

use_pkgconfig(FreeImage_PKGC freeimage)

findpkg_framework(FreeImage)

find_path(FreeImage_INCLUDE_DIR NAMES FreeImage.h HINTS ${FreeImage_INC_SEARCH_PATH} ${FreeImage_PKGC_INCLUDE_DIRS})
MESSAGE(STATUS "FreeImage_INCLUDE_DIR NAMES " ${FreeImage_INCLUDE_DIR})

find_library(FreeImage_LIBRARY_REL NAMES ${FreeImage_LIBRARY_NAMES} HINTS ${FreeImage_LIB_SEARCH_PATH} ${FreeImage_PKGC_LIBRARY_DIRS} PATH_SUFFIXES "" release relwithdebinfo minsizerel dist)
find_library(FreeImage_LIBRARY_DBG NAMES ${FreeImage_LIBRARY_NAMES_DBG} HINTS ${FreeImage_LIB_SEARCH_PATH} ${FreeImage_PKGC_LIBRARY_DIRS} PATH_SUFFIXES "" debug dist)

make_library_set(FreeImage_LIBRARY)
MESSAGE(STATUS "FreeImage_LIBRARY_DBG NAMES " ${FreeImage_LIBRARY_NAMES})

findpkg_finish(FreeImage)


#
# Try to find the FreeImage library and include path.
# Once done this will define
#
# FREEIMAGE_FOUND
# FREEIMAGE_INCLUDE_PATH
# FREEIMAGE_LIBRARY
# 

IF (WIN32)
    FIND_PATH( FREEIMAGE_INCLUDE_PATH FreeImage.h
        ${FREEIMAGE_ROOT_DIR}/include
        ${FREEIMAGE_ROOT_DIR}
        DOC "The directory where FreeImage.h resides")
    FIND_LIBRARY( FREEIMAGE_LIBRARY
        NAMES FreeImage freeimage
        PATHS
        ${FREEIMAGE_ROOT_DIR}/lib
        ${FREEIMAGE_ROOT_DIR}
        DOC "The FreeImage library")
ELSE (WIN32)
    FIND_PATH( FREEIMAGE_INCLUDE_PATH FreeImage.h
        /usr/include
        /usr/local/include
        /sw/include
        /opt/local/include
        DOC "The directory where FreeImage.h resides")
    FIND_LIBRARY( FREEIMAGE_LIBRARY
        NAMES FreeImage freeimage
        PATHS
        /usr/lib64
        /usr/lib
        /usr/local/lib64
        /usr/local/lib
        /sw/lib
        /opt/local/lib
        DOC "The FreeImage library")
ENDIF (WIN32)

SET(FREEIMAGE_LIBRARIES ${FREEIMAGE_LIBRARY})

IF (FREEIMAGE_INCLUDE_PATH AND FREEIMAGE_LIBRARY)
    SET(FREEIMAGE_FOUND TRUE)
ELSE (FREEIMAGE_INCLUDE_PATH AND FREEIMAGE_LIBRARY)
    SET(FREEIMAGE_FOUND FALSE)
ENDIF (FREEIMAGE_INCLUDE_PATH AND FREEIMAGE_LIBRARY)

MARK_AS_ADVANCED(FREEIMAGE_LIBRARY FREEIMAGE_LIBRARIES FREEIMAGE_INCLUDE_PATH)
