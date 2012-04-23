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

set(LUXMARK_VERSION_MAJOR 2)
set(LUXMARK_VERSION_MINOR 1beta1)

find_package(Qt4 REQUIRED)
IF(QT4_FOUND)
  include(${QT_USE_FILE})
  include_directories(${QT_INCLUDES} ${QT_QTNETWORK_INCLUDE_DIR})
ELSE(QT4_FOUND)
  message(STATUS "Is env variable QTDIR set correctly? Currently: $ENV{QTDIR}")
ENDIF(QT4_FOUND)


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

#
# PathOCL kernel
#
SET(KERNELS kernel_core kernel_datatypes kernel_filters kernel_samplers kernel_scene kernels)
FOREACH(kernel ${KERNELS})
	STRING(TOLOWER ${kernel} kernel_lowercase)

	IF(WIN32)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo "#include \"slg2/pathocl/kernels/kernels.h\"" > ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo std::string luxrays::KernelSource_PathOCL_${kernel} = >> ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND for /F \"tokens=*\" %%a in (${CMAKE_CURRENT_SOURCE_DIR}\\slg2\\pathocl\\kernels\\pathocl_${kernel_lowercase}.cl) do echo \"%%a\\n\" >> ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo ";" >> ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cl
		)
	ELSE(WIN32)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo \"\#include \\"pathocl/kernels/kernels.h\\"\" > ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo "std::string luxrays::KernelSource_PathOCL_${kernel} = " >> ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND cat slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			COMMAND echo "\;" >> ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cpp
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/slg2/pathocl/kernels/pathocl_${kernel_lowercase}.cl
		)
	ENDIF(WIN32)
ENDFOREACH(kernel)

#############################################################################
#
# LuxMark binary
#
#############################################################################

set(QT_USE_QTOPENGL true)
include(${QT_USE_FILE})

set(LUXMARK_SRCS
	aboutdialog.cpp
    hardwaretree.cpp
	luxmarkapp.cpp
	main.cpp
	mainwindow.cpp
    resultdialog.cpp
	submitdialog.cpp
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#!!! Files copied from SLG, do not edit the files !!!
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	slg2/renderconfig.cpp
	slg2/renderengine.cpp
	slg2/rendersession.cpp
	slg2/pathocl/kernels/pathocl_kernel_core.cpp
	slg2/pathocl/kernels/pathocl_kernel_datatypes.cpp
	slg2/pathocl/kernels/pathocl_kernel_filters.cpp
	slg2/pathocl/kernels/pathocl_kernel_samplers.cpp
	slg2/pathocl/kernels/pathocl_kernel_scene.cpp
	slg2/pathocl/kernels/pathocl_kernels.cpp
	slg2/pathocl/compiledscene.cpp
	slg2/pathocl/pathocl.cpp
	slg2/pathocl/renderthread.cpp
	)
set(LUXMARK_MOC
	aboutdialog.h
	mainwindow.h
    hardwaretree.h
	luxmarkapp.h
    resultdialog.h
	submitdialog.h
	)
set(LUXMARK_UIS
	aboutdialog.ui
	mainwindow.ui
    resultdialog.ui
	submitdialog.ui
	)
set(LUXMARK_RCS
	resources.qrc
	)

QT4_ADD_RESOURCES(LUXMARK_RC_SRCS ${LUXMARK_RCS})
QT4_WRAP_UI(LUXMARK_UI_HDRS ${LUXMARK_UIS})
QT4_WRAP_CPP(LUXMARK_MOC_SRCS ${LUXMARK_MOC})

include_directories(${LUXRAYS_INCLUDE_DIR})
include_directories(".")
include_directories("slg2")
link_directories (${LUXRAYS_LIB_DIR})

set(LUXMARK_SRCS
	${LUXMARK_SRCS}
	${LUXMARK_MOC_SRCS}
	${LUXMARK_RC_SRCS}
	${LUXMARK_UI_HDRS}
	)

#############################################################################
#
#  APPLE CUSTOM GUI_TYPE MACOSX_BUNDLE 
#
#############################################################################

IF(APPLE)
	SET(GUI_TYPE MACOSX_BUNDLE)
	# SET(MACOSX_BUNDLE_LONG_VERSION_STRING "${OPENSCENEGRAPH_MAJOR_VERSION}.${OPENSCENEGRAPH_MINOR_VERSION}.${OPENSCENEGRAPH_PATCH_VERSION}")
	# Short Version is the "marketing version". It is the version
	# the user sees in an information panel.
	SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "v${LUXMARK_VERSION_MAJOR}.${LUXMARK_VERSION_MINOR}")
	# Bundle version is the version the OS looks at.
	SET(MACOSX_BUNDLE_BUNDLE_VERSION "v${LUXMARK_VERSION_MAJOR}.${LUXMARK_VERSION_MINOR}")
	SET(MACOSX_BUNDLE_GUI_IDENTIFIER "org.luxrenderk.luxrender" )
	SET(MACOSX_BUNDLE_BUNDLE_NAME "LuxMark" )
	SET(MACOSX_BUNDLE_ICON_FILE "LuxMark.icns")
	# SET(MACOSX_BUNDLE_COPYRIGHT "")
	# SET(MACOSX_BUNDLE_INFO_STRING "Info string, localized?")
ENDIF(APPLE)

#############################################################################

IF(APPLE)
	add_executable(LuxMark  ${GUI_TYPE} ${LUXMARK_SRCS})
	target_link_libraries(LuxMark luxrays ${QT_LIBRARIES} ${QT_QTNETWORK_LIBRARIES} ${GLEW_LIBRARY} ${GLUT_LIBRARY})
	ADD_CUSTOM_COMMAND(
			TARGET LuxMark POST_BUILD
				COMMAND rm -rf ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/LuxMark.app/Contents/Resources
				COMMAND mkdir ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/LuxMark.app/Contents/Resources
				COMMAND cp ${OSX_DEPENDENCY_ROOT}/icons/luxrender.icns ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/LuxMark.app/Contents/Resources/
				COMMAND mv ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/LuxMark.app/Contents/Resources/luxrender.icns ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/LuxMark.app/Contents/Resources/LuxMark.icns
#				COMMAND macdeployqt ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/LuxMark.app ### bundling Qt frameworks ###
				)
ELSE(APPLE)

	ADD_EXECUTABLE(luxmark ${LUXMARK_SRCS})

	TARGET_LINK_LIBRARIES(luxmark luxrays ${QT_LIBRARIES} ${QT_QTNETWORK_LIBRARIES} ${GLEW_LIBRARY} ${OPENGL_LIBRARY})
	TARGET_LINK_LIBRARIES(luxmark ${OpenCL_LIBRARY})
	TARGET_LINK_LIBRARIES(luxmark ${GLUT_LIBRARY})
	
	# This instructs FREEGLUT to emit a pragma for the static version
	SET_TARGET_PROPERTIES(luxmark PROPERTIES COMPILE_DEFINITIONS FREEGLUT_STATIC)

ENDIF(APPLE)