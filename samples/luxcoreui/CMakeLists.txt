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

################################################################################
#
# Find OpenGL
#
################################################################################

find_package(OpenGL)

if (OPENGL_FOUND)
	include_directories(BEFORE SYSTEM ${OPENGL_INCLUDE_PATH})
endif()

if (NOT OPENGL_FOUND)
	MESSAGE(FATAL_ERROR "--> Could not locate required OpenGL files")
endif()

################################################################################
#
# Find GTK 3.0
#
################################################################################

# Find GTK 3.0 for Linux only (required by luxcoreui NFD)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	include_directories(${GTK3_INCLUDE_DIRS})
endif()

################################################################################
#
# GLFW library
#
################################################################################

add_subdirectory(deps/glfw-3.1.1)
include_directories(deps/glfw-3.1.1/include)
include_directories(deps/nfd/include)

################################################################################
#
# luxcore UI binary
#
################################################################################

include_directories(deps/imgui-1.46)

set(LUXCOREUI_SRCS
	luxcoreui.cpp
	luxcoreapp.cpp
	uiloop.cpp
	uikeys.cpp
	uimenu.cpp
	uimouse.cpp
	helpwindow.cpp
	logwindow.cpp
	objectwindow.cpp
	objecteditorwindow.cpp
	acceleratorwindow.cpp
	epsilonwindow.cpp
	filmchannelswindow.cpp
	filmoutputswindow.cpp
	filmradiancegroupswindow.cpp
	imagewindow.cpp
	lightstrategywindow.cpp
	ocldevicewindow.cpp
	pixelfilterwindow.cpp
	renderenginewindow.cpp
	samplerwindow.cpp
	haltconditionswindow.cpp
	statswindow.cpp
	importancewindow.cpp
	imgui_impl_glfw.cpp
	bake.cpp
	resources/luxlogo_bg.cpp
	deps/imgui-1.46/imgui.cpp
	deps/imgui-1.46/imgui_demo.cpp
	deps/imgui-1.46/imgui_draw.cpp
	deps/nfd/nfd_common.c
	)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	SET(LUXCOREUI_SRCS ${LUXCOREUI_SRCS} deps/nfd/nfd_win.cpp)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	SET(LUXCOREUI_SRCS ${LUXCOREUI_SRCS} deps/nfd/nfd_gtk.c)
	add_definitions(${GTK3_CFLAGS_OTHER})
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	SET(LUXCOREUI_SRCS ${LUXCOREUI_SRCS} deps/nfd/nfd_cocoa.m)
ENDIF()

add_executable(luxcoreui ${LUXCOREUI_SRCS})

TARGET_LINK_LIBRARIES(luxcoreui
	glfw
	${LUXCORE_LIBRARY}
	${GLFW_LIBRARIES}
	${OPENGL_LIBRARIES}
	${GTK3_LIBRARIES})
