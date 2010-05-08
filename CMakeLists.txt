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

# Luxrays build system

cmake_minimum_required(VERSION 2.6)
project(LuxRays)

SET(LUXRAYS_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LUXRAYS_CMAKE_DIR}")

# The supported kind of builds: Release or Debug
#SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF(NOT CMAKE_BUILD_TYPE)

message(STATUS "Building mode: " ${CMAKE_BUILD_TYPE})

# The version number.
set(LUXRAYS_VERSION_MAJOR 0)
set(LUXRAYS_VERSION_MINOR 1alpha7dev)

set(LUXRAYS_SOURCE_DIR src)
set(LUXRAYS_INCLUDE_DIR include)
set(LUXRAYS_LIB_DIR lib)
set(LUXRAYS_BIN_DIR bin)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${LUXRAYS_LIB_DIR}")
message(STATUS "Library directory: " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${LUXRAYS_BIN_DIR}")
message(STATUS "Binaries directory: " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${LUXRAYS_SOURCE_DIR}/cfg.h.in"
  "${LUXRAYS_INCLUDE_DIR}/luxrays/cfg.h"
  )

include_directories("${LUXRAYS_INCLUDE_DIR}" "${LUXRAYS_SOURCE_DIR}")

#############################################################################
#
# OpenCL Libraries
#
#############################################################################

if(LUXRAYS_DISABLE_OPENCL)
    set(OCL_LIBRARY "")
else(LUXRAYS_DISABLE_OPENCL)
    find_path(OCL_INCLUDE_PATH NAMES CL/cl.hpp OpenCL/cl.hpp PATHS /home/david/src/ati-stream-sdk-v2.1-lnx64/include)
    find_library(OCL_LIBRARY OpenCL /home/david/src/ati-stream-sdk-v2.1-lnx64/lib/x86_64)

    message(STATUS "OpenCL include directory: " ${OCL_INCLUDE_PATH})
    message(STATUS "OpenCL library directory: " ${OCL_LIBRARY})

    include_directories("${OCL_INCLUDE_PATH}")
endif(LUXRAYS_DISABLE_OPENCL)

#############################################################################
#
# OpenGL & GLUT Libraries
#
#############################################################################

find_package(GLUT)
find_package(OpenGL)

message(STATUS "OpenGL include directory: " "${OPENGL_INCLUDE_DIR}")
message(STATUS "OpenGL library: " "${OPENGL_LIBRARY}")
message(STATUS "GLUT include directory: " "${GLUT_INCLUDE_DIR}")
message(STATUS "GLUT library: " "${GLUT_LIBRARY}")

#############################################################################
#
# Boost Libraries
#
#############################################################################

find_package(Boost 1.39.0 REQUIRED COMPONENTS thread filesystem system)

if(Boost_FOUND)
	message(STATUS "Boost include directory: " "${Boost_INCLUDE_DIRS}")
	message(STATUS "Boost library directory: " "${Boost_LIBRARY_DIRS}")
else(Boost_FOUND)
	message(FATAL_ERROR "Could not find Boost Libraries")
endif(Boost_FOUND)

#############################################################################
#
# FreeImage Libraries
#
#############################################################################

find_package(FreeImage REQUIRED)

if(FREEIMAGE_FOUND)
    message(STATUS "FreeImage library directory: " ${FREEIMAGE_LIBRARIES})
    message(STATUS "FreeImage include directory: " ${FREEIMAGE_INCLUDE_PATH})
else(FREEIMAGE_FOUND)
    message(FATAL_ERROR "Could not find FreeImage")
endif(FREEIMAGE_FOUND)

include_directories("${FREEIMAGE_INCLUDE_PATH}")

#############################################################################
#
# LuxRays Docs
#
#############################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
        message(STATUS "Found Doxygen and generating LuxRays documentation")

        # Generate doxygen.template
        set(DOXYGEN_LUXRAYS_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/luxrays.template)
        configure_file(
          "${DOXYGEN_LUXRAYS_TEMPLATE}.in"
          "${DOXYGEN_LUXRAYS_TEMPLATE}"
          )

        set(DOXYGEN_LUXRAYS_INPUT ${CMAKE_CURRENT_BINARY_DIR}/doxygen-luxrays.conf)
        set(DOXYGEN_LUXRAYS_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doc/luxrays)
        set(DOXYGEN_LUXRAYS_OUTPUT ${DOXYGEN_OUTPUT_DIR}/html/index.html)

        message(STATUS "Doxygen LuxRays output:" ${DOXYGEN_LUXRAYS_OUTPUT})

        if(DOXYGEN_DOT_FOUND)
                message(STATUS "Found dot")
                set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
        endif(DOXYGEN_DOT_FOUND)

        add_custom_command(
            OUTPUT ${DOXYGEN_LUXRAYS_OUTPUT}
            # Creating custom doxygen-luxrays.conf
            COMMAND cp ${DOXYGEN_LUXRAYS_TEMPLATE} ${DOXYGEN_LUXRAYS_INPUT}
            COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/include  ${CMAKE_CURRENT_SOURCE_DIR}/src >> ${DOXYGEN_LUXRAYS_INPUT}
            COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_LUXRAYS_OUTPUT_DIR} >> ${DOXYGEN_LUXRAYS_INPUT}
            COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_LUXRAYS_INPUT}
            # Launch doxygen
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_LUXRAYS_INPUT}
            DEPENDS ${DOXYGEN_LUXRAYS_TEMPLATE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        add_custom_target(apidoc_luxrays DEPENDS ${DOXYGEN_LUXRAYS_OUTPUT})
ENDIF (DOXYGEN_FOUND)

#############################################################################
#
# Compiler flags
#
#############################################################################

set(CMAKE_CXX_FLAGS_DEBUG "-Wall -msse -msse2 -msse3 -mssse3 -fPIC -O0 -g")
# NOTE: QBVH can not be compiled with -O3 because of a GCC bug
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
# The QBVH accelerator needs to be compiled with much reduced optimizations
# otherwise gcc produces incorrect code and ruins the render on 64bits machines
SET_SOURCE_FILES_PROPERTIES(${LUXRAYS_SOURCE_DIR}/accelerators/qbvhaccel.cpp COMPILE_FLAGS "-O2")

#OSX-flags by jensverwiebe
#set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller -cl-fast-relaxed-math -cl-mad-enable")

###########################################################################
#
# Generate LuxRays kernel files
#
###########################################################################

add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_BVH = " >> ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_QBVH = " >> ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_Pixel_ClearFB = " >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_Pixel_ClearSampleFB = " >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_Pixel_UpdateFrameBuffer = " >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_Pixel_AddSampleBuffer = " >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_Pixel_AddSampleBufferPreview = " >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cl
   )
add_custom_command(
   OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
   COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_Pixel_AddSampleBufferGaussian2x2 = " >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
   COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >>  ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
   COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
   MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cl
   )

###########################################################################
#
# LuxRays Library
#
###########################################################################

set(LUXRAYS_SRCS
    ${LUXRAYS_SOURCE_DIR}/accelerators/bvhaccel.cpp
    ${LUXRAYS_SOURCE_DIR}/accelerators/qbvhaccel.cpp
    ${LUXRAYS_SOURCE_DIR}/core/context.cpp
    ${LUXRAYS_SOURCE_DIR}/core/dataset.cpp
    ${LUXRAYS_SOURCE_DIR}/core/device.cpp
    ${LUXRAYS_SOURCE_DIR}/core/geometry/bbox.cpp
    ${LUXRAYS_SOURCE_DIR}/core/geometry/matrix4x4.cpp
	${LUXRAYS_SOURCE_DIR}/core/geometry/ray.cpp
    ${LUXRAYS_SOURCE_DIR}/core/geometry/transform.cpp
    ${LUXRAYS_SOURCE_DIR}/core/trianglemesh.cpp
    ${LUXRAYS_SOURCE_DIR}/idevices/openclidevice.cpp
    ${LUXRAYS_SOURCE_DIR}/idevices/nativeidevice.cpp
    ${LUXRAYS_SOURCE_DIR}/idevices/virtualidevice.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffer_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebufferpreview_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearfb_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/pixel_clearsamplefb_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/pixel_updateframebuffer_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/pdevices/nativepdevice.cpp
    ${LUXRAYS_SOURCE_DIR}/pdevices/openclpdevice.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/core/exttrianglemesh.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/plymesh/rply.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/properties.cpp)

add_library(luxrays STATIC ${LUXRAYS_SRCS})

###########################################################################
#
# Samples
#
###########################################################################

add_subdirectory(samples/benchsimple)
add_subdirectory(samples/smallluxgpu)
add_subdirectory(samples/benchpixel)

###########################################################################
#
# Binary samples directory
#
###########################################################################

set(CMAKE_INSTALL_PREFIX .)
#set(LUXRAYS_BIN_SAMPLES_DIR "luxrays-v${LUXRAYS_VERSION_MAJOR}.${LUXRAYS_VERSION_MINOR}")
set(LUXRAYS_BIN_SAMPLES_DIR "slg-v1.5beta3")

add_custom_command(
    OUTPUT "${LUXRAYS_BIN_SAMPLES_DIR}"
    COMMAND rm -rf ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND mkdir ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp -r scenes ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp samples/smallluxgpu/bat/*.bat ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp samples/smallluxgpu/exe/*.* ${LUXRAYS_BIN_SAMPLES_DIR}
	COMMAND mkdir ${LUXRAYS_BIN_SAMPLES_DIR}/blender
    COMMAND cp samples/smallluxgpu/blender/*.py ${LUXRAYS_BIN_SAMPLES_DIR}/blender
    COMMAND cp bin/slg ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp AUTHORS.txt COPYING.txt README.txt ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMENT "Building ${LUXRAYS_BIN_SAMPLES_DIR}")

add_custom_command(
    OUTPUT "${LUXRAYS_BIN_SAMPLES_DIR}.tgz"
    COMMAND tar zcvf ${LUXRAYS_BIN_SAMPLES_DIR}.tgz ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND rm -rf ${LUXRAYS_BIN_SAMPLES_DIR}
    DEPENDS ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMENT "Building ${LUXRAYS_BIN_SAMPLES_DIR}.tgz")

add_custom_target(samples_tgz DEPENDS "${LUXRAYS_BIN_SAMPLES_DIR}.tgz")
