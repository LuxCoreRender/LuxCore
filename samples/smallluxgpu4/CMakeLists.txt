###########################################################################
#   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  #
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

TARGET_LINK_LIBRARIES(slg4 smallluxgpu luxrays ${OPENGL_LIBRARIES} ${GLEW_LIBRARY} ${GLUT_LIBRARY})

# This instructs FREEGLUT to emit a pragma for the static version
SET_TARGET_PROPERTIES(slg4 PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)

IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET slg4 POST_BUILD
	COMMAND rm -rf ${OSX_DEPENDENCY_ROOT}/bin/slg4 # clean first to assure update
	COMMAND cp ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/slg4 ${OSX_DEPENDENCY_ROOT}/bin/slg4
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)
