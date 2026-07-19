################################################################################
# Copyright 1998-2025 by Authors (see AUTHORS.txt)
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
#
# luxcoreconsole binary
#
#############################################################################

set(LUXCORECONSOLE_SRCS luxcoreconsole.cpp)

add_executable(luxcoreconsole ${LUXCORECONSOLE_SRCS})
TARGET_LINK_LIBRARIES(luxcoreconsole PRIVATE luxrays luxcore_static boost::boost)

if(APPLE)
  TARGET_LINK_LIBRARIES(luxcoreconsole PRIVATE expat "-framework Carbon" "-framework IOKit")
endif()

set_target_properties(luxcoreconsole PROPERTIES
  INSTALL_RPATH "\$ORIGIN/../lib"
)

install(
    TARGETS luxcoreconsole
    RUNTIME_DEPENDENCY_SET LUXCONSOLE_DEPS
    COMPONENT luxcoreconsole
    FRAMEWORK
        DESTINATION luxcore
    OPTIONAL
    PERMISSIONS
      OWNER_WRITE OWNER_READ OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE
)
install(RUNTIME_DEPENDENCY_SET LUXCONSOLE_DEPS
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ${INSTALL_EXCLUDE_REGEXES}
    DIRECTORIES "${CONAN_RUNTIME_LIB_DIRS}"
    PERMISSIONS
      OWNER_WRITE OWNER_READ OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE
)
