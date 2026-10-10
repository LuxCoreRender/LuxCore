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

include(doxylux)
generate_doc(luxrays)

################################################################################
#
# Kernel files
#
################################################################################

# The kernel source file list is shared with the SPIR-V bake project
include("${PROJECT_SOURCE_DIR}/build-system/cmake/KernelFiles.cmake")
list(TRANSFORM LUXRAYS_KERNELS PREPEND "${PROJECT_SOURCE_DIR}/")

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
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/buffer.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/config.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/cuda.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/mc.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/ocl.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/safesave.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/serializationutils.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/thread.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/ply/rply.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/properties.cpp
  ${PROJECT_SOURCE_DIR}/src/luxrays/utils/utils.cpp
  ${PROJECT_SOURCE_DIR}/deps/cuew/src/cuew.cpp
  ${PROJECT_SOURCE_DIR}/deps/clew/src/clew.cpp
)
SOURCE_GROUP("Source Files\\LuxRays Library" FILES ${LUXRAYS_SRCS})


set(generated_kernels_dir "${GENERATED_DIR}/luxrays-kernels")
file(MAKE_DIRECTORY ${generated_kernels_dir})

PreprocessOCLKernels(${generated_kernels_dir} luxrays ${LUXRAYS_KERNELS})

# The generated kernel sources are derived from the kernel source files list
set(LUXRAYS_SRCS_KERNELS)
foreach(luxrays_kernel ${LUXRAYS_KERNELS})
  get_filename_component(luxrays_kernel_name ${luxrays_kernel} NAME_WE)
  list(APPEND LUXRAYS_SRCS_KERNELS ${generated_kernels_dir}/${luxrays_kernel_name}_kernel.cpp)
endforeach()

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
target_link_libraries(luxrays PRIVATE
  embree::embree
  openimageio::openimageio
  boost::boost
  OpenMP::OpenMP_CXX
)

################################################################################
#
# Predefines
#
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
