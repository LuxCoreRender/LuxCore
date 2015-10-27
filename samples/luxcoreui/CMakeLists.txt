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

#############################################################################
#
# GLFW library
#
#############################################################################

add_subdirectory(deps/glfw-3.1.1)
include_directories(deps/glfw-3.1.1/include)

#############################################################################
#
# luxcore UI binary
#
#############################################################################

include_directories(deps/imgui-1.45)

set(LUXCOREUI_SRCS
	luxcoreui.cpp
	luxcoreapp.cpp
	uiloop.cpp
	uikeys.cpp
	uimenu.cpp
	uimouse.cpp
	helpwindow.cpp
	logwindow.cpp
	objecteditorwindow.cpp
	pixelfilterwindow.cpp
	renderenginewindow.cpp
	samplerwindow.cpp
	statswindow.cpp
	imgui_impl_glfw.cpp
	deps/imgui-1.45/imgui.cpp
	deps/imgui-1.45/imgui_demo.cpp
	deps/imgui-1.45/imgui_draw.cpp
	)

add_executable(luxcoreui ${LUXCOREUI_SRCS})

TARGET_LINK_LIBRARIES(luxcoreui glfw ${GLFW_LIBRARIES} luxcore smallluxgpu luxrays ${EMBREE_LIBRARY} ${OPENGL_LIBRARIES} ${TIFF_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES})
