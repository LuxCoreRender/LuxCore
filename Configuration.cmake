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
# Use cmake "-DLUXRAY_CUSTOM_CONFIG=YouFileCName" To define your personal settings
# where YouFileCName.cname must exist in one of the cmake include directories
# best to use cmake/SpecializedConfig/
# 
# To not load defaults before loading custom values define
# -DLUXRAYS_NO_DEFAULT_CONFIG=true
#
# WARNING: These variables will be cashed like any other
#
###########################################################################

IF (NOT LUXRAYS_NO_DEFAULT_CONFIG)

  # Disable Boost automatic linking
  ADD_DEFINITIONS(-DBOOST_ALL_NO_LIB)

	IF (WIN32)

	 	MESSAGE(STATUS "Using default WIN32 Configuration settings")
				
	  set(BUILD_LUXMARK TRUE) # This will require QT
			
	  set(ENV{QTDIR} "c:/qt/")
		
	  set(FREEIMAGE_SEARCH_PATH     "${LuxRays_SOURCE_DIR}/../FreeImage")
	 	set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/source")
	  set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/release"
		                                "${FREEIMAGE_SEARCH_PATH}/debug"
		                                "${FREEIMAGE_SEARCH_PATH}/dist")
			                              
	  set(BOOST_SEARCH_PATH         "${LuxRays_SOURCE_DIR}/../boost")
	  set(OPENCL_SEARCH_PATH        "${LuxRays_SOURCE_DIR}/../opencl")
	  set(GLUT_SEARCH_PATH          "${LuxRays_SOURCE_DIR}/../freeglut")
			
	ELSE(WIN32)
			
	
	ENDIF(WIN32)
	
ELSE(NOT LUXRAYS_NO_DEFAULT_CONFIG)
	
	MESSAGE(STATUS "LUXRAYS_NO_DEFAULT_CONFIG defined - not using default configuration values.")

ENDIF(NOT LUXRAYS_NO_DEFAULT_CONFIG)

#
# Overwrite defaults with Custom Settings
#
IF (LUXRAY_CUSTOM_CONFIG)
  MESSAGE(STATUS "Using custom build config: ${LUXRAY_CUSTOM_CONFIG}")
  INCLUDE(${LUXRAY_CUSTOM_CONFIG})
ENDIF (LUXRAY_CUSTOM_CONFIG)

