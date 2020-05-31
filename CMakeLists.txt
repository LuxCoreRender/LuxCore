################################################################################
# Copyright 1998-2020 by authors (see AUTHORS.txt)
#
#   This file is part of LuxCoreRender.
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

MESSAGE(STATUS "CMake version " ${CMAKE_VERSION} " detected")


################################################################################
#
# Check and configure cmake
#
################################################################################

cmake_minimum_required(VERSION 3.5.1)
cmake_policy(VERSION 3.0)

# Remove the following when the version check is at least 2.8.4
SET(CMAKE_LEGACY_CYGWIN_WIN32 0)

set(supported_build_variants "Debug" "Release")

# Detect whether we're using a single config or multi config generator and
# configure it:

unset(CMAKE_CONFIGURATION_TYPES CACHE)

# This boots up the generator:
enable_language(C)
enable_language(CXX)

if (CMAKE_CONFIGURATION_TYPES)

	message(STATUS "Multi-config generator detected")

	# Hard-wire supported configurations:

	set(CMAKE_CONFIGURATION_TYPES
			${supported_build_variants} CACHE INTERNAL "-" FORCE)
else()
	message(STATUS "Single-config generator detected")

	# For generators like make, cmake needs to know which variant to use
	# so let's have a drop-down selector for it in the GUI:

	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build configuration")
	set_property(
			CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${supported_build_variants})

	message(STATUS "Using build variant: " ${CMAKE_BUILD_TYPE})

endif()

# The following implicitly boots up the generator, so don't move it up:

project(LuxRays)

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

INCLUDE(PlatformSpecific)
INCLUDE(Configuration)
INCLUDE(KernelPreprocess)

# Install CMake modules
#add_subdirectory(CMake)

SET(LuxRays_INCLUDE_DIR "${LuxRays_SOURCE_DIR}/include")
include_directories("${LuxRays_INCLUDE_DIR}")

include_directories("${LuxRays_SOURCE_DIR}/deps/json-3.7.3/include")
include_directories("${LuxRays_SOURCE_DIR}/deps/cuew/include")
include_directories("${LuxRays_SOURCE_DIR}/deps/clew/include")

# Find dependencies
include(Dependencies)

if (NOT Boost_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required Boost files - Please check ${BOOST_SEARCH_PATH}")
endif()

if (NOT OPENIMAGEIO_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required OpenImageIO files - Please check ${OPENIMAGEIO_SEARCH_PATH}")
endif()

if (NOT OPENGL_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required OpenGL file")
endif()

if (NOT EMBREE_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required Intel Embree files - Please check ${EMBREE_SEARCH_PATH}")
endif()

if (NOT OIDN_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required Intel Oidn files - Please check ${OIDN_SEARCH_PATH}")
endif()

if (NOT TBB_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required Intel TBB files - Please check ${TBB_SEARCH_PATH}")
endif()

if (NOT BLOSC_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required C-BLOSC files - Please check ${BLOSC_SEARCH_PATH}")
endif()

################################################################################
#
# Build options
#
################################################################################

if (LUXRAYS_DISABLE_OPENCL)
	ADD_DEFINITIONS("-DLUXRAYS_DISABLE_OPENCL")
endif()

if (LUXRAYS_DISABLE_CUDA)
	ADD_DEFINITIONS("-DLUXRAYS_DISABLE_CUDA")
endif()

if (BUILD_LUXCORE_DLL)
	set(LUXCORE_LIBRARY luxcore)
	ADD_DEFINITIONS("-DLUXCORE_DLL")
else()
	set(LUXCORE_LIBRARY luxcore slg-core slg-film slg-kernels luxrays bcd opensubdiv openvdb ${BLOSC_LIBRARY} ${EMBREE_LIBRARY} ${OIDN_LIBRARY} ${TBB_LIBRARY} ${TIFF_LIBRARIES} ${TIFF_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES})
endif()

################################################################################
#
# LuxRays, SLG, LuxCore and PyLuxCoreTools libraries
#
################################################################################

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(GENERATED_INCLUDE_DIR "${GENERATED_DIR}/include")

file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})
include_directories(${GENERATED_INCLUDE_DIR})


add_subdirectory(src/luxrays)
add_subdirectory(src/slg)
add_subdirectory(src/luxcore)
add_subdirectory(src/pyluxcoretools)

################################################################################
#
# Samples
#
################################################################################

if(NOT APPLE OR OSX_BUILD_DEMOS)
	add_subdirectory(samples/luxcoredemo)
	add_subdirectory(samples/luxcorescenedemo)

	if (NOT WIN32 OR NOT BUILD_LUXCORE_DLL)
		# Internal tests can not be compiled on WIN32 with DLL enabled
		add_subdirectory(tests/luxcoreimplserializationdemo)
	endif()
endif()

add_subdirectory(samples/luxcoreconsole)

if(OPENGL_FOUND)
	add_subdirectory(samples/luxcoreui)
endif(OPENGL_FOUND)


################################################################################
#
# Tests
#
################################################################################

add_subdirectory(pyunittests)

################################################################################
#
# For non win32 we'll have to copy everything to a single dir
#
################################################################################

INCLUDE(AssembleBinDirs)
