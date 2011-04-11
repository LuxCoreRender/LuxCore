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

set(SLG_VERSION_MAJOR 1)
set(SLG_VERSION_MINOR 8beta1)

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

#
# PathGPU kernel
#
SET(KERNELS PathGPU)
FOREACH(kernel ${KERNELS})
	STRING(TOLOWER ${kernel} kernel_lowercase)

  IF(WIN32)
    add_custom_command(
      OUTPUT pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo "#include \"pathgpu/kernels/kernels.h\"" > pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo std::string luxrays::KernelSource_${kernel} = >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
#      COMMAND ../../BuildTools/ProcessClKernelFile.exe pathgpu\\kernels\\${kernel_lowercase}_kernel.cl >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND for /F \"tokens=*\" %%a in (pathgpu\\kernels\\${kernel_lowercase}_kernel.cl) do echo \"%%a\\n\" >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo ";" >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      MAIN_DEPENDENCY pathgpu/kernels/${kernel_lowercase}_kernel.cl
    )
	ELSE(WIN32)
    add_custom_command(
      OUTPUT pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo \"\#include \\"pathgpu/kernels/kernels.h\\"\" > pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo "std::string luxrays::KernelSource_${kernel} = " >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND cat pathgpu/kernels/${kernel_lowercase}_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo "\;" >> pathgpu/kernels/${kernel_lowercase}_kernel.cpp
      MAIN_DEPENDENCY pathgpu/kernels/${kernel_lowercase}_kernel.cl
    )
  ENDIF(WIN32)
ENDFOREACH(kernel)


#
# PathGPU2 kernel
#
SET(KERNELS kernel_core kernel_datatypes kernel_filters kernel_samplers kernel_scene kernels)
FOREACH(kernel ${KERNELS})
	STRING(TOLOWER ${kernel} kernel_lowercase)

  IF(WIN32)
    add_custom_command(
      OUTPUT pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND echo "#include \"pathgpu2/kernels/kernels.h\"" > pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND echo std::string luxrays::KernelSource_PathGPU2_${kernel} = >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
#      COMMAND ../../BuildTools/ProcessClKernelFile.exe pathgpu2\\kernels\\pathgpu2_${kernel_lowercase}.cl >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND for /F \"tokens=*\" %%a in (pathgpu2\\kernels\\pathgpu2_${kernel_lowercase}.cl) do echo \"%%a\\n\" >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND echo ";" >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cl
    )
	ELSE(WIN32)
    add_custom_command(
      OUTPUT pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND echo \"\#include \\"pathgpu2/kernels/kernels.h\\"\" > pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND echo "std::string luxrays::KernelSource_PathGPU2_${kernel} = " >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND cat pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      COMMAND echo "\;" >> pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cpp
      MAIN_DEPENDENCY pathgpu2/kernels/pathgpu2_${kernel_lowercase}.cl
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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../${LuxRays_BIN_DIR}")

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

TARGET_LINK_LIBRARIES(slg LuxRays ${GLEW_LIBRARY} ${OPENGL_LIBRARY} ${Boost_LIBRARIES} )
TARGET_LINK_LIBRARIES(slg ${OpenCL_LIBRARY})
TARGET_LINK_LIBRARIES(slg ${GLUT_LIBRARY})

# This instructs FREEGLUT to emit a pragma for the static version
SET_TARGET_PROPERTIES(slg PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)