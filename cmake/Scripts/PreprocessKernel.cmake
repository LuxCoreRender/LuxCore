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

set(template "${CMAKE_CURRENT_LIST_DIR}/PreprocessedKernel.template")

file(READ ${SRC} CL_SOURCE_CODE)

# Escape quotes:
string(REGEX REPLACE "\\\\" "\\\\\\\\" CL_SOURCE_CODE "${CL_SOURCE_CODE}")
string(REGEX REPLACE "\"" "\\\\\"" CL_SOURCE_CODE "${CL_SOURCE_CODE}")

# Wrap lines in string literals:
string(REGEX REPLACE "\r?\n$" "" CL_SOURCE_CODE "${CL_SOURCE_CODE}")
string(REGEX REPLACE "\r?\n" "\\\\n\"\n\"" CL_SOURCE_CODE "\"${CL_SOURCE_CODE}\\n\"")

configure_file(${template} ${DST} @ONLY)
