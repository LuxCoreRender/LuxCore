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

###########################################################################
#
# Binary samples directory
#
###########################################################################

IF (WIN32)
	
  # For MSVC moving exe files gets done automatically
  # If there is someone compiling on windows and
  # not using msvc (express is free) - feel free to implement

ELSE (WIN32)
	set(CMAKE_INSTALL_PREFIX .)
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
ENDIF(WIN32)

###########################################################################
#
# LuxMark binary directory
#
###########################################################################

IF (WIN32)

ELSE (WIN32)

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
ENDIF(WIN32)
