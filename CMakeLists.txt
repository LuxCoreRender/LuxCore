################################################################################
# Copyright 1998-2025 by Authors (see AUTHORS.txt)
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

cmake_minimum_required(VERSION 3.29)
cmake_policy(VERSION 3.25)


MESSAGE(STATUS "Building LuxCore ${LUXCORE_VERSION}")

project(LuxCore)

MESSAGE(STATUS "C++ compiler identifier is ${CMAKE_CXX_COMPILER_ID}")
MESSAGE(STATUS "C++ compiler version is ${CMAKE_CXX_COMPILER_VERSION}")
MESSAGE(STATUS "C compiler identifier is ${CMAKE_C_COMPILER_ID}")
MESSAGE(STATUS "C compiler version is ${CMAKE_C_COMPILER_VERSION}")

# Version computation
#
# Version number can be set by external caller (for instance, Github action)
# by setting LUXCORE_VERSION.
# Otherwise (like in a plain make build), we'll fallback to build-settings
# default version
#
# Versioning should comply to semver standard: https://semver.org
#
FILE(READ "${CMAKE_CURRENT_LIST_DIR}/build-system/build-settings.json" settings_json)
STRING(JSON _DEFAULT_VERSION_MAJOR GET "${settings_json}" DefaultVersion major)
STRING(JSON _DEFAULT_VERSION_MINOR GET "${settings_json}" DefaultVersion minor)
STRING(JSON _DEFAULT_VERSION_PATCH GET "${settings_json}" DefaultVersion patch)
SET(
  _DEFAULT_VERSION_BASE
  "${_DEFAULT_VERSION_MAJOR}.${_DEFAULT_VERSION_MINOR}.${_DEFAULT_VERSION_PATCH}"
)
STRING(JSON _DEFAULT_VERSION_PRERELEASE GET "${settings_json}" DefaultVersion prerelease)
if (NOT "${_DEFAULT_VERSION_PRERELEASE}" STREQUAL "")
  SET(_DEFAULT_LUXCORE_VERSION "${_DEFAULT_VERSION_BASE}-${_DEFAULT_VERSION_PRERELEASE}")
else()
  SET(_DEFAULT_LUXCORE_VERSION "${_DEFAULT_VERSION_BASE}")
endif()

SET(LUXCORE_VERSION "${_DEFAULT_LUXCORE_VERSION}" CACHE STRING "LuxCore version")

# Dependencies check
STRING(JSON REQUIRED_LUXCOREDEPS_VERSION GET "${settings_json}" Dependencies release)
MESSAGE(STATUS "LuxCore Dependencies - Required version: ${REQUIRED_LUXCOREDEPS_VERSION}")
IF(NOT ${REQUIRED_LUXCOREDEPS_VERSION} VERSION_EQUAL ${LUXCOREDEPS_VERSION})
  MESSAGE(WARNING "LuxCore Dependencies - Provided version: ${LUXCOREDEPS_VERSION}")
  MESSAGE(WARNING "\
LuxCore Dependencies - Provided version does not match required version!\n\
This may result in build errors or undefined behavior.\n\
You should try to update dependencies ('make deps').\
  ")
ELSE()
  MESSAGE(STATUS "LuxCore Dependencies - Provided version: ${LUXCOREDEPS_VERSION} - OK")
ENDIF()

# Options
OPTION(LUXCORE_DEMOS "Enable to build LuxCore demos" OFF)
OPTION(LUXCORE_TESTS "Enable to build tests" OFF)
OPTION(LUXRAYS_ENABLE_OPENCL "Enable to use OpenCL" ON)
OPTION(LUXRAYS_ENABLE_CUDA "Enable to use CUDA" ON)
OPTION(LUXRAYS_ENABLE_OPTIX "Enable to use Optix" ON)

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
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
MESSAGE(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

## the RPATH to be used when installing, but only if it's not a system directory
#list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
#if("${isSystemDir}" STREQUAL "-1")
    #set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
#endif("${isSystemDir}" STREQUAL "-1")
MESSAGE(STATUS "Install RPATH: ${CMAKE_INSTALL_RPATH}")
if(LINUX)
  add_link_options("LINKER:--disable-new-dtags")
endif()

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

include(GNUInstallDirs)

# Set path to modules (keep it after project declaration)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/build-system/cmake")

################################################################################
#
# Include necessary dependencies
#
################################################################################

# Ensure compiler version meets minimum requirements
INCLUDE(CheckCompilerVersion)

# Nota: please keep all 'find_package' gathered here (for maintenance reasons)
message(STATUS "LuxCore - Add dependency targets")
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
find_package(OpenMP REQUIRED)
if (WIN32 OR LINUX)
  find_package(nvrtc REQUIRED)
endif()

# Bison/Flex
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# Additional dependencies for pyluxcore
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Additional dependencies for luxcoreui
find_package(imgui)
find_package(glfw3)
find_package(imguifiledialog)

# Documentation
find_package(Doxygen)


################################################################################
#
# Complementary includes
#
################################################################################


SET(LuxRays_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
include_directories("${LuxRays_INCLUDE_DIR}")

# Embedded dependencies
include_directories("${PROJECT_SOURCE_DIR}/deps/cuew/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/clew/include")
include_directories("${PROJECT_SOURCE_DIR}/deps/optix-7.1.0/include")

# OpenCL preprocessing
INCLUDE(KernelPreprocess)

################################################################################
#
# Per-platform options
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

  # Debug information format (embedded)
  set(
    CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
  )
endif(MSVC)

if(APPLE)
  # OpenMP
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp=libomp -Wno-register")
  # Debug
  add_compile_options("$<$<CONFIG:Debug,RelWithDebInfo>:-gfull>")
  add_link_options("$<$<CONFIG:Debug,RelWithDebInfo>:-gfull>")

endif(APPLE)

set(INSTALL_EXCLUDE_REGEXES
  [[.*nvrtc.*\.alt\..*]]
  "libgcc_s.so.1"
  "libstdc\\+\\+.so.6"
  "libm.so.6"
  "libdl.so.2"
  "librt.so.1"
  "libc.so.6"
  "libnsl.so.1"
  "libutil.so.1"
  "libpthread.so.0"
  "libX11.so.6"
  "libXext.so.6"
  "libXrender.so.1"
  "libICE.so.6"
  "libSM.so.6"
  "libGL.so.1"
  "libgobject-2.0.so.0"
  "libgthread-2.0.so.0"
  "libglib-2.0.so.0"
  "libresolv.so.2"
  "libexpat.so.1"
  "libz.so.1"
  ".*system32/.*\\.dll"
)

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
add_subdirectory(src/pyluxcore)
add_subdirectory(src/pyluxcoretools)

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

add_subdirectory(samples/luxcoreconsole)

add_subdirectory(samples/luxcoreui)


################################################################################
#
# Tests
#
################################################################################

if(LUXCORE_TESTS)
  add_subdirectory(pyunittests)
endif()

################################################################################
#
# Pack
#
################################################################################

if(LINUX)
include(InstallRequiredSystemLibraries)
endif()

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.txt")
set(CPACK_PACKAGE_NAME "LuxCore")
set(CPACK_PACKAGE_VENDOR "LuxCoreRender")
set(CPACK_PACKAGE_VERSION "${LUXCORE_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_THREADS 0)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)  # Install: per-component
set(CPACK_COMPONENTS_GROUPING "ALL_COMPONENTS_IN_ONE") # Archive: all-in-one
if(LINUX)
    set(CPACK_COMPONENTS_ALL "luxcore;luxcoreui;luxcoreconsole;Unspecified")
else()
    set(CPACK_COMPONENTS_ALL "luxcore;luxcoreui;luxcoreconsole")
endif()

include(CPack)
