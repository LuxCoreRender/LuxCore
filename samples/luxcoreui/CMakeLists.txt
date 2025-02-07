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

#############################################################################
#
# GLFW library
#
#############################################################################

add_subdirectory(deps/glfw-3.1.1)
include_directories(deps/glfw-3.1.1/include)
include_directories(deps/nfd/include)

#############################################################################
#
# luxcore UI binary
#
#############################################################################

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
target_link_libraries(luxcoreui PRIVATE luxrays boost::boost luxcore)

if(APPLE)

  TARGET_LINK_LIBRARIES(luxcoreui PRIVATE
	glfw
	expat
	${GLFW_LIBRARIES}
	${LUXCORE_LIBRARY}
	${OPENGL_LIBRARIES}
	${GTK3_LIBRARIES}
	${Boost_LIBRARIES})
else()
TARGET_LINK_LIBRARIES(luxcoreui PRIVATE
	glfw
	${GLFW_LIBRARIES}
	${LUXCORE_LIBRARY}
	${OPENGL_LIBRARIES}
	${GTK3_LIBRARIES}
	${Boost_LIBRARIES})
endif()

if(LINUX)
  target_link_libraries(luxcoreui PRIVATE gtk::gtk)
endif()
