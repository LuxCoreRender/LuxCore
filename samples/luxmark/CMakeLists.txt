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

set(LUXMARK_VERSION_MAJOR 0)
set(LUXMARK_VERSION_MINOR 1)

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/luxmarkcfg.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/luxmarkcfg.h"
  )

###########################################################################
#
# Generate kernel files
#
###########################################################################

add_custom_command(
   OUTPUT pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND echo \"\#include \\"pathgpu/kernels/kernels.h\\"\" > pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND echo "std::string luxrays::KernelSource_PathGPU = " >> pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND cat pathgpu/kernels/pathgpu_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> pathgpu/kernels/pathgpu_kernel.cpp
   COMMAND echo "\;" >> pathgpu/kernels/pathgpu_kernel.cpp
   MAIN_DEPENDENCY pathgpu/kernels/pathgpu_kernel.cl
   )

#############################################################################
#
# LuxMark binary
#
#############################################################################

set(QT_USE_QTOPENGL true)
include(${QT_USE_FILE})

set(LUXMARK_SRCS
	aboutdialog.cpp
	luxmarkapp.cpp
	main.cpp
	mainwindow.cpp
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#!!! Files copied from SLG, do not edit the files !!!
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	path/path.cpp
	pathgpu/kernels/pathgpu_kernel.cpp
	pathgpu/pathgpu.cpp
	renderconfig.cpp
	slgscene.cpp
    sppm/hashgrid.cpp
    sppm/hybridhashgrid.cpp
    sppm/hitpoints.cpp
    sppm/kdtree.cpp
    sppm/sppm.cpp
	volume.cpp
	)
set(LUXMARK_MOC
	aboutdialog.h
	mainwindow.h
	luxmarkapp.h
	)
set(LUXMARK_UIS
	aboutdialog.ui
	mainwindow.ui
	)
set(LUXMARK_RCS
	resources.qrc
	)

QT4_ADD_RESOURCES(LUXMARK_RC_SRCS ${LUXMARK_RCS})
QT4_WRAP_UI(LUXMARK_UI_HDRS ${LUXMARK_UIS})
QT4_WRAP_CPP(LUXMARK_MOC_SRCS ${LUXMARK_MOC})

include_directories(${LUXRAYS_INCLUDE_DIR})
include_directories(".")
link_directories (${LUXRAYS_LIB_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../${LUXRAYS_BIN_DIR}")

set(LUXMARK_SRCS
	${LUXMARK_SRCS}
	${LUXMARK_MOC_SRCS}
	${LUXMARK_RC_SRCS}
	${LUXMARK_UI_HDRS}
	)

add_executable(luxmark ${LUXMARK_SRCS})
target_link_libraries(luxmark luxrays ${QT_LIBRARIES} ${GLEW_LIBRARY} ${GLUT_LIBRARY} ${OPENGL_LIBRARY} ${OCL_LIBRARY} ${Boost_LIBRARIES} ${FREEIMAGE_LIBRARIES})
