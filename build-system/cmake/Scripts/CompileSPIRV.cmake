################################################################################
# Copyright 1998-2026 by authors (see AUTHORS.txt)
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

# Compile an OpenCL source in SPIR-V format with the host clang.
#
# The parameters are read from the PARAMS file (one per line) in order to use
# exactly the same parameters used to compute the kernel cache hash.

if(NOT DEFINED CLANG_EXECUTABLE OR NOT DEFINED SRC OR NOT DEFINED PARAMS OR NOT DEFINED DST)
  message(FATAL_ERROR "Usage: cmake -DCLANG_EXECUTABLE=... -DSRC=... -DPARAMS=... -DDST=... -P CompileSPIRV.cmake")
endif()

file(STRINGS ${PARAMS} SPIRV_PARAMETERS)

execute_process(
  COMMAND ${CLANG_EXECUTABLE} --target=spirv64-unknown-unknown -x cl
  ${SPIRV_PARAMETERS}
  -c ${SRC} -o ${DST}
  RESULT_VARIABLE SPIRV_RESULT
  OUTPUT_VARIABLE SPIRV_OUTPUT
  ERROR_VARIABLE SPIRV_OUTPUT
)
if(NOT SPIRV_RESULT EQUAL 0)
  message(FATAL_ERROR "Unable to compile ${SRC} in SPIR-V format:\n${SPIRV_OUTPUT}")
endif()
