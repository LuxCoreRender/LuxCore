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

# The supported kind of builds: Release or Debug
#SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF(NOT CMAKE_BUILD_TYPE)

message(STATUS "Building mode: " ${CMAKE_BUILD_TYPE})

# The version number.
set(LUXRAYS_VERSION_MAJOR 0)
set(LUXRAYS_VERSION_MINOR 1alpha0dev)

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

find_path(OCL_INCLUDE_PATH NAMES CL/cl.hpp OpenCL/cl.hpp PATHS /home/david/src/ati-stream-sdk-v2.0-lnx64/include)
find_library(OCL_LIBRARY OpenCL /home/david/src/ati-stream-sdk-v2.01-lnx64/lib/x86_64)

message(STATUS "OpenCL include directory: " ${OCL_INCLUDE_PATH})
message(STATUS "OpenCL library directory: " ${OCL_LIBRARY})

include_directories("${OCL_INCLUDE_PATH}")

#############################################################################
#
# OpenGL & GLUT Libraries
#
#############################################################################

find_package(GLUT)
find_package(OpenGL)

message(STATUS "OpenGL include directory: " ${OPENGL_INCLUDE_DIR})
message(STATUS "GLUT include directory: " ${GLUT_INCLUDE_DIR})

#############################################################################
#
# Boost Libraries
#
#############################################################################

IF(APPLE) # typical boostlibs from luxrender
	FIND_PACKAGE(Boost 1.39.0 REQUIRED COMPONENTS thread-xgcc40-mt-1_39)
ELSE(APPLE)
	FIND_PACKAGE(Boost 1.39.0 REQUIRED COMPONENTS thread)
ENDIF(APPLE)

if(Boost_FOUND)
	message(STATUS "Boost include directory: " "${Boost_INCLUDE_DIRS}")
	message(STATUS "Boost library directory: " "${Boost_LIBRARY_DIRS}")
else(Boost_FOUND)
	message(FATAL_ERROR "Could not find Boost Libraries")
endif(Boost_FOUND)

#############################################################################
#
# PNG Libraries
#
#############################################################################

find_package(PNG)

if(PNG_FOUND)
    message(STATUS "PNG include directory: " "${PNG_INCLUDE_DIR}")
    message(STATUS "PNG libraries: " "${PNG_LIBRARIES}")
else(PNG_FOUND)
	message(FATAL_ERROR "Could not find PNG Library")
endif(PNG_FOUND)

#############################################################################
#
# Compiler flags
#
#############################################################################

set(CMAKE_CXX_FLAGS_DEBUG "-Wall -fPIC -O0 -g")
# NOTE: QBVH can not be compiled with -O3 because of a GCC bug
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O2 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")

#OSX-flags by jensverwiebe
#set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller -cl-fast-relaxed-math -cl-mad-enable")

###########################################################################
#
# LuxRays Library
#
###########################################################################

# Generate kernel file
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
    ${LUXRAYS_SOURCE_DIR}/devices/opencldevice.cpp
    ${LUXRAYS_SOURCE_DIR}/devices/nativedevice.cpp
    ${LUXRAYS_SOURCE_DIR}/devices/virtualdevice.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/bvh_kernel.cpp
    ${LUXRAYS_SOURCE_DIR}/kernels/qbvh_kernel.cpp
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

###########################################################################
#
# Binary samples directory
#
###########################################################################

set(CMAKE_INSTALL_PREFIX .)
set(LUXRAYS_BIN_SAMPLES_DIR "luxrays-v${LUXRAYS_VERSION_MAJOR}.${LUXRAYS_VERSION_MINOR}")

ADD_CUSTOM_COMMAND(
    OUTPUT "${LUXRAYS_BIN_SAMPLES_DIR}"
    COMMAND rm -rf ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND mkdir ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp -r scenes ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp samples/smallluxgpu/bat/*.bat ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp bin/slg ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND cp AUTHORS.txt COPYING.txt ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMENT "Building ${LUXRAYS_BIN_SAMPLES_DIR}")

ADD_CUSTOM_COMMAND(
    OUTPUT "${LUXRAYS_BIN_SAMPLES_DIR}.tgz"
    COMMAND tar zcvf ${LUXRAYS_BIN_SAMPLES_DIR}.tgz ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMAND rm -rf ${LUXRAYS_BIN_SAMPLES_DIR}
    DEPENDS ${LUXRAYS_BIN_SAMPLES_DIR}
    COMMENT "Building ${LUXRAYS_BIN_SAMPLES_DIR}.tgz")

ADD_CUSTOM_TARGET(samples_tgz DEPENDS "${LUXRAYS_BIN_SAMPLES_DIR}.tgz")
