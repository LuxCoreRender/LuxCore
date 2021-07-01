################################################################################
# Copyright 1998-2020 by authors (see AUTHORS.txt)
#
#   This file is part of LuxCoreRender.
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
#############################################################################
#############################################################################
###
### PyLuxCoreTools Library
###
#############################################################################
#############################################################################
#############################################################################

include(BuildStage)

set(pyluxcoretools_dir "${CMAKE_CURRENT_SOURCE_DIR}/pyluxcoretools")
set(pyluxcoretools_stage "${GENERATED_DIR}/pyluxcoretools")

BuildStage_directory(
		pyluxcoretools_copy_dir ${pyluxcoretools_stage} pyluxcoretools)

if (PYSIDE_UIC MATCHES "pyside-uic")
	# Delete python pre-generated files since they are PySide2 but PySide has been found
	file(REMOVE
			 "${pyluxcoretools_dir}/pyluxcoremenu/menuwindow.py"
			 "${pyluxcoretools_dir}/pyluxcorenetnode/mainwindow.py"
			 "${pyluxcoretools_dir}/pyluxcorenetconsole/mainwindow.py"
			 "${pyluxcoretools_dir}/pyluxcorenetconsole/addnodedialog.py")
endif()

if (PYSIDE_UIC)

	add_custom_command(
		OUTPUT "${pyluxcoretools_dir}/pyluxcoremenu/menuwindow.py"
		COMMAND ${PYSIDE_UIC} "${pyluxcoretools_dir}/pyluxcoremenu/menuwindow.ui"
			-o "${pyluxcoretools_dir}/pyluxcoremenu/menuwindow.py"
		MAIN_DEPENDENCY "${pyluxcoretools_dir}/pyluxcoremenu/menuwindow.ui")

	add_custom_command(
		OUTPUT "${pyluxcoretools_dir}/pyluxcorenetnode/mainwindow.py"
		COMMAND ${PYSIDE_UIC} "${pyluxcoretools_dir}/pyluxcorenetnode/mainwindow.ui"
			-o "${pyluxcoretools_dir}/pyluxcorenetnode/mainwindow.py"
		MAIN_DEPENDENCY "${pyluxcoretools_dir}/pyluxcorenetnode/mainwindow.ui")

	add_custom_command(
		OUTPUT "${pyluxcoretools_dir}/pyluxcorenetconsole/mainwindow.py"
		COMMAND ${PYSIDE_UIC} "${pyluxcoretools_dir}/pyluxcorenetconsole/mainwindow.ui"
			-o "${pyluxcoretools_dir}/pyluxcorenetconsole/mainwindow.py"
		MAIN_DEPENDENCY "${pyluxcoretools_dir}/pyluxcorenetconsole/mainwindow.ui")

	add_custom_command(
		OUTPUT "${pyluxcoretools_dir}/pyluxcorenetconsole/addnodedialog.py"
		COMMAND ${PYSIDE_UIC} "${pyluxcoretools_dir}/pyluxcorenetconsole/addnodedialog.ui"
			-o "${pyluxcoretools_dir}/pyluxcorenetconsole/addnodedialog.py"
		MAIN_DEPENDENCY "${pyluxcoretools_dir}/pyluxcorenetconsole/addnodedialog.ui")

else()

	message(WARNING "pyside-uic not available "
			"- using pre-generated files from the source tree")

endif()

add_custom_target(pyluxcoretools ALL
	WORKING_DIRECTORY ${GENERATED_DIR}
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib"
	COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/pyluxcoretool/pyluxcoretool.py" "__main__.py"
	COMMAND ${CMAKE_COMMAND} -E
			tar "cf" "${CMAKE_BINARY_DIR}/lib/pyluxcoretools.zip"
					--format=zip pyluxcoretools __main__.py
	DEPENDS
		"${pyluxcoretools_dir}/pyluxcoremenu/menuwindow.py"
		"${pyluxcoretools_dir}/pyluxcorenetnode/mainwindow.py"
		"${pyluxcoretools_dir}/pyluxcorenetconsole/mainwindow.py"
		"${pyluxcoretools_dir}/pyluxcorenetconsole/addnodedialog.py"
		pyluxcoretools_copy_dir)
