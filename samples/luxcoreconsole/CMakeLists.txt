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
#
# luxcoreconsole binary
#
#############################################################################

set(LUXCORECONSOLE_SRCS luxcoreconsole.cpp)

add_executable(luxcoreconsole ${LUXCORECONSOLE_SRCS})
TARGET_LINK_LIBRARIES(luxcoreconsole PRIVATE luxrays luxcore boost::boost)

if(APPLE)
  TARGET_LINK_LIBRARIES(luxcoreconsole PRIVATE expat "-framework Carbon" "-framework IOKit")
endif()

install(
    TARGETS luxcoreconsole
    RUNTIME_DEPENDENCIES
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
    DIRECTORIES ${CONAN_RUNTIME_LIB_DIRS}
    COMPONENT luxcoreconsole
    FRAMEWORK
        DESTINATION luxcore
    OPTIONAL
)
