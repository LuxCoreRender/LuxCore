################################################################################
# Copyright 1998-2015 by authors (see AUTHORS.txt)
#
#   This file is part of LuxRender.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

MESSAGE(STATUS "CMAKE VERSION DETECTED " ${CMAKE_VERSION})


################################################################################
#
# Check and configure cmake
#
################################################################################
# Fresh start
cmake_minimum_required(VERSION 2.8)
cmake_policy(VERSION 2.8)
#Remove the following when the version check is at least 2.8.4
SET(CMAKE_LEGACY_CYGWIN_WIN32 0)

project(LuxRays)

# Don't over-configure
SET(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limit configs" FORCE)


################################################################################
#
# Include necessary submodules
#
################################################################################

set(CMAKE_MODULE_PATH
	"${LuxRays_SOURCE_DIR}"
	"${LuxRays_SOURCE_DIR}/cmake"
	"${LuxRays_SOURCE_DIR}/cmake/Utils"
	"${LuxRays_SOURCE_DIR}/cmake/Packages"
	"${LuxRays_SOURCE_DIR}/cmake/SpecializedConfig"
)

# When using single configuration generators like make
# cmake does need to know which of the possible configurations
# to generate
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "assure config" FORCE) # makes sure type is shown in cmake gui

message(STATUS "Building mode: " ${CMAKE_BUILD_TYPE})

INCLUDE(PlatformSpecific)
INCLUDE(Configuration)
INCLUDE(KernelPreprocess)

# Install CMake modules
#add_subdirectory(CMake)

SET(LuxRays_INCLUDE_DIR "${LuxRays_SOURCE_DIR}/include")
include_directories("${LuxRays_INCLUDE_DIR}")

include_directories("${LuxRays_SOURCE_DIR}/deps/eos_portable_archive-v5.1")

# Find dependencies
include(Dependencies)

if (NOT Boost_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required Boost files - Please check ${BOOST_SEARCH_PATH}")
endif()

if (NOT OPENIMAGEIO_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required OpenImageIO files - Please check ${OPENIMAGEIO_SEARCH_PATH}")
endif()

if (NOT OPENCL_FOUND AND NOT LUXRAYS_DISABLE_OPENCL)
	MESSAGE(ERROR "--> Could not locate required OpenCL files, disabling OpenCL support - Please check ${OPENCL_SEARCH_PATH}")
	SET(LUXRAYS_DISABLE_OPENCL 1)
endif()

if (NOT OPENGL_FOUND AND NOT LUXRAYS_DISABLE_OPENCL)
	MESSAGE(ERROR "--> Could not locate required OpenGL files, disabling OpenCL support, disabling samples build")
	SET(LUXRAYS_DISABLE_OPENCL 1)
endif()

if (NOT GLEW_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required GLEW files, disabling samples build - Please check ${GLEW_SEARCH_PATH}")
endif()

if (NOT GLUT_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required GLUT files, disabling samples build - Please check ${GLUT_SEARCH_PATH}")
endif()

if (NOT EMBREE_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required Intel Embree files - Please check ${EMBREE_SEARCH_PATH}")
endif()

################################################################################
#
# Build options
#
################################################################################

if (LUXRAYS_DISABLE_OPENCL)
	ADD_DEFINITIONS("-DLUXRAYS_DISABLE_OPENCL")
endif()

if (LUXCORE_DISABLE_EMBREE_BVH_BUILDER)
	ADD_DEFINITIONS("-DLUXCORE_DISABLE_EMBREE_BVH_BUILDER")
endif()

################################################################################
#
# LuxRays, SLG and LuxCore libraries
#
################################################################################

add_subdirectory(src/luxrays)
add_subdirectory(src/slg)
add_subdirectory(src/luxcore)


################################################################################
#
# Samples
#
################################################################################

#if(NOT APPLE OR OSX_BUILD_DEMOS)
#	add_subdirectory(samples/benchsimple)
#	add_subdirectory(samples/luxcoredemo)
#	add_subdirectory(samples/luxcorescenedemo)
#	add_subdirectory(samples/luxcoreimplserializationdemo)
#endif()

if(OPENGL_FOUND AND GLUT_FOUND AND GLEW_FOUND)
	add_subdirectory(samples/smallluxgpu4)
endif(OPENGL_FOUND AND GLUT_FOUND AND GLEW_FOUND)

add_subdirectory(samples/luxcoreconsole)

if(OPENGL_FOUND)
	add_subdirectory(samples/luxcoreui)
	# APPLE bundle is not prepared yet, windows buildbot exhibits problems, so disable for now
	if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
		add_subdirectory(ui/luxrender2)
	endif()
endif(OPENGL_FOUND)


################################################################################
#
# For non win32 we'll have to copy everything to a single dir
#
################################################################################

INCLUDE(AssembleBinDirs)
