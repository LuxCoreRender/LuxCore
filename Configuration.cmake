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

################################################################################
#
# Configuration
#
# Use cmake "-DLUXRAYS_CUSTOM_CONFIG=YouFileCName" To define your personal settings
# where YouFileCName.cname must exist in one of the cmake include directories
# best to use cmake/SpecializedConfig/
# 
# To not load defaults before loading custom values define
# -DLUXRAYS_NO_DEFAULT_CONFIG=true
#
# WARNING: These variables will be cached like any other
#
################################################################################

IF (NOT LUXRAYS_NO_DEFAULT_CONFIG)
	# Disable Boost automatic linking
	ADD_DEFINITIONS(-DBOOST_ALL_NO_LIB)
ELSE(NOT LUXRAYS_NO_DEFAULT_CONFIG)
	MESSAGE(STATUS "LUXRAYS_NO_DEFAULT_CONFIG defined - not using default configuration values.")
ENDIF(NOT LUXRAYS_NO_DEFAULT_CONFIG)

# Setup libraries output directory
SET (LIBRARY_OUTPUT_PATH
   ${PROJECT_BINARY_DIR}/lib
   CACHE PATH
   "Single Directory for all Libraries"
   )

# Setup binaries output directory
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
   CACHE PATH
   "Single Directory for all binaries"
	)

SET(custom_config_dir "cmake/SpecializedConfig")

SET(LUXRAYS_CUSTOM_CONFIG "" CACHE STRING
		"Use custom configuration file in ${custom_config_dir}")

# Find all config files:
FILE(GLOB available_config_files
		RELATIVE "${CMAKE_SOURCE_DIR}/${custom_config_dir}"
		"${CMAKE_SOURCE_DIR}/${custom_config_dir}/*.cmake")

# Remove file extensions:
set(available_configs "")
MESSAGE(STATUS "Available custom configurations in ${custom_config_dir}:")
FOREACH (config_file ${available_config_files})
	GET_FILENAME_COMPONENT(config_name ${config_file} NAME_WE)
	MESSAGE(STATUS "  " ${config_name})
	LIST(APPEND available_configs ${config_name})
ENDFOREACH()
SET_PROPERTY(CACHE LUXRAYS_CUSTOM_CONFIG PROPERTY STRINGS ${available_configs})

#
# Overwrite defaults with Custom Settings
#
IF (LUXRAYS_CUSTOM_CONFIG)
	MESSAGE(STATUS "Using custom build config: ${LUXRAYS_CUSTOM_CONFIG}")
	INCLUDE(${LUXRAYS_CUSTOM_CONFIG})
ENDIF (LUXRAYS_CUSTOM_CONFIG)
