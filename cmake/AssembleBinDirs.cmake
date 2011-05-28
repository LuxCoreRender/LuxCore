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

	# Windows 32bit
	set(SLG2_BIN_WIN32_DIR "slg-win32-v2.0devel1")
	add_custom_command(
		OUTPUT "${SLG2_BIN_WIN32_DIR}"
		COMMAND rm -rf ${SLG2_BIN_WIN32_DIR}
		COMMAND mkdir ${SLG2_BIN_WIN32_DIR}
		COMMAND cp -r scenes ${SLG2_BIN_WIN32_DIR}
		COMMAND find ${SLG2_BIN_WIN32_DIR}/scenes -name "*.blend" > filelist.tmp
		COMMAND rm -f `cat filelist.tmp`
		COMMAND rm -f cat filelist.tmp
		COMMAND cp samples/smallluxgpu2/bat/*.bat ${SLG2_BIN_WIN32_DIR}
		COMMAND cp samples/smallluxgpu2/exe-32bit/*.* ${SLG2_BIN_WIN32_DIR}
		COMMAND mkdir ${SLG2_BIN_WIN32_DIR}/blender
		COMMAND cp samples/smallluxgpu2/blender/*.py ${SLG2_BIN_WIN32_DIR}/blender
		COMMAND cp AUTHORS.txt COPYING.txt README.txt ${SLG2_BIN_WIN32_DIR}
		COMMENT "Building ${SLG2_BIN_WIN32_DIR}")

	add_custom_command(
		OUTPUT "${SLG2_BIN_WIN32_DIR}.zip"
		COMMAND zip -r ${SLG2_BIN_WIN32_DIR}.zip ${SLG2_BIN_WIN32_DIR}
		COMMAND rm -rf ${SLG2_BIN_WIN32_DIR}
		DEPENDS ${SLG2_BIN_WIN32_DIR}
		COMMENT "Building ${SLG2_BIN_WIN32_DIR}.zip")

	add_custom_target(slg2_win32_zip DEPENDS "${SLG2_BIN_WIN32_DIR}.zip")

	# Windows 64bit
	set(SLG2_BIN_WIN64_DIR "slg-win64-v2.0devel1")
	add_custom_command(
		OUTPUT "${SLG2_BIN_WIN64_DIR}"
		COMMAND rm -rf ${SLG2_BIN_WIN64_DIR}
		COMMAND mkdir ${SLG2_BIN_WIN64_DIR}
		COMMAND cp -r scenes ${SLG2_BIN_WIN64_DIR}
		COMMAND find ${SLG2_BIN_WIN64_DIR}/scenes -name "*.blend" > filelist.tmp
		COMMAND rm -f `cat filelist.tmp`
		COMMAND rm -f cat filelist.tmp
		COMMAND cp samples/smallluxgpu2/bat/*.bat ${SLG2_BIN_WIN64_DIR}
		COMMAND cp samples/smallluxgpu2/exe-64bit/*.* ${SLG2_BIN_WIN64_DIR}
		COMMAND mkdir ${SLG2_BIN_WIN64_DIR}/blender
		COMMAND cp samples/smallluxgpu2/blender/*.py ${SLG2_BIN_WIN64_DIR}/blender
		COMMAND cp AUTHORS.txt COPYING.txt README.txt ${SLG2_BIN_WIN64_DIR}
		COMMENT "Building ${SLG2_BIN_WIN64_DIR}")

	add_custom_command(
		OUTPUT "${SLG2_BIN_WIN64_DIR}.zip"
		COMMAND zip -r ${SLG2_BIN_WIN64_DIR}.zip ${SLG2_BIN_WIN64_DIR}
		COMMAND rm -rf ${SLG2_BIN_WIN64_DIR}
		DEPENDS ${SLG2_BIN_WIN64_DIR}
		COMMENT "Building ${SLG2_BIN_WIN64_DIR}.zip")

	add_custom_target(slg2_win64_zip DEPENDS "${SLG2_BIN_WIN64_DIR}.zip")

	# Linux 64bit
	set(SLG2_BIN_LINUX64_DIR "slg-linux64-v2.0devel1")
	add_custom_command(
		OUTPUT "${SLG2_BIN_LINUX64_DIR}"
		COMMAND rm -rf ${SLG2_BIN_LINUX64_DIR}
		COMMAND mkdir ${SLG2_BIN_LINUX64_DIR}
		COMMAND cp -r scenes ${SLG2_BIN_LINUX64_DIR}
		COMMAND find ${SLG2_BIN_LINUX64_DIR}/scenes -name "*.blend" > filelist.tmp
		COMMAND rm -f `cat filelist.tmp`
		COMMAND rm -f cat filelist.tmp
		COMMAND cp samples/smallluxgpu2/sh/*.sh ${SLG2_BIN_LINUX64_DIR}
		COMMAND cp bin/slg2 ${SLG2_BIN_LINUX64_DIR}
		COMMAND mkdir ${SLG2_BIN_LINUX64_DIR}/blender
		COMMAND cp samples/smallluxgpu2/blender/*.py ${SLG2_BIN_LINUX64_DIR}/blender
		COMMAND cp AUTHORS.txt COPYING.txt README.txt ${SLG2_BIN_LINUX64_DIR}
		COMMENT "Building ${SLG2_BIN_LINUX64_DIR}")

	add_custom_command(
		OUTPUT "${SLG2_BIN_LINUX64_DIR}.zip"
		COMMAND zip -r ${SLG2_BIN_LINUX64_DIR}.zip ${SLG2_BIN_LINUX64_DIR}
		COMMAND rm -rf ${SLG2_BIN_LINUX64_DIR}
		DEPENDS ${SLG2_BIN_LINUX64_DIR}
		COMMENT "Building ${SLG2_BIN_LINUX64_DIR}.zip")

	add_custom_target(slg2_linux64_zip DEPENDS "${SLG2_BIN_LINUX64_DIR}.zip")
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
