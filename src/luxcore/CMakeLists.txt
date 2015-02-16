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
################################################################################
################################################################################
###
### LuxCore Library
###
################################################################################
################################################################################
################################################################################

set(LUXCORE_VERSION_MAJOR 1)
set(LUXCORE_VERSION_MINOR 5dev)

# Configure a header file to pass some of the CMake settings
# to the source code
configure_file(
  "${LuxRays_SOURCE_DIR}/include/luxcore/cfg.h.in"
  "${LuxRays_SOURCE_DIR}/include/luxcore/cfg.h"
  )

################################################################################
#
# LuxCore Docs
#
################################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
	message(STATUS "Found Doxygen and generating LuxCore documentation")

	# Generate doxygen.template
	set(DOXYGEN_LUXCORE_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/luxcore.template)
	configure_file(
	  "${DOXYGEN_LUXCORE_TEMPLATE}.in"
	  "${DOXYGEN_LUXCORE_TEMPLATE}"
	  )

	set(DOXYGEN_LUXCORE_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-luxcore.conf)
	set(DOXYGEN_LUXCORE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/luxcore)
	set(DOXYGEN_LUXCORE_OUTPUT ${DOXYGEN_OUTPUT_DIR}/luxcore/html/index.html)

	message(STATUS "Doxygen LuxCore output: " ${DOXYGEN_LUXCORE_OUTPUT})

	if(DOXYGEN_DOT_FOUND)
			message(STATUS "Found dot")
			set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
	endif(DOXYGEN_DOT_FOUND)

	add_custom_command(
		OUTPUT ${DOXYGEN_LUXCORE_OUTPUT}
		# Creating custom doxygen-luxrays.conf
		COMMAND mkdir -p ${DOXYGEN_LUXCORE_OUTPUT_DIR}
		COMMAND cp ${DOXYGEN_LUXCORE_TEMPLATE} ${DOXYGEN_LUXCORE_INPUT}
		COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/luxcore  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/luxcore >> ${DOXYGEN_LUXCORE_INPUT}
		COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_LUXCORE_OUTPUT_DIR} >> ${DOXYGEN_LUXCORE_INPUT}
		COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_LUXCORE_INPUT}
		# Launch doxygen
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_LUXCORE_INPUT}
		DEPENDS ${DOXYGEN_LUXCORE_TEMPLATE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
	)

	add_custom_target(apidoc_luxcore DEPENDS ${DOXYGEN_LUXCORE_OUTPUT})
endif(DOXYGEN_FOUND)

#############################################################################
#
# Custom Commnad
#
#############################################################################

IF (NOT BISON_NOT_AVAILABLE AND NOT FLEX_NOT_AVAILABLE)
	# Create custom command for bison/yacc
	BISON_TARGET(lux_parser ${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxparse.y ${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxparse.cpp)
	SET_SOURCE_FILES_PROPERTIES(${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxparse.cpp GENERATED)

	# Create custom command for flex/lex
	FLEX_TARGET(lux_lexer ${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxlex.l ${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxlex.cpp COMPILE_FLAGS "${FLEX_FLAGS}")
	SET_SOURCE_FILES_PROPERTIES(${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxlex.cpp GENERATED)
	ADD_FLEX_BISON_DEPENDENCY(lux_lexer lux_parser)

	SET(LUX_PARSER_SRC
		${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxparse.y
		${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxlex.l
	)
	SOURCE_GROUP("Source Files\\Lux Parser" FILES ${LUX_PARSER_SRCS})
ENDIF (NOT BISON_NOT_AVAILABLE AND NOT FLEX_NOT_AVAILABLE)

################################################################################
#
# LuxCore library
#
################################################################################

set(LUXCORE_SRCS
	${LuxRays_SOURCE_DIR}/src/luxcore/luxcore.cpp
	${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxparse.cpp
	${LuxRays_SOURCE_DIR}/src/luxcore/luxparser/luxlex.cpp
)
SOURCE_GROUP("Source Files\\LuxCore Library" FILES ${LUXCORE_LIB_SRCS})	

set(LUXCORE_LIB_SRCS
	${LUXCORE_SRCS}
)

if(APPLE)
	add_library(luxcore SHARED ${LUXCORE_LIB_SRCS} ${LUX_PARSER_SRC})
	add_dependencies(luxcore luxrays smallluxgpu) # build these first
	target_link_libraries(luxcore smallluxgpu luxrays ${EMBREE_LIBRARY} ${Boost_LIBRARIES} ${TIFF_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES})

	if(${CMAKE_GENERATOR} MATCHES "Xcode")
		SET_TARGET_PROPERTIES(luxcore PROPERTIES XCODE_ATTRIBUTE_LD_DYLIB_INSTALL_NAME @loader_path/libluxcore.dylib)
		SET_TARGET_PROPERTIES(luxcore PROPERTIES XCODE_ATTRIBUTE_DYLIB_COMPATIBILITY_VERSION 2.0.0)
		SET_TARGET_PROPERTIES(luxcore PROPERTIES XCODE_ATTRIBUTE_DYLIB_CURRENT_VERSION 2.0.0)
	else()
		SET_TARGET_PROPERTIES(luxcore PROPERTIES LINK_FLAGS "-compatibility_version 2.0.0 -current_version 2.0.0")
	endif()
else(APPLE)
	add_library(luxcore STATIC ${LUXCORE_LIB_SRCS} ${LUX_PARSER_SRC})
endif(APPLE)

add_definitions(-DCPP_API_EXPORTS) # for controlling visibility
link_directories (${SLG_LIB_DIR} ${LuxRays_LIB_DIR})

IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET luxcore POST_BUILD
	COMMAND rm -rf ${OSX_DEPENDENCY_ROOT}/include/LuxRays/luxcore # clean first to assure update
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/libluxcore.dylib ${OSX_DEPENDENCY_ROOT}/lib/LuxRays # for repo
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/libluxcore.dylib ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE} # for slg4
	COMMAND cp -R ${CMAKE_SOURCE_DIR}/include/luxcore ${OSX_DEPENDENCY_ROOT}/include/LuxRays
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)

################################################################################
#
# PyLuxCore (Python bindings)
#
################################################################################

set(PYLUXCORE_SRCS
	${LuxRays_SOURCE_DIR}/src/luxcore/pyluxcore.cpp
	${LuxRays_SOURCE_DIR}/src/luxcore/pyluxcoreforblender.cpp
)

add_library(pyluxcore MODULE ${PYLUXCORE_SRCS})

if(APPLE)
	target_link_libraries(pyluxcore -Wl,-undefined -Wl,dynamic_lookup luxcore smallluxgpu luxrays ${PYTHON_LIBRARIES} ${Boost_LIBRARIES} ${TIFF_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES})
	#target_link_libraries(pyluxcore -Wl,-undefined -Wl,dynamic_lookup ${OSX_DEPENDENCY_ROOT}/lib/liblux.dylib luxcore ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})
    SET_TARGET_PROPERTIES(pyluxcore PROPERTIES XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING NO) # exclude pylux from strip, not possible
else()
	target_link_libraries(pyluxcore luxcore smallluxgpu luxrays ${EMBREE_LIBRARY} ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})
endif()

set_target_properties(pyluxcore PROPERTIES PREFIX "")
if(MSVC)
	set_target_properties(pyluxcore PROPERTIES SUFFIX ".pyd")
endif(MSVC)

################################################################################
#
# Predefines
#
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
