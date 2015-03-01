################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
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
# SLG binary
#
#############################################################################

set(SMALLLUXGPU_SRCS
	smallluxgpu.cpp
	displayfunc.cpp
	)

add_executable(slg4 ${SMALLLUXGPU_SRCS})

if(APPLE)
	TARGET_LINK_LIBRARIES(slg4 luxcore ${OPENGL_LIBRARIES} ${GLEW_LIBRARY} ${GLUT_LIBRARY})
else(APPLE)
	TARGET_LINK_LIBRARIES(slg4 luxcore smallluxgpu luxrays ${EMBREE_LIBRARY} ${OPENGL_LIBRARIES} ${GLEW_LIBRARY} ${GLUT_LIBRARY} ${TIFF_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES})
endif(APPLE)

# This instructs FREEGLUT to emit a pragma for the static version
SET_TARGET_PROPERTIES(slg4 PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)

IF(APPLE AND EMBREE_FOUND)
	ADD_CUSTOM_COMMAND(
	TARGET slg4 POST_BUILD
	COMMAND cp ${OSX_DEPENDENCY_ROOT}/lib/embree2/libembree.2.4.0.dylib ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/libembree.2.4.0.dylib
	)
ENDIF()

IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET slg4 POST_BUILD
	COMMAND rm -rf ${OSX_DEPENDENCY_ROOT}/bin/slg4 # clean first to assure update
	COMMAND cp -f ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/slg4 ${OSX_DEPENDENCY_ROOT}/bin/slg4
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)
