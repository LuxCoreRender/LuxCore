# SPDX-FileCopyrightText: 2026 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

# Dependency-free standalone project used to pre-compile the OpenCL
# kernels in SPIR-V format (see the LUXRAYS_PRECOMPILE_SPIRV option and
# build-system/cmake/Scripts/GenerateEmbeddedSPIRV.cmake).
#
# It is used by the CI (see .github/workflows/wheel-builder.yml and
# sample-builder.yml) where building LuxCore (i.e. the LuxCoreDeps
# dependencies) just in order to bake the modules would be a waste of
# time and is not even always possible (e.g. on Windows runners no
# compiler able to build LuxCore is available).
#
# It compiles the spirv-baker tool with the system compiler and the
# generated kernel sources alone: no LuxCoreDeps dependency at all.
# The SPIR-V format being platform independent, the result can be used
# by the main build on any platform through LUXRAYS_SPIRV_BAKED_DIR.
#
# Usage:
#
#   cmake -S build-system/spirv -B bake-build -G Ninja
#   cmake --build bake-build
#
# The baked modules are written in bake-build/baked/.

cmake_minimum_required(VERSION 3.20)

project(SpirvBaker CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

# The kernel source files and the bake variants
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/KernelFiles.cmake")
list(TRANSFORM LUXRAYS_KERNELS PREPEND "${SOURCE_ROOT}/")
list(TRANSFORM SLG_KERNELS PREPEND "${SOURCE_ROOT}/")

# The tool used to write the kernel sources and parameters
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/KernelPreprocess.cmake")

set(generated_kernels_dir "${CMAKE_BINARY_DIR}/generated/kernels")
file(MAKE_DIRECTORY ${generated_kernels_dir})

PreprocessOCLKernels(${generated_kernels_dir} luxrays ${LUXRAYS_KERNELS})
PreprocessOCLKernels(${generated_kernels_dir} slg ${SLG_KERNELS})

# The spirv-baker tool: it links the generated kernel sources alone (i.e.
# no LuxCoreDeps dependency)
set(SPIRV_BAKER_SRCS
  "${SOURCE_ROOT}/src/slg/kernels/spirvbaker.cpp"
  "${SOURCE_ROOT}/src/slg/kernels/kernelsources.cpp"
)
foreach(spirv_kernel ${LUXRAYS_KERNELS} ${SLG_KERNELS})
  get_filename_component(spirv_kernel_name ${spirv_kernel} NAME_WE)
  list(APPEND SPIRV_BAKER_SRCS "${generated_kernels_dir}/${spirv_kernel_name}_kernel.cpp")
endforeach()

add_executable(spirv-baker ${SPIRV_BAKER_SRCS})
# luxrays/cfg.h is a generated header (see src/luxrays/CMakeLists.txt).
# The version value doesn't matter here: the tool doesn't use it.
set(generated_header_dir "${CMAKE_BINARY_DIR}/generated/include/luxrays")
file(MAKE_DIRECTORY ${generated_header_dir})
configure_file(
  "${SOURCE_ROOT}/include/luxrays/cfg.h.in"
  "${generated_header_dir}/cfg.h")
target_include_directories(spirv-baker PRIVATE
  "${SOURCE_ROOT}/include"
  "${CMAKE_BINARY_DIR}/generated/include")

# Check the compiler chain used to bake the modules: the direct clang SPIR-V
# back-end, or - when not available (e.g. the official Windows builds don't
# include it) - any clang plus the llvm-spirv tool (SPIRV-LLVM-Translator)
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/SPIRVCompiler.cmake")
find_spirv_compiler()

if(SPIRV_COMPILE_MODE STREQUAL "none")
  if(NOT CLANG_EXECUTABLE)
    message(FATAL_ERROR "clang not found: it is required to bake the OpenCL kernels in SPIR-V format")
  else()
    message(FATAL_ERROR "No way to compile OpenCL C in SPIR-V format: neither a clang including the SPIR-V back-end nor a clang + llvm-spirv (SPIRV-LLVM-Translator) chain is available. Note: the official Windows clang builds don't include the SPIR-V back-end, but the llvm-spirv tool is available for Windows (e.g. in the MSYS2 or Intel oneAPI packages).")
  endif()
endif()
message(STATUS "Baking the OpenCL kernels in SPIR-V format with ${CLANG_EXECUTABLE} (${SPIRV_COMPILE_MODE} mode)")

# Bake the kernel sources and parameters for each variant
set(baked_dir "${CMAKE_BINARY_DIR}/baked")
file(MAKE_DIRECTORY ${baked_dir})

set(spirv_baker_outputs ${baked_dir}/spirvmanifest.txt)
foreach(spirv_variant ${SPIRV_VARIANTS})
  list(APPEND spirv_baker_outputs
    ${baked_dir}/${spirv_variant}.cl
    ${baked_dir}/${spirv_variant}.params
  )
endforeach()

add_custom_command(
  OUTPUT ${spirv_baker_outputs}
  COMMAND $<TARGET_FILE:spirv-baker> ${baked_dir}
  DEPENDS spirv-baker
  COMMENT "Baking the OpenCL kernel sources for the SPIR-V pre-compilation"
)

# Compile each variant in SPIR-V format
set(spirv_spv_files)
foreach(spirv_variant ${SPIRV_VARIANTS})
  set(spirv_spv_file ${baked_dir}/${spirv_variant}.spv)
  list(APPEND spirv_spv_files ${spirv_spv_file})

  add_custom_command(
    OUTPUT ${spirv_spv_file}
    COMMAND ${CMAKE_COMMAND}
      -DCLANG_EXECUTABLE=${CLANG_EXECUTABLE}
      -DSPIRV_COMPILE_MODE=${SPIRV_COMPILE_MODE}
      -DSRC=${baked_dir}/${spirv_variant}.cl
      -DPARAMS=${baked_dir}/${spirv_variant}.params
      -DDST=${spirv_spv_file}
      -DLLVM_SPIRV_EXECUTABLE=${LLVM_SPIRV_EXECUTABLE}
      -P "${SOURCE_ROOT}/build-system/cmake/Scripts/CompileSPIRV.cmake"
    DEPENDS
      ${baked_dir}/${spirv_variant}.cl
      ${baked_dir}/${spirv_variant}.params
    COMMENT "Compiling ${spirv_variant} in SPIR-V format"
  )
endforeach()

add_custom_target(spirv-bake ALL DEPENDS ${spirv_baker_outputs} ${spirv_spv_files})
