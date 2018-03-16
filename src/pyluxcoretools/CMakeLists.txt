################################################################################
# Copyright 1998-2018 by authors (see AUTHORS.txt)
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

add_custom_command(
	OUTPUT "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.py"
	COMMAND pyside-uic "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.ui"
		-o "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.py"
	MAIN_DEPENDENCY "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.ui")

add_custom_command(
	OUTPUT "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetnode/mainwindow.py"
	COMMAND pyside-uic "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetnode/mainwindow.ui"
		-o "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetnode/mainwindow.py"
	MAIN_DEPENDENCY "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetnode/mainwindow.ui")

add_custom_command(
	OUTPUT "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/mainwindow.py"
	COMMAND pyside-uic "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/mainwindow.ui"
		-o "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/mainwindow.py"
	MAIN_DEPENDENCY "${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/mainwindow.ui")

add_custom_target(pyluxcoretools ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory "${LuxRays_SOURCE_DIR}/lib"
	COMMAND ${CMAKE_COMMAND} -E tar "cf" "${LuxRays_SOURCE_DIR}/lib/pyluxcoretools.zip" --format=zip
       "${CMAKE_CURRENT_SOURCE_DIR}/pyluxcoretools"
	DEPENDS
		"${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcoremenu/menuwindow.py"
		"${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetnode/mainwindow.py"
		"${LuxRays_SOURCE_DIR}/src/pyluxcoretools/pyluxcoretools/pyluxcorenetconsole/mainwindow.py")
