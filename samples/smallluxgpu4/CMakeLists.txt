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

set(SLG_VERSION_MAJOR 4)
set(SLG_VERSION_MINOR 0devel1)

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/slgcfg.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/slgcfg.h"
  )

#############################################################################
#
# SLG Docs
#
#############################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
	message(STATUS "Found Doxygen and generating SLG documentation")

	# Generate doxygen.template
	set(DOXYGEN_SLG_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/slg4.template)
	configure_file(
	  "${DOXYGEN_SLG_TEMPLATE}.in"
	  "${DOXYGEN_SLG_TEMPLATE}"
	  )

	set(DOXYGEN_SLG_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-slg4.conf)
	set(DOXYGEN_SLG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/slg4)
	set(DOXYGEN_SLG_OUTPUT ${DOXYGEN_OUTPUT_DIR}/html/index.html)

	message(STATUS "Doxygen SLG output: " ${DOXYGEN_SLG_OUTPUT})

	if(DOXYGEN_DOT_FOUND)
			message(STATUS "Found dot")
			set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
	endif(DOXYGEN_DOT_FOUND)

	add_custom_command(
		OUTPUT ${DOXYGEN_SLG_OUTPUT}
		# Creating custom doxygen-slg.conf
		COMMAND mkdir -p ${DOXYGEN_SLG_OUTPUT_DIR}
		COMMAND cp ${DOXYGEN_SLG_TEMPLATE} ${DOXYGEN_SLG_INPUT}
		COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/smallluxgpu4 >> ${DOXYGEN_SLG_INPUT}
		COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_SLG_OUTPUT_DIR} >> ${DOXYGEN_SLG_INPUT}
		COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_SLG_INPUT}
		# Launch doxygen
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SLG_INPUT}
		DEPENDS ${DOXYGEN_SLG_TEMPLATE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
	)

	add_custom_target(apidoc_slg4 DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

###########################################################################
#
# Generate kernel files
#
###########################################################################

set(SLG_KERNELS
	${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/datatypes.cl
	${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/filters.cl
	${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/samplers.cl
	${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_kernels.cl
)

PreprocessOCLKernels(${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels slg ${SLG_KERNELS})

#############################################################################
#
# SLG library
#
#############################################################################

include_directories(${LuxRays_INCLUDE_DIR})
include_directories(".")

set(SLG4LIB_SRCS
	rendersession.cpp
	renderconfig.cpp
	renderengine.cpp
	telnet.cpp
	bidircpu/bidircpu.cpp
	bidircpu/bidircputhread.cpp
	bidirhybrid/bidirhybrid.cpp
	bidirhybrid/bidirhybridthread.cpp
	bidirhybrid/bidirstate.cpp
	bidirvmcpu/bidirvmcpu.cpp
	bidirvmcpu/bidirvmcputhread.cpp
	bidirvmcpu/hashgrid.cpp
	cbidirhybrid/cbidirhybrid.cpp
	pathocl/kernels/datatypes_kernel.cpp
	pathocl/kernels/filters_kernel.cpp
	pathocl/kernels/samplers_kernel.cpp
	pathocl/kernels/pathocl_kernels_kernel.cpp
	pathocl/compiledscene.cpp
	pathocl/pathocl.cpp
	pathocl/pathoclthread.cpp
	lightcpu/lightcpu.cpp
	lightcpu/lightcputhread.cpp
	pathcpu/pathcpu.cpp
	pathcpu/pathcputhread.cpp
	filesaver/filesaver.cpp
	)

add_library(smallluxgpu4 STATIC ${SLG4LIB_SRCS})

link_directories (${LuxRays_LIB_DIR})

IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET smallluxgpu4 POST_BUILD
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/libsmallluxgpu4.a ${OSX_DEPENDENCY_ROOT}/lib/LuxRays
	# todo: copy also needed headers
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)

#############################################################################
#
# SLG library demo
#
#############################################################################

set(SLGLIBDEMO_SRCS
	slglibdemo.cpp
	)

add_executable(slg4libdemo ${SLGLIBDEMO_SRCS})

TARGET_LINK_LIBRARIES(slg4libdemo smallluxgpu4 luxrays)

#############################################################################
#
# SLG binary
#
#############################################################################

set(SMALLLUXGPU_SRCS
	smallluxgpu.cpp
	displayfunc.cpp
	)

add_executable(slg4 ${SMALLLUXGPU_SRCS})

TARGET_LINK_LIBRARIES(slg4 smallluxgpu4 luxrays ${GLEW_LIBRARY} ${GLUT_LIBRARY})

# This instructs FREEGLUT to emit a pragma for the static version
SET_TARGET_PROPERTIES(slg4 PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)
