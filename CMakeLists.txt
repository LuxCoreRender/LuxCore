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

cmake_minimum_required(VERSION 3.25)
cmake_policy(VERSION 3.25)
include(CMakeDependentOption)

project(LuxRays)

# Options
OPTION(LUXCORE_LIBONLY "Enable to build LuxCore library only" OFF)
cmake_dependent_option(LUXCORE_UI "Enable to build luxcoreui" OFF "NOT LUXCORE_LIBONLY" OFF)
cmake_dependent_option(LUXCORE_CON "Enable to build luxcoreconsole" OFF "NOT LUXCORE_LIBONLY" OFF)
OPTION(LUXCORE_PYLUXCORE "Enable to build LuxCore Python bindings" OFF)
OPTION(LUXCORE_DEMOS "Enable to build LuxCore demos" OFF)
OPTION(LUXCORE_TESTS "Enable to build tests" OFF)
OPTION(LUXCORE_STATIC "Enable to build LuxCore as static library" ON)
OPTION(LUXRAYS_ENABLE_OPENCL "Enable to use OpenCL" ON)
OPTION(LUXRAYS_ENABLE_CUDA "Enable to use CUDA" ON)
OPTION(LUXRAYS_ENABLE_OPTIX "Enable to use Optix" ON)

if(LUXCORE_LIBONLY OR LUXCORE_UI OR LUXCORE_CON)
    set(LUXCORE_LIB ON)
else()
    set(LUXCORE_LIB OFF)
endif()

# Version
FILE(READ "${CMAKE_CURRENT_LIST_DIR}/luxcore.json" luxcore_json)
STRING(JSON LUXCORE_VERSION_MAJOR GET "${luxcore_json}" Version major)
STRING(JSON LUXCORE_VERSION_MINOR GET "${luxcore_json}" Version minor)
STRING(JSON LUXCORE_VERSION_BUGFIX GET "${luxcore_json}" Version bugfix)
MESSAGE(STATUS "Building LuxCore ${LUXCORE_VERSION_MAJOR}.${LUXCORE_VERSION_MINOR}.${LUXCORE_VERSION_BUGFIX}")
# We set LuxRays version same as LuxCore (don't know if it is judicious...)
SET(LUXRAYS_VERSION_MAJOR ${LUXCORE_VERSION_MAJOR})
SET(LUXRAYS_VERSION_MINOR ${LUXCORE_VERSION_MINOR})
SET(LUXRAYS_VERSION_BUGFIX ${LUXCORE_VERSION_BUGFIX})

# Fundamental settings
# This boots up the generator:
enable_language(C)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)  # Propagate Conan targets to subdirs
set(CMAKE_VERBOSE_MAKEFILE ON)

# RPATH
# From https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling
# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)
MESSAGE(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif("${isSystemDir}" STREQUAL "-1")
MESSAGE(STATUS "Install RPATH: ${CMAKE_INSTALL_RPATH}")


if(LUXCORE_LIB AND NOT LUXCORE_STATIC)
  set(BUILD_LUXCORE_DLL ON)
else()
  set(BUILD_LUXCORE_DLL OFF)
endif()
MESSAGE(STATUS "Set BUILD_LUXCORE_DLL to ${BUILD_LUXCORE_DLL}")


# Build type
# Detect whether we're using a single config or multi config generator and
# configure it:

set(supported_build_variants "Debug" "Release")
unset(CMAKE_CONFIGURATION_TYPES CACHE)

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
    CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${supported_build_variants}
  )

  message(STATUS "Using build variant: " ${CMAKE_BUILD_TYPE})

endif()



# The following implicitly boots up the generator, so don't move it up:


# Set path to modules (keep it after project declaration)
set(CMAKE_MODULE_PATH
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/cmake"
)

################################################################################
#
# Include necessary dependencies
#
################################################################################

# Nota: please keep all 'find_package' gathered here (for maintenance reason)
message(STATUS "LuxCore - Add dependency targets")
find_package(OpenMP REQUIRED)
find_package(TBB REQUIRED)
find_package(minizip REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(PNG REQUIRED)
find_package(OpenColorIO REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(embree REQUIRED)
find_package(c-blosc REQUIRED)
find_package(oidn REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenVDB REQUIRED)
find_package(Imath REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenSubdiv REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(robin_hood REQUIRED)

# Bison/Flex
if(NOT WIN32)
    find_package(BISON REQUIRED)
    find_package(FLEX REQUIRED)
endif()

# Additional dependencies for pyluxcore
if(LUXCORE_PYLUXCORE)
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(blender-types REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)
endif()

# Additional dependencies for luxcoreui
if(LUXCORE_UI)
    find_package(imgui)
    find_package(glfw3)
    find_package(nativefiledialog)
endif()


# Include embedded dependencies

SET(LuxRays_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
include_directories("${LuxRays_INCLUDE_DIR}")

include_directories("${PROJECT_SOURCE_DIR}/deps/cuew/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/clew/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/optix-7.1.0/include")

# OpenCL preprocessing
INCLUDE(KernelPreprocess)

################################################################################
#
# Build options
#
################################################################################

if(LINUX)
  set(OpenGL_GL_PREFERENCE "LEGACY")
endif(LINUX)

if(MSVC)
  # Minimizes Windows header files
  #ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)
  # Do not define MIN and MAX macros
  ADD_DEFINITIONS(-DNOMINMAX)
  # Do not warn about standard but insecure functions
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
  # Enable Unicode
  ADD_DEFINITIONS(-D_UNICODE)
  # Enable SSE2/SSE/MMX
  ADD_DEFINITIONS(-D__SSE2__ -D__SSE__ -D__MMX__)
  # Add compile and link options
  add_compile_options("/arch:AVX" "/DWIN32" "/D_WINDOWS" "/EHsc")
  add_link_options("/INCREMENTAL:NO" "/PDBALTPATH:%_PDB%")
endif(MSVC)

if(APPLE)
  # OpenMP
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp -Wno-register")
endif(APPLE)

if (LUXRAYS_DISABLE_OPENCL)
  ADD_DEFINITIONS("-DLUXRAYS_DISABLE_OPENCL")
  # CUDA requires OpenCL support
  ADD_DEFINITIONS("-DLUXRAYS_DISABLE_CUDA")

  message(STATUS "OpenCL and CUDA support: disabled")
else()
  if (LUXRAYS_DISABLE_CUDA)
    ADD_DEFINITIONS("-DLUXRAYS_DISABLE_CUDA")
    message(STATUS "OpenCL support: enabled")
    message(STATUS "CUDA support: disabled")
  else()
    message(STATUS "OpenCL support: enabled")
    message(STATUS "CUDA support: enabled")
  endif()
endif()

if (LUXCORE_DISABLE_OIDN)
  ADD_DEFINITIONS("-DLUXCORE_DISABLE_OIDN")

  message(STATUS "Intel OIDN support: disabled")
else()
  message(STATUS "Intel OIDN support: enabled")
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
if(LUXCORE_PYLUXCORE)
    add_subdirectory(src/pyluxcoretools)
endif()

################################################################################
#
# Samples
#
################################################################################

if(LUXCORE_DEMOS)
  if((${CMAKE_SYSTEM_NAME} MATCHES "Linux") OR OSX_BUILD_DEMOS OR WIN_BUILD_DEMOS)
    add_subdirectory(samples/luxcoredemo)
    add_subdirectory(samples/luxcorescenedemo)

    if (NOT WIN32 OR NOT BUILD_LUXCORE_DLL)
      # Internal tests can not be compiled on WIN32 with DLL enabled
      add_subdirectory(tests/luxcoreimplserializationdemo)
    endif()
  endif()
endif()

if(LUXCORE_CON)
    add_subdirectory(samples/luxcoreconsole)
endif()

# TODO
find_package(OpenGL)
IF (NOT OPENGL_FOUND)
  MESSAGE(FATAL_ERROR "OpenGL not found! Please either install OpenGL or disable LUX_UI.")
ENDIF()

if(LUXCORE_UI)
    add_subdirectory(samples/luxcoreui)
endif()

################################################################################
#
# Tests
#
################################################################################

if(LUXCORE_TESTS)
  add_subdirectory(pyunittests)
endif()
