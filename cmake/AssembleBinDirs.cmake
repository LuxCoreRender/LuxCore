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

################################################################################
#
# Binary samples directory
#
################################################################################

IF (WIN32)
	
  # For MSVC moving exe files gets done automatically
  # If there is someone compiling on windows and
  # not using msvc (express is free) - feel free to implement

ELSE (WIN32)
	set(CMAKE_INSTALL_PREFIX .)

	# Windows 32bit
	set(SLG2_BIN_WIN32_DIR "slg-win32-v2.0devel5")
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
	set(SLG2_BIN_WIN64_DIR "slg-win64-v2.0devel5")
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
	set(SLG2_BIN_LINUX64_DIR "slg-linux64-v2.0devel5")
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
