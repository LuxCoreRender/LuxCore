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

include(FindPkgMacros)
getenv_path(LuxRays_DEPENDENCIES_DIR)

################################################################################
#
# Core dependencies
#
################################################################################

# Find threading library
find_package(Threads REQUIRED)

find_package(OpenImageIO REQUIRED)
include_directories(BEFORE SYSTEM ${OPENIMAGEIO_INCLUDE_DIR})
find_package(OpenEXR REQUIRED)

if(NOT APPLE)
    # Apple has these available hardcoded and matched in macos repo, see Config_OSX.cmake

    include_directories(BEFORE SYSTEM ${OPENEXR_INCLUDE_DIRS})
    find_package(TIFF REQUIRED)
    include_directories(BEFORE SYSTEM ${TIFF_INCLUDE_DIR})
    find_package(JPEG REQUIRED)
    include_directories(BEFORE SYSTEM ${JPEG_INCLUDE_DIR})
    find_package(PNG REQUIRED)
    include_directories(BEFORE SYSTEM ${PNG_PNG_INCLUDE_DIR})
	# Find Python Libraries
    if("${PYTHON_V}" EQUAL "27")
        find_package(PythonLibs 2.7)
    else()
        find_package(PythonLibs 3.4)
    endif()
endif()

find_program(PYSIDE_UIC NAMES pyside-uic pyside2-uic
		HINTS "${PYTHON_INCLUDE_DIRS}/../Scripts"
		PATHS "c:/Program Files/Python${PYTHON_V}/Scripts")

include_directories(${PYTHON_INCLUDE_DIRS})

# Find Boost
set(Boost_USE_STATIC_LIBS       ON)
set(Boost_USE_MULTITHREADED     ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
set(BOOST_ROOT                  "${BOOST_SEARCH_PATH}")
#set(Boost_DEBUG                 ON)
set(Boost_MINIMUM_VERSION       "1.56.0")

# For Windows builds, PYTHON_V must be defined as "3x" (x=Python minor version, e.g. "35")
# For other platforms, specifying python minor version is not needed
set(LUXRAYS_BOOST_COMPONENTS thread program_options filesystem serialization iostreams regex system python${PYTHON_V} chrono serialization numpy${PYTHON_V})
find_package(Boost ${Boost_MINIMUM_VERSION} COMPONENTS ${LUXRAYS_BOOST_COMPONENTS})
if (NOT Boost_FOUND)
        # Try again with the other type of libs
        if(Boost_USE_STATIC_LIBS)
                set(Boost_USE_STATIC_LIBS OFF)
        else()
                set(Boost_USE_STATIC_LIBS ON)
        endif()
        # The following line is necessary with CMake 3.18.0 to find static libs on Windows
        unset(Boost_LIB_PREFIX)
		message(STATUS "Re-trying with link static = ${Boost_USE_STATIC_LIBS}")
        find_package(Boost ${Boost_MINIMUM_VERSION} COMPONENTS ${LUXRAYS_BOOST_COMPONENTS})
endif()

if (Boost_FOUND)
	include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
	link_directories(${Boost_LIBRARY_DIRS})
	# Don't use old boost versions interfaces
	ADD_DEFINITIONS(-DBOOST_FILESYSTEM_NO_DEPRECATED)
	if (Boost_USE_STATIC_LIBS)
		ADD_DEFINITIONS(-DBOOST_STATIC_LIB)
		ADD_DEFINITIONS(-DBOOST_PYTHON_STATIC_LIB)
	endif()
endif ()


# OpenGL
find_package(OpenGL)

if (OPENGL_FOUND)
	include_directories(BEFORE SYSTEM ${OPENGL_INCLUDE_PATH})
endif ()

# Intel Embree
set(EMBREE_ROOT                "${EMBREE_SEARCH_PATH}")
find_package(Embree REQUIRED)

if (EMBREE_FOUND)
	include_directories(BEFORE SYSTEM ${EMBREE_INCLUDE_PATH})
endif ()

# Intel Oidn
set(OIDN_ROOT                "${OIDN_SEARCH_PATH}")
find_package(Oidn REQUIRED)

if (OIDN_FOUND)
	include_directories(BEFORE SYSTEM ${OIDN_INCLUDE_PATH})
endif ()

# Intel TBB
set(TBB_ROOT                   "${TBB_SEARCH_PATH}")
find_package(TBB REQUIRED)

if (TBB_FOUND)
	include_directories(BEFORE SYSTEM ${TBB_INCLUDE_PATH})
endif ()

# Blosc
set(BLOSC_ROOT                   "${BLOSC_SEARCH_PATH}")
find_package(Blosc REQUIRED)

if (BLOSC_FOUND)
	include_directories(BEFORE SYSTEM ${BLOSC_INCLUDE_PATH})
endif ()

# OpenMP
if(NOT APPLE)
	find_package(OpenMP)
	if (OPENMP_FOUND)
		MESSAGE(STATUS "OpenMP found - compiling with")
   		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
   		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	else()
		MESSAGE(WARNING "OpenMP not found - compiling without")
	endif()
endif()

# Find GTK 3.0 for Linux only (required by luxcoreui NFD)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	include_directories(${GTK3_INCLUDE_DIRS})
endif()

# Find BISON
IF (NOT BISON_NOT_AVAILABLE)
	find_package(BISON)
ENDIF (NOT BISON_NOT_AVAILABLE)

# Find FLEX
IF (NOT FLEX_NOT_AVAILABLE)
	find_package(FLEX)
ENDIF (NOT FLEX_NOT_AVAILABLE)
