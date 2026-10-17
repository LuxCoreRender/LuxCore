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
#############################################################################
#############################################################################
###
### SLG Library
###
#############################################################################
#############################################################################
#############################################################################

#############################################################################
#
# SLG Docs
#
#############################################################################

include(doxylux)
generate_doc(slg)

###########################################################################

link_directories (${LuxRays_LIB_DIR})



add_definitions(-DCPP_API_EXPORTS ${VISIBILITY_FLAGS}) # for controlling visibility

###########################################################################
#
# BCD files
#
###########################################################################

# TODO Provide it as a dependency

set(BCD_SRCS
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/Chronometer.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/CovarianceMatrix.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/Denoiser.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/DenoisingUnit.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/MultiscaleDenoiser.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/SamplesAccumulator.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/SpikeRemovalFilter.cpp
  ${PROJECT_SOURCE_DIR}/deps/bcd-1.1/src/core/Utils.cpp
)

SOURCE_GROUP("Source Files\\BCD Library" FILES ${BCD_SRCS})

include_directories(${PROJECT_SOURCE_DIR}/deps/bcd-1.1/include)
include_directories(${PROJECT_SOURCE_DIR}/deps/bcd-1.1/include/bcd/core)
include_directories(${PROJECT_SOURCE_DIR}/deps/lut)

add_library(bcd STATIC ${BCD_SRCS})

target_link_libraries(bcd PRIVATE boost::boost Eigen3::Eigen)

if(APPLE)
  target_link_libraries(bcd PRIVATE OpenMP::OpenMP)
else()
  target_link_libraries(bcd PRIVATE OpenMP::OpenMP_CXX)
endif(APPLE)

# Required by VisualStudio to have M_PI defined
add_definitions("-D_USE_MATH_DEFINES")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_definitions("-DHAVE_GETRANDOM")
  add_definitions("-DUSE_SSE")
endif()

if(APPLE)
  add_definitions("-DHAVE_ARC4RANDOM_BUF")
  add_definitions("-DUSE_SSE")
endif(APPLE)



###########################################################################
#
# Kernel files
#
###########################################################################

# The kernel source file list is shared with the SPIR-V bake project
include("${PROJECT_SOURCE_DIR}/build-system/cmake/KernelFiles.cmake")
list(TRANSFORM SLG_KERNELS PREPEND "${PROJECT_SOURCE_DIR}/")

# Make sure some IDE won't try to compile CL by some built-in rule:
SET_SOURCE_FILES_PROPERTIES(${SLG_KERNELS} PROPERTIES HEADER_FILE_ONLY TRUE)

SOURCE_GROUP("Source Files\\SLG Kernels" FILES ${SLG_KERNELS})

#############################################################################
#
# SLG Core library
#
#############################################################################

# Note: the following order of object linking is important for GCC when building
# a static library (to avoid an undefined symbol reference with the linker)
set(SLG_CORE_SRCS
  ${PROJECT_SOURCE_DIR}/src/slg/bsdf/hitpoint.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/bsdf/bsdf.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/cameras/camera.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/cameras/environment.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/cameras/orthographic.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/cameras/perspective.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/cameras/projective.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/cameras/stereo.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/colorspace.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/indexbvh.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/indexkdtree.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/indexoctree.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/sdl.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/sphericalfunction/sphericalfunction.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/core/sphericalfunction/photometricdataies.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/renderengine.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/cpurenderengine.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/oclrenderengine.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathtracer.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilerepository.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilerepositoryserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bakecpu/bakecpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bakecpu/bakecpurenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bakecpu/bakecputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bidircpu/bidircpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bidircpu/bidircputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bidircpu/bidircpurenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/bidirvmcpu/hashgrid.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicbvh.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgickdtree.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicoctree.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicproperties.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicradius.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicupdate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicvisibility.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/tracephotonsthread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/caches/photongi/photongicache.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compiledscene.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilecamera.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilefilm.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilegeometry.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compileimagemaps.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilelights.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilematerials.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilepathtracer.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilephotongi.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compilesceneobjects.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/compiletextures.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbase.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbasenativethread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthreadfilm.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthreadinit.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthreadkernels.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathocl/pathocl.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathocl/pathoclrenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathocl/pathoclopenclthread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathocl/pathoclnativethread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/rtpathocl/rtpathocl.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/rtpathocl/rtpathoclthread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/lightcpu/lightcpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/lightcpu/lightcpurenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/lightcpu/lightcputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathcpu/pathcpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathcpu/pathcpurenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/pathcpu/pathcputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/rtpathcpu/rtpathcpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/rtpathcpu/rtpathcputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/filesaver/filesaver.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathcpu/tilepathcpu.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathcpu/tilepathcpurenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathcpu/tilepathcputhread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathocl.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathoclrenderstate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathnativethread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathoclthread.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/imagemap.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/imagemapcache.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/imagemapcacheserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/imagemapinstrum.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/imagemapserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/resizepolicies/calcoptsize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/resizepolicies/fixed.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/resizepolicies/minmem.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/resizepolicies/mipmapmem.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/resizepolicies/none.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/imagemap/resizepolicies/resizepolicy.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/constantinfinitelight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/distantlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/infinitelight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/laserlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/light.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/lightsourcedefs.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/mapspherelight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/mappointlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/pointlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/projectionlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/sharpdistantlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/sky2light.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/sky3light.cpp
  ${PROJECT_SOURCE_DIR}/include/slg/lights/data/pragueskymodel/PragueSkyModel.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/spherelight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/spotlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/sunlight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/trianglelight.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/dlscache.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/dlscacheimpl/dlscacheimpl.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/dlscacheimpl/dlscoctree.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/dlscacheimpl/dlscbvh.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/distributionlightstrategy.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/lightstrategy.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/logpower.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/power.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/strategies/uniform.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/lights/visibility/envlightvisibilitycache.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/archglass.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/carpaint.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/cloth.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/disney.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/glass.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/glossy2.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/glossycoating.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/glossytranslucent.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/material.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/materialdefs.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/matte.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/mattetranslucent.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/metal2.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/mirror.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/mixmat.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/null.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/roughglass.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/roughmatte.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/roughmattetranslucent.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/thinfilmcoating.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/twosided.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/materials/velvet.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/sampler.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/random.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/rtpathcpusampler.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/tilepathsampler.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/sobol.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/soboldata.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/sobolsequence.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/samplers/metropolis.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/colorspaceconverters.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/extmeshcache.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/extmeshcacheserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parsecamera.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parselights.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parsematerials.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parseobjects.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parseshapes.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parsetextures.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/parsevolumes.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/sceneobject.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/sceneobjectdefs.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/scene.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/scenepreprocess.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/sceneserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/scene/sceneupdate.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/bevelshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/cameraprojuv.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/displacement.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/edgedetectoraov.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/groupshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/harlequinshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/islandaovshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/merge_on_distance.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/meshshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/pointiness.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/randomtriangleaovshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/shape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/simplify_old.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/simplify.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/strands.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/subdiv.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/slg.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/mapping/mapping.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/band.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/bevel.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/bilerp.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/blackbody.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/blender_noiselib.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/blender_texture.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/bombing.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/brick.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/brightcontrast.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/checkerboard.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/colordepth.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/constfloat.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/constfloat3.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/cloud.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/densitygrid.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/distort.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/dots.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fbm.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnelapprox.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresnelcauchy.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresnelcolor.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresnelconst.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresnelluxpop.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresnelpreset.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresnelsopra.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/fresnel/fresneltexture.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/hitpoint/hitpointaov.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/hitpoint/hitpointcolor.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/hitpoint/position.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/hitpoint/shadingnormal.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/hsv.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/imagemaptex.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/irregulardata.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/lampspectrum.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/marble.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/abs.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/add.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/clamp.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/divide.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/greaterthan.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/lessthan.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/mixtex.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/modulo.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/power.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/random.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/remap.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/rounding.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/scale.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/math/subtract.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/normalmap.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/object_id.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/texture.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/texturedefs.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/triplanar.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/uv.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/vectormath/dotproduct.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/vectormath/makefloat3.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/vectormath/splitfloat3.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/windy.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/wireframe.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/textures/wrinkled.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/filenameresolver.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/film2sceneradius.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/group_by_equivalence.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/harlequincolors.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/meshutils.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/scenevisibility.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/pathdepthinfo.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/pathinfo.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/pathvolumeinfo.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/utils/varianceclamping.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/volumes/clear.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/volumes/heterogenous.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/volumes/homogenous.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/volumes/volume.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/renderconfig.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/rendersession.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/renderstate.cpp
)
SOURCE_GROUP("Source Files\\SLG Core Library" FILES ${SLG_CORE_SRCS})

add_library(slg-core STATIC ${SLG_CORE_SRCS})

target_link_libraries(slg-core PRIVATE
  # slg-kernels must come before luxrays: kernelsources.cpp.o (pulled from
  # slg-kernels) references the luxrays kernel sources and the linker doesn't
  # rescan archives
  slg-kernels
  luxrays
  boost::boost
  OpenColorIO::OpenColorIO
  OpenVDB::openvdb
  openexr::openexr
  opensubdiv::opensubdiv
  Imath::Imath
  embree::embree
  openimageio::openimageio
  nlohmann_json::nlohmann_json
  robin_hood::robin_hood
  OpenMP::OpenMP_CXX
)

if(APPLE)
  target_link_libraries(slg-core PRIVATE OpenMP::OpenMP)
else()
  target_link_libraries(slg-core PRIVATE OpenMP::OpenMP_CXX)
endif(APPLE)

#############################################################################
#
# SLG Kernels library
#
#############################################################################

set(generated_kernels_dir "${GENERATED_DIR}/slg-kernels")
file(MAKE_DIRECTORY ${generated_kernels_dir})

PreprocessOCLKernels(${generated_kernels_dir} slg ${SLG_KERNELS})

# The generated kernel sources are derived from the kernel source files list
set(SLG_KERNEL_SRCS)
foreach(slg_kernel ${SLG_KERNELS})
  get_filename_component(slg_kernel_name ${slg_kernel} NAME_WE)
  list(APPEND SLG_KERNEL_SRCS ${generated_kernels_dir}/${slg_kernel_name}_kernel.cpp)
endforeach()
list(APPEND SLG_KERNEL_SRCS ${PROJECT_SOURCE_DIR}/src/slg/kernels/kernelsources.cpp)

SOURCE_GROUP("Source Files\\SLG Kernels\\Generated" FILES ${SLG_KERNEL_SRCS})

add_library(slg-kernels STATIC ${SLG_KERNELS} ${SLG_KERNEL_SRCS})
target_link_libraries(slg-kernels PRIVATE boost::boost OpenMP::OpenMP_CXX)

#############################################################################
#
# Embedded pre-compiled SPIR-V modules
#
#############################################################################

option(LUXRAYS_PRECOMPILE_SPIRV "Pre-compile the OpenCL kernels in SPIR-V format at build time" ON)

if(LUXRAYS_PRECOMPILE_SPIRV AND APPLE)
  # Apple OpenCL doesn't support the ingestion of SPIR-V modules
  # (cl_khr_il_program): there is no point in baking them
  set(LUXRAYS_PRECOMPILE_SPIRV OFF)
endif()

if(LUXRAYS_PRECOMPILE_SPIRV AND NOT LUXRAYS_DISABLE_OPENCL AND NOT CMAKE_CROSSCOMPILING)
  # The SPIR-V format is platform independent: the modules can be baked
  # beforehand (e.g. by a Linux CI job) and reused on all the platforms.
  # LUXRAYS_SPIRV_BAKED_DIR points to a directory with a spirvmanifest.txt
  # file and the baked <variant>.spv modules (as produced by the in-tree
  # bake below, see the spirv-baker target). When empty, the modules are
  # baked with the host clang.
  # It can also be set through the environment.
  set(LUXRAYS_SPIRV_BAKED_DIR "" CACHE PATH
    "Directory with pre-baked OpenCL kernels in SPIR-V format (spirvmanifest.txt and <variant>.spv files). Leave empty to bake them with the host clang.")
  if(NOT LUXRAYS_SPIRV_BAKED_DIR AND DEFINED ENV{LUXRAYS_SPIRV_BAKED_DIR})
    file(TO_CMAKE_PATH "$ENV{LUXRAYS_SPIRV_BAKED_DIR}" LUXRAYS_SPIRV_BAKED_DIR)
  endif()

  set(spirv_dir "${GENERATED_DIR}/spirv")
  file(MAKE_DIRECTORY ${spirv_dir})

  # SPIRV_VARIANTS is defined in build-system/cmake/KernelFiles.cmake

  set(spirv_module_sources ${spirv_dir}/embeddedspirv.cpp)
  foreach(spirv_variant ${SPIRV_VARIANTS})
    list(APPEND spirv_module_sources ${spirv_dir}/${spirv_variant}_module.cpp)
  endforeach()

  set(spirv_spv_files)

  if(LUXRAYS_SPIRV_BAKED_DIR)
    #--------------------------------------------------------------------
    # Use the pre-baked modules
    #--------------------------------------------------------------------
    set(spirv_baked_dir "${LUXRAYS_SPIRV_BAKED_DIR}")

    if(NOT EXISTS ${spirv_baked_dir}/spirvmanifest.txt)
      message(FATAL_ERROR "Missing ${spirv_baked_dir}/spirvmanifest.txt (LUXRAYS_SPIRV_BAKED_DIR)")
    endif()
    foreach(spirv_variant ${SPIRV_VARIANTS})
      if(NOT EXISTS ${spirv_baked_dir}/${spirv_variant}.spv)
        message(FATAL_ERROR "Missing ${spirv_baked_dir}/${spirv_variant}.spv (LUXRAYS_SPIRV_BAKED_DIR)")
      endif()
      list(APPEND spirv_spv_files ${spirv_baked_dir}/${spirv_variant}.spv)
    endforeach()

    message(STATUS "Using the pre-baked OpenCL kernels in SPIR-V format from ${spirv_baked_dir}")
  else()
    #--------------------------------------------------------------------
    # Bake the modules with the host clang: the direct clang SPIR-V
    # back-end, or - when not available (e.g. the official Windows builds
    # don't include it) - any clang plus the llvm-spirv tool
    # (SPIRV-LLVM-Translator)
    #--------------------------------------------------------------------
    include("${PROJECT_SOURCE_DIR}/build-system/cmake/SPIRVCompiler.cmake")
    find_spirv_compiler()

    if(SPIRV_COMPILE_MODE STREQUAL "none")
      message(STATUS "No way to compile OpenCL C in SPIR-V format (neither a clang including the SPIR-V back-end nor a clang + llvm-spirv chain): OpenCL kernels pre-compilation disabled")
    else()
      message(STATUS "Pre-compiling the OpenCL kernels in SPIR-V format with ${CLANG_EXECUTABLE} (${SPIRV_COMPILE_MODE} mode)")

        # The tool used to write the kernel sources and parameters. The stub of
        # the embedded SPIR-V registry is included as a source because
        # ocl.cpp.o (pulled from luxrays for the kernel hash functions)
        # references it: the real registry can not be linked here (it is
        # generated by running this very tool).
        add_executable(spirv-baker
          ${PROJECT_SOURCE_DIR}/src/slg/kernels/spirvbaker.cpp
          ${PROJECT_SOURCE_DIR}/src/luxrays/utils/embeddedspirvstub.cpp
        )
        # Note: luxrays links its dependencies PRIVATE so they are not
        # propagated here and they have to be linked explicitly.
        # slg-kernels must come before luxrays: kernelsources.cpp.o references
        # the luxrays kernel sources and the linker doesn't rescan archives.
        target_link_libraries(spirv-baker PRIVATE
          slg-kernels
          luxrays
          onetbb::onetbb
          boost::boost
          OpenMP::OpenMP_CXX
          embree::embree
          openimageio::openimageio
        )

        set(spirv_baked_dir "${spirv_dir}/baked")
        file(MAKE_DIRECTORY ${spirv_baked_dir})

        # Write the kernel sources and parameters for each variant
        set(spirv_baker_outputs ${spirv_baked_dir}/spirvmanifest.txt)
        foreach(spirv_variant ${SPIRV_VARIANTS})
          list(APPEND spirv_baker_outputs
            ${spirv_baked_dir}/${spirv_variant}.cl
            ${spirv_baked_dir}/${spirv_variant}.params
          )
        endforeach()

        add_custom_command(
          OUTPUT ${spirv_baker_outputs}
          COMMAND $<TARGET_FILE:spirv-baker> ${spirv_baked_dir}
          DEPENDS spirv-baker
          COMMENT "Baking the OpenCL kernel sources for the SPIR-V pre-compilation"
        )

        # Compile each variant in SPIR-V format
        foreach(spirv_variant ${SPIRV_VARIANTS})
          set(spirv_spv_file ${spirv_baked_dir}/${spirv_variant}.spv)
          list(APPEND spirv_spv_files ${spirv_spv_file})

          add_custom_command(
            OUTPUT ${spirv_spv_file}
            COMMAND ${CMAKE_COMMAND}
              -DCLANG_EXECUTABLE=${CLANG_EXECUTABLE}
              -DSPIRV_COMPILE_MODE=${SPIRV_COMPILE_MODE}
              -DSRC=${spirv_baked_dir}/${spirv_variant}.cl
              -DPARAMS=${spirv_baked_dir}/${spirv_variant}.params
              -DDST=${spirv_spv_file}
              -DLLVM_SPIRV_EXECUTABLE=${LLVM_SPIRV_EXECUTABLE}
              -P "${PROJECT_SOURCE_DIR}/build-system/cmake/Scripts/CompileSPIRV.cmake"
            DEPENDS
              ${spirv_baked_dir}/${spirv_variant}.cl
              ${spirv_baked_dir}/${spirv_variant}.params
            COMMENT "Compiling ${spirv_variant} in SPIR-V format"
          )
        endforeach()
    endif()
  endif()

  if(spirv_spv_files)
    # Generate the sources embedding the SPIR-V modules
    add_custom_command(
      OUTPUT ${spirv_module_sources}
      COMMAND ${CMAKE_COMMAND}
        -DINPUT_DIR=${spirv_baked_dir}
        -DOUTPUT_DIR=${spirv_dir}
        -P "${PROJECT_SOURCE_DIR}/build-system/cmake/Scripts/GenerateEmbeddedSPIRV.cmake"
      DEPENDS
        ${spirv_baked_dir}/spirvmanifest.txt
        ${spirv_spv_files}
        "${PROJECT_SOURCE_DIR}/build-system/cmake/Scripts/GenerateEmbeddedSPIRV.cmake"
      COMMENT "Generating the embedded SPIR-V modules registry"
    )

    add_library(spirv-embedded STATIC ${spirv_module_sources})
  endif()
endif()

if(NOT TARGET spirv-embedded)
  # No embedded pre-compiled SPIR-V modules available
  add_library(spirv-embedded STATIC ${PROJECT_SOURCE_DIR}/src/luxrays/utils/embeddedspirvstub.cpp)
endif()

# Note: spirv-embedded is linked via the $<LINK_GROUP:RESCAN,...> expression in
# src/luxcore/CMakeLists.txt (together with slg-core, slg-film, slg-kernels and
# luxrays, which have mutual references)

#############################################################################
#
# SLG Film library
#
#############################################################################

set(SLG_FILM_SRCS
  ${PROJECT_SOURCE_DIR}/src/slg/film/film.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmaddsample.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmchannels.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmimagepipeline.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmimagepipelinehw.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmoutput.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmoutputs.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmparse.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmsamplescounts.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmsamplesplatter.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmserialize.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filmproperties.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/framebuffer.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/sampleresult.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/convtest/filmconvtest.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/noiseestimation/filmnoiseestimation.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/denoiser/filmdenoiser.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/denoiser/samplesaccumulator.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/filter.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/filterdistribution.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/none.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/blackmanharris.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/box.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/catmullrom.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/gaussian.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/mitchell.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/mitchellss.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/filters/sinc.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/imagepipeline.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/imagepipelineserialization.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/radiancechannelscale.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/radiancechannelscaleserialization.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/backgroundimg.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/bakemapmargin.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/bcddenoiser.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/bloom.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/cameraresponse.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/coloraberration.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/colorlut.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/contourlines.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/gammacorrection.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/gaussianblur3x3.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/mist.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/nop.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/patterns.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/objectidmask.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/outputswitcher.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/vignetting.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/premultiplyalpha.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/autolinear.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/linear.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/luxlinear.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/opencolorio.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/reinhard02.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/tonemap.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/intel_oidn.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/whitebalance.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/optixdenoiser.cpp
)
SOURCE_GROUP("Source Files\\SLG Film Library" FILES ${SLG_FILM_SRCS})

add_library(slg-film STATIC ${SLG_FILM_SRCS})
target_link_libraries(slg-film PRIVATE
    slg-kernels
    luxrays
    boost::boost
    OpenColorIO::OpenColorIO
    embree::embree
    oidn::oidn
    openimageio::openimageio
    OpenMP::OpenMP_CXX
)

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
