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

# Use relative paths
# This is mostly to reduce path size for command-line limits on windows
if(WIN32)
  # This seems to break Xcode projects so definitely don't enable on Apple builds
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
endif(WIN32)


###########################################################################
#
# Compiler Flags
#
###########################################################################

IF(MSVC)
	message(STATUS "MSVC")

	# Change warning level to something saner
  # Force to always compile with W3
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
  endif()

	IF(MSVC10)
		message(STATUS "Version 10")
		# Whole Program Opt. gui display fixed in cmake 2.8.5
		# See http://public.kitware.com/Bug/view.php?id=6794
		# /GL will be used to build the code but the selection is not displayed in the menu

		set(MSVC_RELEASE_COMPILER_FLAGS "/WX- /MP /Ox /Ob2 /Oi /Oy /GT /GL /Gm- /EHsc /MD /GS /arch:SSE2 /fp:precise /Zc:wchar_t /Zc:forScope /GR /openmp /Gd /TP /GL /GF /Ot")
		set(MSVC_RELEASE_WITH_DEBUG_COMPILER_FLAGS "/Zi")

		set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   ${MSVC_RELEASE_COMPILER_FLAGS} ${MSVC_RELEASE_WITH_DEBUG_COMPILER_FLAGS}")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${MSVC_RELEASE_COMPILER_FLAGS} ${MSVC_RELEASE_WITH_DEBUG_COMPILER_FLAGS}")
		
		#set(MSVC_RELEASE_LINKER_FLAGS "/LTCG /OPT:REF /OPT:ICF")
		#set(MSVC_RELEASE_WITH_DEBUG_LINKER_FLAGS "/DEBUG")
		#set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${MSVC_RELEASE_LINKER_FLAGS} ${MSVC_RELEASE_WITH_DEBUG_LINKER_FLAGS}")

		# currently not in release version but should be soon - in meantime linker will inform you about switching this flag automatically because of /GL
		set(MSVC_RELEASE_LINKER_FLAGS "/LTCG")
		set(STATIC_LIBRARY_FLAGS_RELEASE "${STATIC_LIBRARY_FLAGS_RELEASE} ${MSVC_RELEASE_LINKER_FLAGS}")

		# Definitions.
		ADD_DEFINITIONS(-D__SSE2__ -D__SSE__ -D__MMX__)
		ADD_DEFINITIONS(-D_UNICODE -DUNICODE)
		ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE)

	ENDIF(MSVC10) 	
ENDIF(MSVC)



IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")

  SET(CMAKE_CXX_FLAGS_DEBUG "-Wall -msse -msse2 -msse3 -mssse3 -fPIC -O0 -g")
  SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
  
ENDIF()


# Setting Universal Binary Properties, only for Mac OS X
#  generate with xcode/crosscompile, setting: ( darwin - 10.6 - gcc - g++ - MacOSX10.6.sdk - Find from root, then native system )
IF(APPLE)
	IF(COMMAND cmake_policy)
		IF(CMAKE_VERSION VERSION_LESS 2.8.1)
			cmake_policy(SET CMP0003 NEW)
		ELSE(CMAKE_VERSION VERSION_LESS 2.8.1)
			cmake_policy(SET CMP0015 NEW)
		ENDIF(CMAKE_VERSION VERSION_LESS 2.8.1)
	ENDIF(COMMAND cmake_policy)

	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.6)
	if(CMAKE_VERSION VERSION_LESS 2.8.1)
		SET(CMAKE_OSX_ARCHITECTURES i386;x86_64)
	else(CMAKE_VERSION VERSION_LESS 2.8.1)
		SET(CMAKE_XCODE_ATTRIBUTE_ARCHS i386\ x86_64)
	endif(CMAKE_VERSION VERSION_LESS 2.8.1)
	set(CMAKE_CONFIGURATION_TYPES Release)
	set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.6.sdk)
	INCLUDE_DIRECTORIES( ../macos/include )
	set(BUILD_LUXMARK on)

	#OSX-flags by jensverwiebe
	set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
	
	# Do not set "-cl-fast-relaxed-math -cl-mad-enable" as they change the precision and behaviour of floating point math!

	MESSAGE(STATUS "")
	MESSAGE(STATUS "################ GENERATED XCODE PROJECT INFORMATION ################")
	MESSAGE(STATUS "")
	MESSAGE(STATUS "OSX_DEPLOYMENT_TARGET : " ${CMAKE_OSX_DEPLOYMENT_TARGET})
	IF(CMAKE_VERSION VERSION_LESS 2.8.1)
		MESSAGE(STATUS "Setting CMAKE_OSX_ARCHITECTURES ( cmake lower 2.8 method ): " ${CMAKE_OSX_ARCHITECTURES})
	ELSE(CMAKE_VERSION VERSION_LESS 2.8.1)
		MESSAGE(STATUS "CMAKE_XCODE_ATTRIBUTE_ARCHS ( cmake 2.8 or higher method ): " ${CMAKE_XCODE_ATTRIBUTE_ARCHS})
	ENDIF(CMAKE_VERSION VERSION_LESS 2.8.1)
	MESSAGE(STATUS "OSX SDK SETTING : " ${CMAKE_OSX_SYSROOT})
	MESSAGE(STATUS "BUILD_CONFIGURATION_TYPE : " ${CMAKE_CONFIGURATION_TYPES})
	MESSAGE(STATUS "")
	MESSAGE(STATUS "#####################################################################")

ENDIF(APPLE)
