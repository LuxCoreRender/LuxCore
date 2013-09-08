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
### SLG Library
###
#############################################################################
#############################################################################
#############################################################################

set(SLG_VERSION_MAJOR 4)
set(SLG_VERSION_MINOR 0devel1)

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${LuxRays_SOURCE_DIR}/include/slg/cfg.h.in"
  "${LuxRays_SOURCE_DIR}/include/slg/cfg.h"
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
	set(DOXYGEN_SLG_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/slg.template)
	configure_file(
	  "${DOXYGEN_SLG_TEMPLATE}.in"
	  "${DOXYGEN_SLG_TEMPLATE}"
	  )

	set(DOXYGEN_SLG_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-slg.conf)
	set(DOXYGEN_SLG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/slg)
	set(DOXYGEN_SLG_OUTPUT ${DOXYGEN_OUTPUT_DIR}/slg/html/index.html)

	message(STATUS "Doxygen SLG output: " ${DOXYGEN_SLG_OUTPUT})

	if(DOXYGEN_DOT_FOUND)
			message(STATUS "Found dot")
			set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
	endif(DOXYGEN_DOT_FOUND)

	add_custom_command(
		OUTPUT ${DOXYGEN_SLG_OUTPUT}
		# Creating custom doxygen-luxrays.conf
		COMMAND mkdir -p ${DOXYGEN_SLG_OUTPUT_DIR}
		COMMAND cp ${DOXYGEN_SLG_TEMPLATE} ${DOXYGEN_SLG_INPUT}
		COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/slg  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/slg >> ${DOXYGEN_SLG_INPUT}
		COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_SLG_OUTPUT_DIR} >> ${DOXYGEN_SLG_INPUT}
		COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_SLG_INPUT}
		# Launch doxygen
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SLG_INPUT}
		DEPENDS ${DOXYGEN_SLG_TEMPLATE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
	)

	add_custom_target(apidoc_slg DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

###########################################################################
#
# Generate kernel files
#
###########################################################################

set(SLG_KERNELS
	${LuxRays_SOURCE_DIR}/include/slg/core/mc_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/camera/camera_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/camera/camera_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/datatypes.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/pathocl_kernels.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/rtpathocl_kernels.cl
	${LuxRays_SOURCE_DIR}/include/slg/film/filter_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/film/filter_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sampler/sampler_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sampler/sampler_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/scene_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/mapping_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/mapping_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/hitpoint_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/light_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/light_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/bsdf_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/bsdf_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/material_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/material_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/texture_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/texture_funcs.cl
)

PreprocessOCLKernels(${LuxRays_SOURCE_DIR}/src/slg/kernels slg ${SLG_KERNELS})

#############################################################################
#
# SLG library
#
#############################################################################

set(SLG_SRCS
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidircpu/bidircpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidircpu/bidircputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirhybrid/bidirhybrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirhybrid/bidirhybridthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirhybrid/bidirstate.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirvmcpu/hashgrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/cbidirhybrid/cbidirhybrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/compiledscene.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/pathocl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/pathoclthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/rtpathocl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/rtpathoclthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/lightcpu/lightcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/lightcpu/lightcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathcpu/pathcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathcpu/pathcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/filesaver/filesaver.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathhybrid/pathhybrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathhybrid/pathhybridthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathhybrid/pathstate.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/biaspathcpu/biaspathcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/biaspathcpu/biaspathcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/film.cpp
	${LuxRays_SOURCE_DIR}/src/slg/rendersession.cpp
	${LuxRays_SOURCE_DIR}/src/slg/renderconfig.cpp
	${LuxRays_SOURCE_DIR}/src/slg/renderengine.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sampler/sampler.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sampler/sobol.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/bsdf.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/camera.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/extmeshcache.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/light.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/material.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/scene.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/sdl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/spd.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/texture.cpp
	${LuxRays_SOURCE_DIR}/src/slg/telnet/telnet.cpp
	${LuxRays_SOURCE_DIR}/src/slg/utils/convtest/pdiff/lpyramid.cpp
    ${LuxRays_SOURCE_DIR}/src/slg/utils/convtest/pdiff/metric.cpp
    ${LuxRays_SOURCE_DIR}/src/slg/utils/convtest/convtest.cpp
)
SOURCE_GROUP("Source Files\\SLG Library" FILES ${SLG_LIB_SRCS})	

set(SLG_SRCS_KERNELS
	${LuxRays_SOURCE_DIR}/src/slg/kernels/sampler_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/sampler_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/filter_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/filter_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/camera_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/camera_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/mc_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/bsdf_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/bsdf_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/material_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/material_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/light_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/light_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/scene_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/mapping_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/mapping_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/hitpoint_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/datatypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/pathocl_kernels_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/rtpathocl_kernels_kernel.cpp
)
SOURCE_GROUP("Source Files\\SLG Kernels" FILES ${SLG_SRCS_KERNELS})

set(SLG_LIB_SRCS
	${SLG_SRCS}
	${SLG_SRCS_KERNELS}
)

add_library(smallluxgpu STATIC ${SLG_LIB_SRCS})

link_directories (${LuxRays_LIB_DIR})

IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET smallluxgpu POST_BUILD
	COMMAND rm -rf ${OSX_DEPENDENCY_ROOT}/include/LuxRays/slg # clean first to assure update
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/libsmallluxgpu.a ${OSX_DEPENDENCY_ROOT}/lib/LuxRays
	COMMAND cp -R ${CMAKE_SOURCE_DIR}/include/slg ${OSX_DEPENDENCY_ROOT}/include/LuxRays
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
