###########################################################################
#   Copyright (C) 1998-2011 by authors (see AUTHORS.txt )                 #
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

include_directories(${LuxRays_INCLUDE_DIR})
link_directories (${LuxRays_LIB_DIR})

add_executable(benchpixel benchpixel.cpp)

target_link_libraries(benchpixel LuxRays ${OPENGL_LIBRARY} ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(benchpixel ${OpenCL_LIBRARY})

# This instructs FREEGLUT to emit a pragma for the static version
SET_TARGET_PROPERTIES(benchpixel PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)