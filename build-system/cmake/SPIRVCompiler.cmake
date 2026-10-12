# SPDX-FileCopyrightText: 2026 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

# Detect a compiler chain able to compile OpenCL C sources in SPIR-V format.
#
# Two modes are supported:
# - "direct": a clang including the SPIR-V back-end compiles the sources in
#   one step (e.g. the LLVM packages of the Linux distributions).
# - "spir": any clang emits LLVM bitcode (the spir64 pseudo target is always
#   available, even in the official Windows builds, which don't include the
#   SPIR-V back-end) and the SPIRV-LLVM-Translator (i.e. the llvm-spirv tool)
#   translates it to SPIR-V. On Windows, the llvm-spirv tool is shipped,
#   among others, in the MSYS2 or Intel oneAPI packages.
#
# The mode actually working is verified with a compile probe: the target list
# given by 'clang --print-targets' is not reliable across builds.
#
# Sets in the caller scope:
# - SPIRV_COMPILE_MODE: "direct", "spir" or "none"
# - CLANG_EXECUTABLE: the clang to use
# - LLVM_SPIRV_EXECUTABLE: the translation tool ("spir" mode only)

macro(find_spirv_compiler)
  set(SPIRV_COMPILE_MODE "none")

  find_program(CLANG_EXECUTABLE NAMES clang clang-20 clang-19 clang-18 clang-17)
  if(CLANG_EXECUTABLE)
    # Probe the direct compilation with the clang SPIR-V back-end
    set(spirv_probe_cl "${CMAKE_BINARY_DIR}/spirv-probe.cl")
    set(spirv_probe_spv "${CMAKE_BINARY_DIR}/spirv-probe.spv")
    file(WRITE ${spirv_probe_cl} "__kernel void LuxCoreSPIRVProbe() {}\n")
    execute_process(
      COMMAND ${CLANG_EXECUTABLE} --target=spirv64-unknown-unknown -x cl
      -c ${spirv_probe_cl} -o ${spirv_probe_spv}
      RESULT_VARIABLE spirv_direct_probe_result
      OUTPUT_QUIET ERROR_QUIET
    )
    if(spirv_direct_probe_result EQUAL 0)
      set(SPIRV_COMPILE_MODE "direct")
    else()
      # Fall back on the two-step SPIR pipeline: any clang -> LLVM bitcode ->
      # SPIR-V. Note that the llvm-spirv version must be compatible with the
      # one of the clang emitting the bitcode: the probe checks it.
      find_program(LLVM_SPIRV_EXECUTABLE
        NAMES llvm-spirv llvm-spirv-22 llvm-spirv-21 llvm-spirv-20 llvm-spirv-19 llvm-spirv-18)
      if(LLVM_SPIRV_EXECUTABLE)
        set(spirv_probe_bc "${CMAKE_BINARY_DIR}/spirv-probe.bc")
        execute_process(
          COMMAND ${CLANG_EXECUTABLE} --target=spir64-unknown-unknown -x cl
          -emit-llvm -c ${spirv_probe_cl} -o ${spirv_probe_bc}
          RESULT_VARIABLE spirv_spir_probe_result
          OUTPUT_QUIET ERROR_QUIET
        )
        if(spirv_spir_probe_result EQUAL 0)
          execute_process(
            COMMAND ${LLVM_SPIRV_EXECUTABLE} ${spirv_probe_bc} -o ${spirv_probe_spv}
            RESULT_VARIABLE spirv_translate_probe_result
            OUTPUT_QUIET ERROR_QUIET
          )
          file(REMOVE ${spirv_probe_bc})
          if(spirv_translate_probe_result EQUAL 0)
            set(SPIRV_COMPILE_MODE "spir")
          else()
            message(WARNING "${LLVM_SPIRV_EXECUTABLE} can not translate the LLVM bitcode produced by ${CLANG_EXECUTABLE} (versions mismatch?)")
          endif()
        endif()
      endif()
    endif()

    file(REMOVE ${spirv_probe_cl})
    file(REMOVE ${spirv_probe_spv})
  endif()
endmacro()
