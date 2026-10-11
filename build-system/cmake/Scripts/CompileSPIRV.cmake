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
#
# SPIRV_COMPILE_MODE selects the compilation chain (see
# build-system/cmake/SPIRVCompiler.cmake):
# - "direct": the clang SPIR-V back-end compiles the source in one step.
# - "spir": the clang emits LLVM bitcode (spir64 pseudo target) and the
#   LLVM_SPIRV_EXECUTABLE tool (SPIRV-LLVM-Translator) translates it to
#   SPIR-V. Used where the clang builds don't include the SPIR-V back-end
#   (e.g. the official Windows ones).

if(NOT DEFINED CLANG_EXECUTABLE OR NOT DEFINED SPIRV_COMPILE_MODE OR NOT DEFINED SRC OR NOT DEFINED PARAMS OR NOT DEFINED DST)
  message(FATAL_ERROR "Usage: cmake -DCLANG_EXECUTABLE=... -DSPIRV_COMPILE_MODE=... -DSRC=... -DPARAMS=... -DDST=... -P CompileSPIRV.cmake")
endif()

file(STRINGS ${PARAMS} SPIRV_PARAMETERS)

if(SPIRV_COMPILE_MODE STREQUAL "direct")
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
elseif(SPIRV_COMPILE_MODE STREQUAL "spir")
  if(NOT DEFINED LLVM_SPIRV_EXECUTABLE OR LLVM_SPIRV_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "SPIRV_COMPILE_MODE is 'spir' but LLVM_SPIRV_EXECUTABLE is not set")
  endif()

  # Step 1/2: OpenCL C -> LLVM bitcode
  set(SPIRV_BC "${DST}.bc")
  execute_process(
    COMMAND ${CLANG_EXECUTABLE} --target=spir64-unknown-unknown -x cl
    ${SPIRV_PARAMETERS}
    -emit-llvm -c ${SRC} -o ${SPIRV_BC}
    RESULT_VARIABLE SPIRV_RESULT
    OUTPUT_VARIABLE SPIRV_OUTPUT
    ERROR_VARIABLE SPIRV_OUTPUT
  )
  if(NOT SPIRV_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to compile ${SRC} in SPIR format:\n${SPIRV_OUTPUT}")
  endif()

  # Step 2/2: LLVM bitcode -> SPIR-V
  execute_process(
    COMMAND ${LLVM_SPIRV_EXECUTABLE} ${SPIRV_BC} -o ${DST}
    RESULT_VARIABLE SPIRV_RESULT
    OUTPUT_VARIABLE SPIRV_OUTPUT
    ERROR_VARIABLE SPIRV_OUTPUT
  )
  file(REMOVE ${SPIRV_BC})
  if(NOT SPIRV_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to translate ${SPIRV_BC} in SPIR-V format:\n${SPIRV_OUTPUT}")
  endif()
else()
  message(FATAL_ERROR "Unknown SPIRV_COMPILE_MODE: ${SPIRV_COMPILE_MODE}")
endif()
