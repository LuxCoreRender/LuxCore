###########################################################################
#   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 #
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

set(SLG_VERSION_MAJOR 1)
set(SLG_VERSION_MINOR 8beta2)

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
        set(DOXYGEN_SLG_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/slg.template)
        configure_file(
          "${DOXYGEN_SLG_TEMPLATE}.in"
          "${DOXYGEN_SLG_TEMPLATE}"
          )

        set(DOXYGEN_SLG_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-slg.conf)
        set(DOXYGEN_SLG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/slg)
        set(DOXYGEN_SLG_OUTPUT ${DOXYGEN_OUTPUT_DIR}/html/index.html)

        message(STATUS "Doxygen SLG output:" ${DOXYGEN_SLG_OUTPUT})

        if(DOXYGEN_DOT_FOUND)
                message(STATUS "Found dot")
                set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
        endif(DOXYGEN_DOT_FOUND)

        add_custom_command(
            OUTPUT ${DOXYGEN_SLG_OUTPUT}
            # Creating custom doxygen-slg.conf
			COMMAND mkdir -p ${DOXYGEN_SLG_OUTPUT_DIR}
            COMMAND cp ${DOXYGEN_SLG_TEMPLATE} ${DOXYGEN_SLG_INPUT}
            COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/smallluxgpu >> ${DOXYGEN_SLG_INPUT}
            COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_SLG_OUTPUT_DIR} >> ${DOXYGEN_SLG_INPUT}
            COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_SLG_INPUT}
            # Launch doxygen
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SLG_INPUT}
            DEPENDS ${DOXYGEN_SLG_TEMPLATE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
        )

        add_custom_target(apidoc_slg DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

###########################################################################
#
# Generate kernel files
#
###########################################################################

# PathGPU kernel

add_custom_command(
   OUTPUT pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND echo \"\#include \\"pathgpu/kernels/kernels.h\\"\" > pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU = " >> pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND cat pathgpu/kernels/pathgpu_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND echo "\;" >> pathgpu/kernels/pathgpu_kernel.cpp
   MAIN_DEPENDENCY pathgpu/kernels/pathgpu_kernel.cl
   )

# PathGPU2 kernel

add_custom_command(
   OUTPUT pathgpu2/kernels/pathgpu2_kernel_core.cpp
   COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_kernel_core.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU2_core = " >> pathgpu2/kernels/pathgpu2_kernel_core.cpp
   COMMAND cat pathgpu2/kernels/pathgpu2_kernel_core.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_kernel_core.cpp
   COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_kernel_core.cpp
   MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_kernel_core.cl
   )
add_custom_command(
   OUTPUT pathgpu2/kernels/pathgpu2_kernel_datatypes.cpp
   COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_kernel_datatypes.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU2_datatypes = " >> pathgpu2/kernels/pathgpu2_kernel_datatypes.cpp
   COMMAND cat pathgpu2/kernels/pathgpu2_kernel_datatypes.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_kernel_datatypes.cpp
   COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_kernel_datatypes.cpp
   MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_kernel_datatypes.cl
   )
add_custom_command(
   OUTPUT pathgpu2/kernels/pathgpu2_kernel_filters.cpp
   COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_kernel_filters.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU2_filters = " >> pathgpu2/kernels/pathgpu2_kernel_filters.cpp
   COMMAND cat pathgpu2/kernels/pathgpu2_kernel_filters.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_kernel_filters.cpp
   COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_kernel_filters.cpp
   MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_kernel_filters.cl
   )
add_custom_command(
   OUTPUT pathgpu2/kernels/pathgpu2_kernel_samplers.cpp
   COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_kernel_samplers.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU2_samplers = " >> pathgpu2/kernels/pathgpu2_kernel_samplers.cpp
   COMMAND cat pathgpu2/kernels/pathgpu2_kernel_samplers.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_kernel_samplers.cpp
   COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_kernel_samplers.cpp
   MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_kernel_samplers.cl
   )
add_custom_command(
   OUTPUT pathgpu2/kernels/pathgpu2_kernel_scene.cpp
   COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_kernel_scene.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU2_scene = " >> pathgpu2/kernels/pathgpu2_kernel_scene.cpp
   COMMAND cat pathgpu2/kernels/pathgpu2_kernel_scene.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_kernel_scene.cpp
   COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_kernel_scene.cpp
   MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_kernel_scene.cl
   )
add_custom_command(
   OUTPUT pathgpu2/kernels/pathgpu2_kernels.cpp
   COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_kernels.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU2_kernels = " >> pathgpu2/kernels/pathgpu2_kernels.cpp
   COMMAND cat pathgpu2/kernels/pathgpu2_kernels.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_kernels.cpp
   COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_kernels.cpp
   MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_kernels.cl
   )

#############################################################################
#
# SLG binary
#
#############################################################################

include_directories(${LUXRAYS_INCLUDE_DIR})
include_directories(".")
link_directories (${LUXRAYS_LIB_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../${LUXRAYS_BIN_DIR}")

set(SMALLLUXGPU_SRCS
	displayfunc.cpp
	path/path.cpp
	pathgpu/kernels/pathgpu_kernel.cpp
	pathgpu/pathgpu.cpp
	pathgpu2/kernels/pathgpu2_kernel_core.cpp
	pathgpu2/kernels/pathgpu2_kernel_datatypes.cpp
	pathgpu2/kernels/pathgpu2_kernel_filters.cpp
	pathgpu2/kernels/pathgpu2_kernel_samplers.cpp
	pathgpu2/kernels/pathgpu2_kernel_scene.cpp
	pathgpu2/kernels/pathgpu2_kernels.cpp
	pathgpu2/pathgpu2.cpp
	renderconfig.cpp
	slgscene.cpp
	smallluxgpu.cpp
    sppm/hashgrid.cpp
    sppm/hybridhashgrid.cpp
    sppm/hitpoints.cpp
    sppm/kdtree.cpp
    sppm/sppm.cpp
	telnet.cpp
	volume.cpp
	)

add_executable(slg ${SMALLLUXGPU_SRCS})
target_link_libraries(slg luxrays ${GLEW_LIBRARY} ${GLUT_LIBRARY} ${OPENGL_LIBRARY} ${OCL_LIBRARY} ${Boost_LIBRARIES} ${FREEIMAGE_LIBRARIES})
