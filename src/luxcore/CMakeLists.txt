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
################################################################################
################################################################################
###
### LuxCore Library
###
################################################################################
################################################################################
################################################################################

# Configure a header file to pass some of the CMake settings
# to the source code:

set(generated_header_dir "${GENERATED_INCLUDE_DIR}/luxcore")

file(MAKE_DIRECTORY ${generated_header_dir})

configure_file(
  "${PROJECT_SOURCE_DIR}/include/luxcore/cfg.h.in"
  "${generated_header_dir}/cfg.h"
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
    COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/luxcore/luxcore.h ${CMAKE_CURRENT_SOURCE_DIR}/../../include/luxrays/utils/properties.h ${CMAKE_CURRENT_SOURCE_DIR}/../../include/luxrays/utils/cyhair/cyHairFile.h >> ${DOXYGEN_LUXCORE_INPUT}
    COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_LUXCORE_OUTPUT_DIR} >> ${DOXYGEN_LUXCORE_INPUT}
    COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_LUXCORE_INPUT}
    # Launch doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_LUXCORE_INPUT}
    DEPENDS ${DOXYGEN_LUXCORE_TEMPLATE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  )

  add_custom_target(apidoc_luxcore DEPENDS ${DOXYGEN_LUXCORE_OUTPUT})
endif(DOXYGEN_FOUND)

################################################################################
#
# Custom Command
#
################################################################################

set(generated_parser_dir "${GENERATED_DIR}/luxparser")
file(MAKE_DIRECTORY ${generated_parser_dir})

if (BISON_FOUND AND FLEX_FOUND)

  # Create custom command for bison/yacc
  BISON_TARGET(lux_parser ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.y ${generated_parser_dir}/luxparse.cpp)
  set_source_files_properties(${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.cpp GENERATED)

  # Create custom command for flex/lex
  FLEX_TARGET(lux_lexer ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxlex.l ${generated_parser_dir}/luxlex.cpp COMPILE_FLAGS "${FLEX_FLAGS}")
  set_source_files_properties(${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxlex.cpp GENERATED)
  ADD_FLEX_BISON_DEPENDENCY(lux_lexer lux_parser)

  set(LUX_PARSER_SRC
    ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.y
    ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxlex.l
  )
  source_group("Source Files\\Lux Parser" FILES ${LUX_PARSER_SRC})

else()

  # Just copy the pre-generated files from the source tree:
  file(COPY "${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.cpp"
    "${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.hpp"
    "${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxlex.cpp" DESTINATION ${generated_parser_dir})

  message(WARNING "Flex or Bison not available "
      "- using pre-generated files from the source tree")
endif()

################################################################################
#
# LuxCore library
#
################################################################################

set(LUXCORE_SRCS
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxcore.cpp
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxcoreimpl.cpp
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxcoreinit.cpp
  ${PROJECT_SOURCE_DIR}/src/luxcore/kernelcachefill.cpp
)
SOURCE_GROUP("Source Files\\LuxCore Library" FILES ${LUXCORE_SRCS})

set(LUXCORE_GENERATED_SRCS
  ${generated_parser_dir}/luxparse.cpp
  ${generated_parser_dir}/luxlex.cpp
)
SOURCE_GROUP(
    "Source Files\\LuxCore Library\\Generated"
    FILES ${LUXCORE_GENERATED_SRCS})

set(LUXCORE_LIB_SRCS
  ${LUXCORE_SRCS}
  ${LUXCORE_GENERATED_SRCS}
)

if (BUILD_LUXCORE_DLL)
  add_library(luxcore SHARED ${LUXCORE_LIB_SRCS} ${LUX_PARSER_SRC})
else()
  add_library(luxcore STATIC ${LUXCORE_LIB_SRCS} ${LUX_PARSER_SRC})
endif()

target_link_libraries(luxcore PRIVATE
    slg-core
    slg-film
    slg-kernels
    luxrays
    bcd
    robin_hood::robin_hood
    boost::boost
    spdlog::spdlog_header_only
    fmt::fmt
    openimageio::openimageio
    embree
)


include_directories(${PROJECT_SOURCE_DIR}/deps/bcd-1.1/include)

add_definitions(-DCPP_API_EXPORTS ${VISIBILITY_FLAGS}) # for controlling visibility
link_directories (${SLG_LIB_DIR} ${LuxRays_LIB_DIR})

set_target_properties(luxcore PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

install(
    TARGETS luxcore
    DESTINATION bin
    RUNTIME_DEPENDENCIES
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
    DIRECTORIES ${CONAN_RUNTIME_LIB_DIRS}
    COMPONENT luxcore
    OPTIONAL
)

################################################################################
#
# PyLuxCore (Python bindings)
#
################################################################################

set(PYLUXCORE_SRCS
  ${PROJECT_SOURCE_DIR}/src/luxcore/pyluxcore.cpp
  ${PROJECT_SOURCE_DIR}/src/luxcore/pyluxcoreforblender.cpp
)

pybind11_add_module(pyluxcore MODULE ${PYLUXCORE_SRCS} ${LUXCORE_LIB_SRCS} ${LUX_PARSER_SRC})

target_link_libraries(pyluxcore PRIVATE
    slg-core
    slg-film
    slg-kernels
    luxrays
    bcd
    OpenVDB::openvdb
    spdlog::spdlog_header_only
    pybind11::pybind11
    Python::Module
    boost::boost
    onetbb::onetbb
    blender-types
    robin_hood::robin_hood
    openimageio::openimageio
    embree
)

if(APPLE)
  target_link_libraries(pyluxcore PRIVATE llvm-openmp::llvm-openmp)
else()
  target_link_libraries(pyluxcore PRIVATE OpenMP::OpenMP_CXX)
endif(APPLE)

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
