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

MESSAGE(STATUS "CMAKE VERSION DETECTED " ${CMAKE_VERSION})
cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
cmake_policy(SET CMP0015 OLD)

project(LuxRays)

# Setting Universal Binary Properties, only for Mac OS X
#  generate with xcode/crosscompile, setting: ( darwin - 10.6 - gcc - g++ - MacOSX10.6.sdk - Find from root, then native system )
if(APPLE)
	IF(COMMAND cmake_policy)
		if(CMAKE_VERSION VERSION_LESS 2.8.1)
			cmake_policy(SET CMP0003 NEW)
		else(CMAKE_VERSION VERSION_LESS 2.8.1)
			cmake_policy(SET CMP0015 NEW)
		endif(CMAKE_VERSION VERSION_LESS 2.8.1)
	ENDIF(COMMAND cmake_policy)

	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.6)
	if(CMAKE_VERSION VERSION_LESS 2.8.1)
		SET(CMAKE_OSX_ARCHITECTURES i386;x86_64)
	else(CMAKE_VERSION VERSION_LESS 2.8.1)
		SET(CMAKE_XCODE_ATTRIBUTE_ARCHS i386\ x86_64)
	endif(CMAKE_VERSION VERSION_LESS 2.8.1)
	set(CMAKE_CONFIGURATION_TYPES Release)
	set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.6.sdk)
	INCLUDE_DIRECTORIES( ../macos/include )
	set(BUILD_LUXMARK on)
endif(APPLE)

set(LUXRAYS_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LUXRAYS_CMAKE_DIR}")

# The supported kind of builds: Release or Debug
#set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "Building mode: " ${CMAKE_BUILD_TYPE})

# The version number.
set(LUXRAYS_VERSION_MAJOR 0)
set(LUXRAYS_VERSION_MINOR 2dev)

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
  "${CMAKE_BINARY_DIR}/${LUXRAYS_INCLUDE_DIR}/luxrays/cfg.h"
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
    find_path(OCL_INCLUDE_PATH NAMES CL/cl.hpp OpenCL/cl.hpp PATHS /home/david/src/ati-stream-sdk-v2.3-lnx64/include /usr/local/cuda/include)
    find_library(OCL_LIBRARY OpenCL /home/david/src/ati-stream-sdk-v2.3-lnx64/lib/x86_64)

    message(STATUS "OpenCL include directory: " ${OCL_INCLUDE_PATH})
    message(STATUS "OpenCL library directory: " ${OCL_LIBRARY})

    include_directories("${OCL_INCLUDE_PATH}")
endif(LUXRAYS_DISABLE_OPENCL)

#############################################################################
#
# OpenGL, GLUT and GLEW  Libraries
#
#############################################################################

find_package(GLUT)
find_package(OpenGL)

if(APPLE)
	find_path(GLEW_INCLUDE_DIR
    	glew.h
    	PATHS
		../macos//include/GL
		NO_DEFAULT_PATH
    )
    find_library(GLEW_LIBRARY
    	libGLEW.a
    	PATHS
		../macos//lib
		NO_DEFAULT_PATH
    )
else(APPLE)
	find_package(GLEW)
endif(APPLE)

message(STATUS "OpenGL include directory: " "${OPENGL_INCLUDE_DIR}")
message(STATUS "OpenGL library: " "${OPENGL_LIBRARY}")
message(STATUS "GLUT include directory: " "${GLUT_INCLUDE_DIR}")
message(STATUS "GLUT library: " "${GLUT_LIBRARY}")
message(STATUS "GLEW include directory: " "${GLEW_INCLUDE_DIR}")
message(STATUS "GLEW library: " "${GLEW_LIBRARY}")

#############################################################################
#
# Boost Libraries
#
#############################################################################

if(APPLE)
	set(BOOST_ROOT ../macos)
endif(APPLE)

find_package(Boost 1.43.0 REQUIRED COMPONENTS thread filesystem system)

if(Boost_FOUND)
	message(STATUS "Boost include directory: " "${Boost_INCLUDE_DIRS}")
	message(STATUS "Boost library directory: " "${Boost_LIBRARY_DIRS}")
else(Boost_FOUND)
	message(FATAL_ERROR "Could not find Boost Libraries")
endif(Boost_FOUND)

INCLUDE_DIRECTORIES(SYSTEM ${Boost_INCLUDE_DIRS}) 

#############################################################################
#
# FreeImage Libraries
#
#############################################################################

if(APPLE)
    find_path(FREEIMAGE_INCLUDE_DIRS
    	freeimage.h
    	PATHS
		../macos//include
		NO_DEFAULT_PATH
	)
    find_library(FREEIMAGE_LIBRARIES
    	libfreeimage.a
    	PATHS
		../macos//lib
		NO_DEFAULT_PATH
	)
else(APPLE)
	find_package(FreeImage REQUIRED)

	if(FREEIMAGE_FOUND)
		message(STATUS "FreeImage library directory: " ${FREEIMAGE_LIBRARIES})
		message(STATUS "FreeImage include directory: " ${FREEIMAGE_INCLUDE_PATH})
	else(FREEIMAGE_FOUND)
		 message(FATAL_ERROR "Could not find FreeImage")
	endif(FREEIMAGE_FOUND)

	include_directories("${FREEIMAGE_INCLUDE_PATH}")
endif(APPLE)

#############################################################################
#
# Qt Libraries
#
#############################################################################

find_package(Qt4)
if(QT4_FOUND)
	message(STATUS "Qt library directory: " ${QT_LIBRARY_DIR})
	message(STATUS "Qt include directory: " ${QT_INCLUDE_DIR})
else(QT4_FOUND)
	message(STATUS "Warning : could not find Qt - not building LuxMark")
endif(QT4_FOUND)


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
			COMMAND mkdir -p ${DOXYGEN_LUXRAYS_OUTPUT_DIR}
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
endif(DOXYGEN_FOUND)

#############################################################################
#
# Compiler flags
#
#############################################################################

if(NOT APPLE)
	set(CMAKE_CXX_FLAGS_DEBUG "-Wall -msse -msse2 -msse3 -mssse3 -fPIC -O0 -g")
	# NOTE: QBVH can not be compiled with -O3 because of a GCC bug
	set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller")
	#The QBVH accelerator needs to be compiled with much reduced optimizations
	# otherwise gcc produces incorrect code and ruins the render on 64bits machines

	set_SOURCE_FILES_PROPERTIES(${LUXRAYS_SOURCE_DIR}/accelerators/qbvhaccel.cpp COMPILE_FLAGS "-O2")
	set_SOURCE_FILES_PROPERTIES(${LUXRAYS_SOURCE_DIR}/accelerators/mqbvhaccel.cpp COMPILE_FLAGS "-O2")
else(NOT APPLE)
	#OSX-flags by jensverwiebe
	set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wall -fPIC -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller -cl-fast-relaxed-math -cl-mad-enable")
endif(NOT APPLE)

###########################################################################
#
# Generate LuxRays kernel files
#
###########################################################################

SET(KERNELS BVH QBVH MQBVH Pixel_ClearFB Pixel_ClearSampleFB Pixel_UpdateFrameBuffer Pixel_AddSampleBuffer Pixel_AddSampleBufferPreview Pixel_AddSampleBufferGaussian2x2)

FOREACH(kernel $KERNELS)
	STRING(TOLOWER kernel kernel_lowercase)
	add_custom_command(
		OUTPUT ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cpp
		COMMAND echo "#include \"luxrays/kernels/kernels.h\"" > ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cpp
		COMMAND echo "std::string luxrays::KernelSource_"${kernel}" = " >> ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cpp
		IF(WIN32)
			COMMAND echo "\""${LUXRAYS_SOURCE_DIR}"/kernels/"${kernel_lowercase}"_kernel.cl\";" >> ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cpp
		ELSE(WIN32)
			COMMAND cat ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cpp
			COMMAND echo "\;" >> ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cpp
		ENDIF(WIN32)
		MAIN_DEPENDENCY ${LUXRAYS_SOURCE_DIR}/kernels/${kernel_lowercase}_kernel.cl
	)
ENDFOREACH(kernel)

###########################################################################
#
# LuxRays Library
#
###########################################################################

set(LUXRAYS_SRCS
    ${LUXRAYS_SOURCE_DIR}/accelerators/bvhaccel.cpp
    ${LUXRAYS_SOURCE_DIR}/accelerators/mqbvhaccel.cpp
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
    ${LUXRAYS_SOURCE_DIR}/kernels/mqbvh_kernel.cpp
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
    ${LUXRAYS_SOURCE_DIR}/utils/properties.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/sdl/extmeshcache.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/sdl/light.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/sdl/scene.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/sdl/spd.cpp
    ${LUXRAYS_SOURCE_DIR}/utils/sdl/texmap.cpp)

add_library(luxrays STATIC ${LUXRAYS_SRCS})

# copy needed files for compiling LuxRender to dependencies directory, to find them also if builddir name is not "luxrays"
if(APPLE)
	ADD_CUSTOM_COMMAND(
	TARGET luxrays POST_BUILD
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/release/libluxrays.a ../macos/lib/LuxRays
	COMMAND cp -R ${CMAKE_BINARY_DIR}/include/luxrays ../macos/include/LuxRays
	)
endif(APPLE)

###########################################################################
#
# Samples
#
###########################################################################

add_subdirectory(samples/benchsimple)
add_subdirectory(samples/smallluxgpu)
add_subdirectory(samples/benchpixel)
add_subdirectory(samples/smallppmgpu)
if(QT4_FOUND AND BUILD_LUXMARK)
	add_subdirectory(samples/luxmark)
	message(STATUS "Building LuxMark too")
endif(QT4_FOUND AND BUILD_LUXMARK)

###########################################################################
#
# Binary samples directory
#
###########################################################################

set(CMAKE_INSTALL_PREFIX .)
#set(LUXRAYS_BIN_SAMPLES_DIR "luxrays-v${LUXRAYS_VERSION_MAJOR}.${LUXRAYS_VERSION_MINOR}")
set(LUXRAYS_BIN_SAMPLES_DIR "slg-v1.8beta1")

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

###########################################################################
#
# LuxMark binary directory
#
###########################################################################

set(LUXMARK_LINUX64_BIN_DIR "luxmark-linux64-v1.0")
set(LUXMARK_WIN32_BIN_DIR "luxmark-win32-v1.0")

# Win32

add_custom_command(
    OUTPUT "${LUXMARK_WIN32_BIN_DIR}"
    COMMAND rm -rf ${LUXMARK_WIN32_BIN_DIR}
    COMMAND mkdir ${LUXMARK_WIN32_BIN_DIR}
    COMMAND cp -r samples/luxmark/scenes ${LUXMARK_WIN32_BIN_DIR}
    COMMAND cp samples/luxmark/exe/*.* ${LUXMARK_WIN32_BIN_DIR}
	COMMAND mv ${LUXMARK_WIN32_BIN_DIR}/LuxMark.exe ${LUXMARK_WIN32_BIN_DIR}/LuxMark-win32.exe
    COMMAND cp AUTHORS.txt COPYING.txt README.txt ${LUXMARK_WIN32_BIN_DIR}
    COMMENT "Building ${LUXMARK_WIN32_BIN_DIR}")

add_custom_command(
    OUTPUT "${LUXMARK_WIN32_BIN_DIR}.zip"
    COMMAND zip -r ${LUXMARK_WIN32_BIN_DIR}.zip ${LUXMARK_WIN32_BIN_DIR}
    COMMAND rm -rf ${LUXMARK_WIN32_BIN_DIR}
    DEPENDS ${LUXMARK_WIN32_BIN_DIR}
    COMMENT "Building ${LUXMARK_WIN32_BIN_DIR}.zip")

# Linux64

add_custom_command(
    OUTPUT "${LUXMARK_LINUX64_BIN_DIR}"
    COMMAND rm -rf ${LUXMARK_LINUX64_BIN_DIR}
    COMMAND mkdir ${LUXMARK_LINUX64_BIN_DIR}
    COMMAND cp -r samples/luxmark/scenes ${LUXMARK_LINUX64_BIN_DIR}
    COMMAND cp bin/luxmark ${LUXMARK_LINUX64_BIN_DIR}/luxmark-linux64
    COMMAND cp AUTHORS.txt COPYING.txt README.txt ${LUXMARK_LINUX64_BIN_DIR}
    COMMENT "Building ${LUXMARK_LINUX64_BIN_DIR}")

add_custom_command(
    OUTPUT "${LUXMARK_LINUX64_BIN_DIR}.zip"
    COMMAND zip -r ${LUXMARK_LINUX64_BIN_DIR}.zip ${LUXMARK_LINUX64_BIN_DIR}
    COMMAND rm -rf ${LUXMARK_LINUX64_BIN_DIR}
    DEPENDS ${LUXMARK_LINUX64_BIN_DIR}
    COMMENT "Building ${LUXMARK_LINUX64_BIN_DIR}.zip")

add_custom_target(luxmark_all_zip DEPENDS "${LUXMARK_LINUX64_BIN_DIR}.zip" "${LUXMARK_WIN32_BIN_DIR}.zip")

################################################################################

message(STATUS ${INCLUDE_DIRS})

IF(APPLE)
	MESSAGE(STATUS "")
	MESSAGE(STATUS "################ GENERATED XCODE PROJECT INFORMATION ################")
	MESSAGE(STATUS "")
	MESSAGE(STATUS "OSX_DEPLOYMENT_TARGET : " ${CMAKE_OSX_DEPLOYMENT_TARGET})
	IF(CMAKE_VERSION VERSION_LESS 2.8.1)
		MESSAGE(STATUS "Setting CMAKE_OSX_ARCHITECTURES ( cmake lower 2.8 method ): " ${CMAKE_OSX_ARCHITECTURES})
	ELSE(CMAKE_VERSION VERSION_LESS 2.8.1)
		MESSAGE(STATUS "CMAKE_XCODE_ATTRIBUTE_ARCHS ( cmake 2.8 or higher method ): " ${CMAKE_XCODE_ATTRIBUTE_ARCHS})
	ENDIF(CMAKE_VERSION VERSION_LESS 2.8.1)
	MESSAGE(STATUS "OSX SDK SETTING : " ${CMAKE_OSX_SYSROOT})
	MESSAGE(STATUS "BUILD_CONFIGURATION_TYPE : " ${CMAKE_CONFIGURATION_TYPES})
	MESSAGE(STATUS "")
	MESSAGE(STATUS "#####################################################################")
ENDIF(APPLE)
