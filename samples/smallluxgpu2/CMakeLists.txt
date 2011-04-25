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

set(SLG_VERSION_MAJOR 2)
set(SLG_VERSION_MINOR 0beta1)

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
        set(DOXYGEN_SLG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/slg2)
        set(DOXYGEN_SLG_OUTPUT ${DOXYGEN_OUTPUT_DIR}/html/index.html)

        message(STATUS "Doxygen SLG2 output:" ${DOXYGEN_SLG_OUTPUT})

        if(DOXYGEN_DOT_FOUND)
                message(STATUS "Found dot")
                set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
        endif(DOXYGEN_DOT_FOUND)

        add_custom_command(
            OUTPUT ${DOXYGEN_SLG_OUTPUT}
            # Creating custom doxygen-slg.conf
			COMMAND mkdir -p ${DOXYGEN_SLG_OUTPUT_DIR}
            COMMAND cp ${DOXYGEN_SLG_TEMPLATE} ${DOXYGEN_SLG_INPUT}
            COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../samples/smallluxgpu2 >> ${DOXYGEN_SLG_INPUT}
            COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_SLG_OUTPUT_DIR} >> ${DOXYGEN_SLG_INPUT}
            COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_SLG_INPUT}
            # Launch doxygen
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SLG_INPUT}
            DEPENDS ${DOXYGEN_SLG_TEMPLATE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
        )

        add_custom_target(apidoc_slg2 DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

###########################################################################
#
# Generate kernel files
#
###########################################################################

# PathOCL kernel

add_custom_command(
   OUTPUT pathocl/kernels/pathocl_kernel_core.cpp
   COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > pathocl/kernels/pathocl_kernel_core.cpp
   COMMAND echo "std::string PathOCL::KernelSource_PathOCL_core = " >> pathocl/kernels/pathocl_kernel_core.cpp
   COMMAND cat pathocl/kernels/pathocl_kernel_core.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathocl/kernels/pathocl_kernel_core.cpp
   COMMAND echo "\;" >> pathocl/kernels/pathocl_kernel_core.cpp
   MAIN_DEPENDENCY pathocl/kernels/pathocl_kernel_core.cl
   )
add_custom_command(
   OUTPUT pathocl/kernels/pathocl_kernel_datatypes.cpp
   COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > pathocl/kernels/pathocl_kernel_datatypes.cpp
   COMMAND echo "std::string PathOCL::KernelSource_PathOCL_datatypes = " >> pathocl/kernels/pathocl_kernel_datatypes.cpp
   COMMAND cat pathocl/kernels/pathocl_kernel_datatypes.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathocl/kernels/pathocl_kernel_datatypes.cpp
   COMMAND echo "\;" >> pathocl/kernels/pathocl_kernel_datatypes.cpp
   MAIN_DEPENDENCY pathocl/kernels/pathocl_kernel_datatypes.cl
   )
add_custom_command(
   OUTPUT pathocl/kernels/pathocl_kernel_filters.cpp
   COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > pathocl/kernels/pathocl_kernel_filters.cpp
   COMMAND echo "std::string PathOCL::KernelSource_PathOCL_filters = " >> pathocl/kernels/pathocl_kernel_filters.cpp
   COMMAND cat pathocl/kernels/pathocl_kernel_filters.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathocl/kernels/pathocl_kernel_filters.cpp
   COMMAND echo "\;" >> pathocl/kernels/pathocl_kernel_filters.cpp
   MAIN_DEPENDENCY pathocl/kernels/pathocl_kernel_filters.cl
   )
add_custom_command(
   OUTPUT pathocl/kernels/pathocl_kernel_samplers.cpp
   COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > pathocl/kernels/pathocl_kernel_samplers.cpp
   COMMAND echo "std::string PathOCL::KernelSource_PathOCL_samplers = " >> pathocl/kernels/pathocl_kernel_samplers.cpp
   COMMAND cat pathocl/kernels/pathocl_kernel_samplers.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathocl/kernels/pathocl_kernel_samplers.cpp
   COMMAND echo "\;" >> pathocl/kernels/pathocl_kernel_samplers.cpp
   MAIN_DEPENDENCY pathocl/kernels/pathocl_kernel_samplers.cl
   )
add_custom_command(
   OUTPUT pathocl/kernels/pathocl_kernel_scene.cpp
   COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > pathocl/kernels/pathocl_kernel_scene.cpp
   COMMAND echo "std::string PathOCL::KernelSource_PathOCL_scene = " >> pathocl/kernels/pathocl_kernel_scene.cpp
   COMMAND cat pathocl/kernels/pathocl_kernel_scene.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathocl/kernels/pathocl_kernel_scene.cpp
   COMMAND echo "\;" >> pathocl/kernels/pathocl_kernel_scene.cpp
   MAIN_DEPENDENCY pathocl/kernels/pathocl_kernel_scene.cl
   )
add_custom_command(
   OUTPUT pathocl/kernels/pathocl_kernels.cpp
   COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > pathocl/kernels/pathocl_kernels.cpp
   COMMAND echo "std::string PathOCL::KernelSource_PathOCL_kernels = " >> pathocl/kernels/pathocl_kernels.cpp
   COMMAND cat pathocl/kernels/pathocl_kernels.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathocl/kernels/pathocl_kernels.cpp
   COMMAND echo "\;" >> pathocl/kernels/pathocl_kernels.cpp
   MAIN_DEPENDENCY pathocl/kernels/pathocl_kernels.cl
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
	pathocl/kernels/pathocl_kernel_core.cpp
	pathocl/kernels/pathocl_kernel_datatypes.cpp
	pathocl/kernels/pathocl_kernel_filters.cpp
	pathocl/kernels/pathocl_kernel_samplers.cpp
	pathocl/kernels/pathocl_kernel_scene.cpp
	pathocl/kernels/pathocl_kernels.cpp
	pathocl/pathocl.cpp
	renderconfig.cpp
	smallluxgpu.cpp
	telnet.cpp
	)

add_executable(slg2 ${SMALLLUXGPU_SRCS})
target_link_libraries(slg2 luxrays ${GLEW_LIBRARY} ${GLUT_LIBRARY} ${OPENGL_LIBRARY} ${OCL_LIBRARY} ${Boost_LIBRARIES} ${FREEIMAGE_LIBRARIES})
