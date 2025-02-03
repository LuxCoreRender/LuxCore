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
### LuxRays Library
###
################################################################################
################################################################################
################################################################################

set(LUXRAYS_VERSION_MAJOR 2)
set(LUXRAYS_VERSION_MINOR 9)

# Configure a header file to pass some of the CMake settings
# to the source code:

set(generated_header_dir "${GENERATED_INCLUDE_DIR}/luxrays")

file(MAKE_DIRECTORY ${generated_header_dir})

configure_file(
    "${PROJECT_SOURCE_DIR}/include/luxrays/cfg.h.in"
    "${generated_header_dir}/cfg.h")

################################################################################
#
# LuxRays Docs
#
################################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
  message(STATUS "Found Doxygen and generating LuxRays documentation")

  # Generate doxygen.template
  set(DOXYGEN_LUXRAYS_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/luxrays.template)
  configure_file(
    "${DOXYGEN_LUXRAYS_TEMPLATE}.in"
    "${DOXYGEN_LUXRAYS_TEMPLATE}"
    )

  set(DOXYGEN_LUXRAYS_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-luxrays.conf)
  set(DOXYGEN_LUXRAYS_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/luxrays)
  set(DOXYGEN_LUXRAYS_OUTPUT ${DOXYGEN_OUTPUT_DIR}/luxrays/html/index.html)

  message(STATUS "Doxygen LuxRays output: " ${DOXYGEN_LUXRAYS_OUTPUT})

  if(DOXYGEN_DOT_FOUND)
      message(STATUS "Found dot")
      set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
  endif(DOXYGEN_DOT_FOUND)

  add_custom_command(
    OUTPUT ${DOXYGEN_LUXRAYS_OUTPUT}
    # Creating custom doxygen-luxrays.conf
    COMMAND mkdir -p ${DOXYGEN_LUXRAYS_OUTPUT_DIR}
    COMMAND cp ${DOXYGEN_LUXRAYS_TEMPLATE} ${DOXYGEN_LUXRAYS_INPUT}
    COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/luxrays  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/luxrays >> ${DOXYGEN_LUXRAYS_INPUT}
    COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_LUXRAYS_OUTPUT_DIR} >> ${DOXYGEN_LUXRAYS_INPUT}
    COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_LUXRAYS_INPUT}
    # Launch doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_LUXRAYS_INPUT}
    DEPENDS ${DOXYGEN_LUXRAYS_TEMPLATE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  )

  add_custom_target(apidoc_luxrays DEPENDS ${DOXYGEN_LUXRAYS_OUTPUT})
endif(DOXYGEN_FOUND)

################################################################################
#
# Kernel files
#
################################################################################

set(LUXRAYS_KERNELS
  ${PROJECT_SOURCE_DIR}/include/luxrays/luxrays_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/color/color_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/color/color_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/bvh/bvhbuild_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/accelerators/bvh.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/accelerators/mbvh.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/accelerators/optixaccel.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/accelerators/optixemptyaccel.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/exttrianglemesh_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/exttrianglemesh_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/randomgen_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/randomgen_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/trianglemesh_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/matrix4x4_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/matrix4x4_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/transform_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/transform_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/frame_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/frame_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/epsilon_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/epsilon_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/ray_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/ray_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/point_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/vector_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/vector_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/normal_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/quaternion_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/quaternion_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/bbox_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/bbox_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/motionsystem_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/motionsystem_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/triangle_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/triangle_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/geometry/uv_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/core/utils_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/devices/cudadevice_math.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/devices/cudadevice_oclemul_types.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/devices/cudadevice_oclemul_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/devices/ocldevice_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/utils/atomic_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/luxrays/utils/mc_funcs.cl
)

# Make sure some IDE won't try to compile CL by some built-in rule:
SET_SOURCE_FILES_PROPERTIES(
    ${LUXRAYS_KERNELS} PROPERTIES HEADER_FILE_ONLY TRUE)

SOURCE_GROUP("Source Files\\LuxRays Kernels" FILES ${LUXRAYS_KERNELS})

################################################################################
#
# Library project files
#
################################################################################

set(LUXRAYS_SRCS
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/bvhaccel.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/bvhaccelhw.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/embreeaccel.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/mbvhaccel.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/mbvhaccelhw.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/optixaccel.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/accelerators/optixaccelhw.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/bvh/bvhbuild.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/bvh/bvhclassicbuild.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/bvh/bvhembreebuild.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/color.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spd.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/blackbodyspd.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/equalspd.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/frequencyspd.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/gaussianspd.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/irregular.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/regular.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/rgbillum.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spds/rgbrefl.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/spectrumwavelengths.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/color/swcspectrum.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/accelerator.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/context.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/dataset.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/device.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/epsilon.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/exttrianglemesh.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/exttrianglemeshbevel.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/exttrianglemeshfile.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/hardwaredevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/hardwareintersectiondevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/init.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/intersectiondevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/namedobject.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/namedobjectvector.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/trianglemesh.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/geometry/basictypeserialization.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/geometry/bbox.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/geometry/matrix4x4.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/geometry/motionsystem.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/geometry/quaternion.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/core/geometry/transform.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/devices/cudadevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/devices/cudaintersectiondevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/devices/nativeintersectiondevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/devices/ocldevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/devices/oclintersectiondevice.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/config.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/cuda.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/mc.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/ocl.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/safesave.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/serializationutils.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/thread.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/ply/rply.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/properties.cpp
  ${PROJECT_SOURCE_DIR}/deps/cuew/src/cuew.cpp
  ${PROJECT_SOURCE_DIR}/deps/clew/src/clew.cpp
)
SOURCE_GROUP("Source Files\\LuxRays Library" FILES ${LUXRAYS_SRCS})


set(generated_kernels_dir "${GENERATED_DIR}/luxrays-kernels")
file(MAKE_DIRECTORY ${generated_kernels_dir})

PreprocessOCLKernels(${generated_kernels_dir} luxrays ${LUXRAYS_KERNELS})

set(LUXRAYS_SRCS_KERNELS
  ${generated_kernels_dir}/luxrays_types_kernel.cpp
  ${generated_kernels_dir}/cudadevice_math_kernel.cpp
  ${generated_kernels_dir}/cudadevice_oclemul_types_kernel.cpp
  ${generated_kernels_dir}/cudadevice_oclemul_funcs_kernel.cpp
  ${generated_kernels_dir}/ocldevice_funcs_kernel.cpp
  ${generated_kernels_dir}/bvhbuild_types_kernel.cpp
  ${generated_kernels_dir}/bvh_kernel.cpp
  ${generated_kernels_dir}/mbvh_kernel.cpp
  ${generated_kernels_dir}/optixaccel_kernel.cpp
  ${generated_kernels_dir}/optixemptyaccel_kernel.cpp
  ${generated_kernels_dir}/randomgen_types_kernel.cpp
  ${generated_kernels_dir}/randomgen_funcs_kernel.cpp
  ${generated_kernels_dir}/trianglemesh_types_kernel.cpp
  ${generated_kernels_dir}/exttrianglemesh_types_kernel.cpp
  ${generated_kernels_dir}/exttrianglemesh_funcs_kernel.cpp
  ${generated_kernels_dir}/matrix4x4_types_kernel.cpp
  ${generated_kernels_dir}/matrix4x4_funcs_kernel.cpp
  ${generated_kernels_dir}/transform_types_kernel.cpp
  ${generated_kernels_dir}/transform_funcs_kernel.cpp
  ${generated_kernels_dir}/frame_types_kernel.cpp
  ${generated_kernels_dir}/frame_funcs_kernel.cpp
  ${generated_kernels_dir}/epsilon_types_kernel.cpp
  ${generated_kernels_dir}/epsilon_funcs_kernel.cpp
  ${generated_kernels_dir}/ray_types_kernel.cpp
  ${generated_kernels_dir}/ray_funcs_kernel.cpp
  ${generated_kernels_dir}/point_types_kernel.cpp
  ${generated_kernels_dir}/vector_types_kernel.cpp
  ${generated_kernels_dir}/vector_funcs_kernel.cpp
  ${generated_kernels_dir}/normal_types_kernel.cpp
  ${generated_kernels_dir}/quaternion_types_kernel.cpp
  ${generated_kernels_dir}/quaternion_funcs_kernel.cpp
  ${generated_kernels_dir}/bbox_types_kernel.cpp
  ${generated_kernels_dir}/bbox_funcs_kernel.cpp
  ${generated_kernels_dir}/motionsystem_types_kernel.cpp
  ${generated_kernels_dir}/motionsystem_funcs_kernel.cpp
  ${generated_kernels_dir}/triangle_types_kernel.cpp
  ${generated_kernels_dir}/triangle_funcs_kernel.cpp
  ${generated_kernels_dir}/color_types_kernel.cpp
  ${generated_kernels_dir}/color_funcs_kernel.cpp
  ${generated_kernels_dir}/uv_types_kernel.cpp
  ${generated_kernels_dir}/mc_funcs_kernel.cpp
  ${generated_kernels_dir}/utils_funcs_kernel.cpp
  ${generated_kernels_dir}/atomic_funcs_kernel.cpp
)

SOURCE_GROUP(
    "Source Files\\LuxRays Kernels\\Generated"
    FILES ${LUXRAYS_SRCS_KERNELS})

set(LUXRAYS_LIB_SRCS
  ${LUXRAYS_SRCS}
  ${LUXRAYS_KERNELS}
  ${LUXRAYS_SRCS_KERNELS}
)

add_library(luxrays STATIC ${LUXRAYS_LIB_SRCS})
add_definitions(-DCPP_API_EXPORTS ${VISIBILITY_FLAGS}) # for controlling visibility

TARGET_LINK_LIBRARIES(luxrays ${OPENIMAGEIO_LIBRARIES})
TARGET_LINK_LIBRARIES(luxrays ${Boost_LIBRARIES})
if (WIN32)
# This is needed by Boost 1.67 but is not found automatically
    TARGET_LINK_LIBRARIES(luxrays bcrypt.lib)
endif(WIN32)

TARGET_LINK_LIBRARIES(luxrays ${CMAKE_DL_LIBS})

target_link_libraries(luxrays embree openimageio::openimageio boost::boost)

################################################################################
#
# Predefines
#
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
