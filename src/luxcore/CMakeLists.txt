###########################################################################
#   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  #
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

#############################################################################
#############################################################################
#############################################################################
###
### LuxCore Library
###
#############################################################################
#############################################################################
#############################################################################

set(LUXCORE_VERSION_MAJOR 1)
set(LUXCORE_VERSION_MINOR 4devel1)

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${LuxRays_SOURCE_DIR}/include/luxcore/cfg.h.in"
  "${LuxRays_SOURCE_DIR}/include/luxcore/cfg.h"
  )

#############################################################################
#
# LuxCore Docs
#
#############################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
	message(STATUS "Found Doxygen and generating LuxCore documentation")

	# Generate doxygen.template
	set(DOXYGEN_LUXCORE_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/luxcore.template)
	configure_file(
	  "${DOXYGEN_LUXCORE_TEMPLATE}.in"
	  "${DOXYGEN_LUXCORE_TEMPLATE}"
	  )

	set(DOXYGEN_LUXCORE_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-luxcore.conf)
	set(DOXYGEN_LUXCORE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/luxcore)
	set(DOXYGEN_LUXCORE_OUTPUT ${DOXYGEN_OUTPUT_DIR}/luxcore/html/index.html)

	message(STATUS "Doxygen LuxCore output: " ${DOXYGEN_LUXCORE_OUTPUT})

	if(DOXYGEN_DOT_FOUND)
			message(STATUS "Found dot")
			set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
	endif(DOXYGEN_DOT_FOUND)

	add_custom_command(
		OUTPUT ${DOXYGEN_LUXCORE_OUTPUT}
		# Creating custom doxygen-luxrays.conf
		COMMAND mkdir -p ${DOXYGEN_LUXCORE_OUTPUT_DIR}
		COMMAND cp ${DOXYGEN_LUXCORE_TEMPLATE} ${DOXYGEN_LUXCORE_INPUT}
		COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/luxcore  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/luxcore >> ${DOXYGEN_LUXCORE_INPUT}
		COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_LUXCORE_OUTPUT_DIR} >> ${DOXYGEN_LUXCORE_INPUT}
		COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_LUXCORE_INPUT}
		# Launch doxygen
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_LUXCORE_INPUT}
		DEPENDS ${DOXYGEN_LUXCORE_TEMPLATE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
	)

	add_custom_target(apidoc_luxcore DEPENDS ${DOXYGEN_LUXCORE_OUTPUT})
endif(DOXYGEN_FOUND)

#############################################################################
#
# LuxCore library
#
#############################################################################

set(LUXCORE_SRCS
	${LuxRays_SOURCE_DIR}/src/luxcore/luxcore.cpp
)
SOURCE_GROUP("Source Files\\LuxCore Library" FILES ${LUXCORE_LIB_SRCS})	

set(LUXCORE_LIB_SRCS
	${LUXCORE_SRCS}
)

add_library(luxcore STATIC ${LUXCORE_LIB_SRCS})

link_directories (${SLG_LIB_DIR} ${LuxRays_LIB_DIR})

IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET luxcore POST_BUILD
	COMMAND rm -rf ${OSX_DEPENDENCY_ROOT}/include/LuxRays/luxcore # clean first to assure update
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/libluxcore.a ${OSX_DEPENDENCY_ROOT}/lib/LuxRays
	COMMAND cp -R ${CMAKE_SOURCE_DIR}/include/luxcore ${OSX_DEPENDENCY_ROOT}/include/LuxRays
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)

#############################################################################
#
# PyLuxCore (Python bindings)
#
#############################################################################

set(PYLUXCORE_SRCS
	${LuxRays_SOURCE_DIR}/src/luxcore/pyluxcore.cpp
)

add_library(pyluxcore MODULE ${PYLUXCORE_SRCS})
target_link_libraries(pyluxcore luxcore smallluxgpu luxrays ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})
set_target_properties(pyluxcore PROPERTIES PREFIX "")
if(MSVC)
	set_target_properties(pyluxcore PROPERTIES SUFFIX ".pyd")
endif(MSVC)

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
