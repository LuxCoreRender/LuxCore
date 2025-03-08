################################################################################
# Copyright 1998-2025 by authors (see AUTHORS.txt)
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

# BISON/FLEX

include_directories(${PROJECT_SOURCE_DIR}/deps/bcd-1.1/include)

# Create custom command for bison/yacc
BISON_TARGET(luxbison
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.y
  ${CMAKE_CURRENT_BINARY_DIR}/luxparse.cpp
)

# Create custom command for flex/lex
FLEX_TARGET(luxflex
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxlex.l
  ${CMAKE_CURRENT_BINARY_DIR}/luxlex.cpp COMPILE_FLAGS "${FLEX_FLAGS}"
)
ADD_FLEX_BISON_DEPENDENCY(luxflex luxbison)

set(LUX_PARSER_SRC
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxparse.y
  ${PROJECT_SOURCE_DIR}/src/luxcore/luxparser/luxlex.l
)
source_group("Source Files\\Lux Parser" FILES ${LUX_PARSER_SRC})

add_library(luxparser STATIC ${BISON_luxbison_OUTPUTS} ${FLEX_luxflex_OUTPUTS})
target_link_libraries(luxparser PRIVATE robin_hood::robin_hood boost::boost)
