###########################################################################
#   Copyright (C) 1998-2011 by authors (see AUTHORS.txt )                 #
#                                                                         #
#   This file is part of LuxRays.                                         #
#                                                                         #
#   LuxRays is free software; you can redistribute it and/or modify       #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 3 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   LuxRays is distributed in the hope that it will be useful,            #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program.  If not, see <http://www.gnu.org/licenses/>. #
#                                                                         #
#   LuxRays website: http://www.luxrender.net                             #
###########################################################################

###########################################################################
#
# Configuration
#
# Use cmake "-DLUXRAYS_CUSTOM_CONFIG=YouFileCName" To define your personal settings
# where YouFileCName.cname must exist in one of the cmake include directories
# best to use cmake/SpecializedConfig/
# 
# To not load defaults before loading custom values define
# -DLUXRAYS_NO_DEFAULT_CONFIG=true
#
# WARNING: These variables will be cached like any other
#
###########################################################################

IF (NOT LUXRAYS_NO_DEFAULT_CONFIG)

  # Disable Boost automatic linking
  ADD_DEFINITIONS(-DBOOST_ALL_NO_LIB)

  IF (WIN32)

    MESSAGE(STATUS "Using default WIN32 Configuration settings")

    SET(BUILD_LUXMARK TRUE) # This will require QT

    IF(MSVC)

      STRING(REGEX MATCH "(Win64)" _carch_x64 ${CMAKE_GENERATOR})
      IF(_carch_x64)
        SET(WINDOWS_ARCH "x64")
      ELSE(_carch_x64)
        SET(WINDOWS_ARCH "x86")
      ENDIF(_carch_x64)
      MESSAGE(STATUS "Building for target ${WINDOWS_ARCH}")

      IF(DEFINED ENV{LUX_WINDOWS_BUILD_ROOT})
        MESSAGE(STATUS "Lux build environment variables found")

        SET(OPENCL_SEARCH_PATH    "$ENV{LUX_WINDOWS_BUILD_ROOT}/include")
        SET(BOOST_SEARCH_PATH     "$ENV{LUX_${WINDOWS_ARCH}_BOOST_ROOT}")
        SET(GLUT_SEARCH_PATH      "$ENV{LUX_${WINDOWS_ARCH}_GLUT_ROOT}")
        SET(GLEW_SEARCH_PATH      "$ENV{LUX_${WINDOWS_ARCH}_GLEW_ROOT}")
        SET(FREEIMAGE_SEARCH_PATH "$ENV{LUX_${WINDOWS_ARCH}_FREEIMAGE_ROOT}/FreeImage")
	SET(QT_QMAKE_EXECUTABLE   "$ENV{LUX_${WINDOWS_ARCH}_QT_ROOT}/bin/qmake.exe")
      ENDIF(DEFINED ENV{LUX_WINDOWS_BUILD_ROOT})

    ELSE(MSVC)

      SET(ENV{QTDIR} "c:/qt/")

      SET(FREEIMAGE_SEARCH_PATH     "${LuxRays_SOURCE_DIR}/../FreeImage")
      SET(BOOST_SEARCH_PATH         "${LuxRays_SOURCE_DIR}/../boost")
      SET(OPENCL_SEARCH_PATH        "${LuxRays_SOURCE_DIR}/../opencl")
      SET(GLUT_SEARCH_PATH          "${LuxRays_SOURCE_DIR}/../freeglut")

    ENDIF(MSVC)

    SET(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/source")
    SET(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/release"
                                  "${FREEIMAGE_SEARCH_PATH}/debug"
                                  "${FREEIMAGE_SEARCH_PATH}/dist")

  ENDIF(WIN32)

ELSE(NOT LUXRAYS_NO_DEFAULT_CONFIG)
	
	MESSAGE(STATUS "LUXRAYS_NO_DEFAULT_CONFIG defined - not using default configuration values.")

ENDIF(NOT LUXRAYS_NO_DEFAULT_CONFIG)

# Setup libraries output directory
SET (LIBRARY_OUTPUT_PATH
   ${PROJECT_BINARY_DIR}/lib
   CACHE PATH
   "Single Directory for all Libraries"
   )

# Setup binaries output directory
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
   CACHE PATH
   "Single Directory for all binaries"
	)

#
# Overwrite defaults with Custom Settings
#
IF (LUXRAYS_CUSTOM_CONFIG)
	MESSAGE(STATUS "Using custom build config: ${LUXRAYS_CUSTOM_CONFIG}")
	INCLUDE(${LUXRAYS_CUSTOM_CONFIG})
ENDIF (LUXRAYS_CUSTOM_CONFIG)

