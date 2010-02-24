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

set(SLG_VERSION_MAJOR 1)
set(SLG_VERSION_MINOR 4beta1)

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/slgcfg.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/slgcfg.h"
  )

include_directories(${LUXRAYS_INCLUDE_DIR})
link_directories (${LUXRAYS_LIB_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../${LUXRAYS_BIN_DIR}")

set(SMALLLUXGPU_SRCS
    displayfunc.cpp
    path.cpp
    renderconfig.cpp
    renderthread.cpp
    scene.cpp
    smallluxgpu.cpp)

add_executable(smallluxgpu ${SMALLLUXGPU_SRCS})
target_link_libraries(smallluxgpu luxrays ${GLUT_LIBRARY} ${OPENGL_LIBRARY} ${OCL_LIBRARY} ${Boost_LIBRARIES})
