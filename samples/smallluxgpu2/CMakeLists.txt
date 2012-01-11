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

set(SLG_VERSION_MAJOR 2)
set(SLG_VERSION_MINOR 0devel4)

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

        add_custom_target(apidoc_slg2 DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

###########################################################################
#
# Generate kernel files
#
###########################################################################

#
# PathOCL kernel
#
SET(KERNELS kernel_core kernel_datatypes kernel_filters kernel_samplers kernel_scene kernels)
FOREACH(kernel ${KERNELS})
	STRING(TOLOWER ${kernel} kernel_lowercase)

	IF(WIN32)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo "#include \"pathocl/kernels/kernels.h\"" > ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo std::string luxrays::KernelSource_PathOCL_${kernel} = >> ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND for /F \"tokens=*\" %%a in (${CMAKE_CURRENT_SOURCE_DIR}\\pathocl\\kernels\\pathocl_${kernel_lowercase}.cl) do echo \"%%a\\n\" >> ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo ";" >> ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cl
		)
	ELSE(WIN32)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo "std::string luxrays::KernelSource_PathOCL_${kernel} = " >> ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND cat pathocl/kernels/pathocl_${kernel_lowercase}.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo "\;" >> ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/pathocl/kernels/pathocl_${kernel_lowercase}.cl
		)
	ENDIF(WIN32)
ENDFOREACH(kernel)

#############################################################################
#
# SLG binary
#
#############################################################################

include_directories(${LuxRays_INCLUDE_DIR})
include_directories(".")
link_directories (${LuxRays_LIB_DIR})

set(SMALLLUXGPU_SRCS
	smallluxgpu.cpp
	rendersession.cpp
	renderconfig.cpp
	renderengine.cpp
	displayfunc.cpp
	telnet.cpp
	pathocl/kernels/pathocl_kernel_core.cpp
	pathocl/kernels/pathocl_kernel_datatypes.cpp
	pathocl/kernels/pathocl_kernel_filters.cpp
	pathocl/kernels/pathocl_kernel_samplers.cpp
	pathocl/kernels/pathocl_kernel_scene.cpp
	pathocl/kernels/pathocl_kernels.cpp
	pathocl/compiledscene.cpp
	pathocl/pathocl.cpp
	pathocl/renderthread.cpp
	)

add_executable(slg2 ${SMALLLUXGPU_SRCS})

TARGET_LINK_LIBRARIES(slg2 luxrays ${GLEW_LIBRARY} ${OPENGL_LIBRARY})
TARGET_LINK_LIBRARIES(slg2 ${OpenCL_LIBRARY})
TARGET_LINK_LIBRARIES(slg2 ${GLUT_LIBRARY})

# This instructs FREEGLUT to emit a pragma for the static version
SET_TARGET_PROPERTIES(slg PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)
