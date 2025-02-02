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

find_package(Doxygen)

if(DOXYGEN_FOUND)
  message(STATUS "Found Doxygen and generating SLG documentation")

  # Generate doxygen.template
  set(DOXYGEN_SLG_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/slg.template)
  configure_file(
    "${DOXYGEN_SLG_TEMPLATE}.in"
    "${DOXYGEN_SLG_TEMPLATE}"
    )

  set(DOXYGEN_SLG_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-slg.conf)
  set(DOXYGEN_SLG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/slg)
  set(DOXYGEN_SLG_OUTPUT ${DOXYGEN_OUTPUT_DIR}/slg/html/index.html)

  message(STATUS "Doxygen SLG output: " ${DOXYGEN_SLG_OUTPUT})

  if(DOXYGEN_DOT_FOUND)
      message(STATUS "Found dot")
      set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
  endif(DOXYGEN_DOT_FOUND)

  add_custom_command(
    OUTPUT ${DOXYGEN_SLG_OUTPUT}
    # Creating custom doxygen-luxrays.conf
    COMMAND mkdir -p ${DOXYGEN_SLG_OUTPUT_DIR}
    COMMAND cp ${DOXYGEN_SLG_TEMPLATE} ${DOXYGEN_SLG_INPUT}
    COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/slg  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/slg >> ${DOXYGEN_SLG_INPUT}
    COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_SLG_OUTPUT_DIR} >> ${DOXYGEN_SLG_INPUT}
    COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_SLG_INPUT}
    # Launch doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SLG_INPUT}
    DEPENDS ${DOXYGEN_SLG_TEMPLATE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  )

  add_custom_target(apidoc_slg DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

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
  target_link_libraries(bcd PRIVATE llvm-openmp::llvm-openmp)
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

set(SLG_KERNELS
  ${PROJECT_SOURCE_DIR}/include/slg/cameras/camera_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/cameras/camera_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/engines/caches/photongi/pgic_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/engines/caches/photongi/pgic_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/engines/pathoclbase/kernels/pathoclbase_datatypes.cl
  ${PROJECT_SOURCE_DIR}/include/slg/engines/pathoclbase/kernels/pathoclbase_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/engines/pathoclbase/kernels/pathoclbase_kernels_micro.cl
  ${PROJECT_SOURCE_DIR}/include/slg/engines/pathtracer_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/film_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/film_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/film_mergesamplebuffer_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/denoiser/filmdenoiser_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/sampleresult_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_backgroundimg_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_bloom_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_cameraresponse_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_coloraberration_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_gammacorrection_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_gaussianblur3x3_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_objectidmask_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_optixdenoiser_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_premultiplyalpha_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_vignetting_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_whitebalance_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_reduce_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_autolinear_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_linear_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_luxlinear_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_reinhard02_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/sampleresult_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/filters/filter_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/film/filters/filter_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/imagemap/imagemap_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/imagemap/imagemap_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/light_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/light_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/strategies/dlsc_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/strategies/dlsc_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/strategies/lightstrategy_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/visibility/elvc_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/lights/visibility/elvc_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/samplers/sampler_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/samplers/sampler_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/samplers/sampler_random_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/samplers/sampler_sobol_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/samplers/sampler_metropolis_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/samplers/sampler_tilepath_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/bsdf/bsdf_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/bsdf/bsdf_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/bsdf/bsdfutils_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/bsdf/hitpoint_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/bsdf/hitpoint_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/material_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_generic.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_default.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_archglass.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_carpaint.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_clearvol.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_cloth.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glass.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glossy2.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glossycoating.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glossytranslucent.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_heterogeneousvol.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_homogeneousvol.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_matte.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_matte_translucent.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_metal2.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_mirror.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_mix.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_null.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_roughglass.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_roughmatte_translucent.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_thinfilmcoating.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_velvet.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_disney.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_twosided.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/material_funcs_evalops.cl
  ${PROJECT_SOURCE_DIR}/include/slg/materials/material_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/scene/scene_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/scene/scene_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/scene/sceneobject_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/hitpoint/texture_hitpoint_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/mapping/mapping_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/mapping/mapping_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/math/texture_abs_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/math/texture_clamp_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/math/texture_random_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_funcs_evalops.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_bilerp_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_blackbody_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_blender_noise_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_blender_noise_funcs2.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_blender_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_bombing_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_brick_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_bump_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_colordepth_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_densitygrid_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_distort_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_hsv_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_irregulardata_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_noise_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_imagemap_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_others_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/texture_triplanar_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/fresnel/texture_fresnelcolor_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/textures/fresnel/texture_fresnelconst_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/evalstack_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/mortoncurve_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/pathdepthinfo_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/pathdepthinfo_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/pathvolumeinfo_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/pathvolumeinfo_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/pathinfo_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/pathinfo_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/utils/varianceclamping_funcs.cl
  ${PROJECT_SOURCE_DIR}/include/slg/volumes/volume_types.cl
  ${PROJECT_SOURCE_DIR}/include/slg/volumes/volume_funcs.cl
)

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
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/meshshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/pointiness.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/randomtriangleaovshape.cpp
  ${PROJECT_SOURCE_DIR}/src/slg/shapes/shape.cpp
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
    boost::boost
    OpenColorIO::OpenColorIO
    OpenVDB::openvdb
    openexr::openexr
    opensubdiv::opensubdiv
    Imath::Imath
    embree
    openimageio::openimageio
    nlohmann_json::nlohmann_json
    robin_hood::robin_hood
)

if(APPLE)
  target_link_libraries(slg-core PRIVATE llvm-openmp::llvm-openmp)
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

set(SLG_KERNEL_SRCS
  ${generated_kernels_dir}/sampler_types_kernel.cpp
  ${generated_kernels_dir}/sampler_funcs_kernel.cpp
  ${generated_kernels_dir}/sampler_random_funcs_kernel.cpp
  ${generated_kernels_dir}/sampler_sobol_funcs_kernel.cpp
  ${generated_kernels_dir}/sampler_metropolis_funcs_kernel.cpp
  ${generated_kernels_dir}/sampler_tilepath_funcs_kernel.cpp
  ${generated_kernels_dir}/film_types_kernel.cpp
  ${generated_kernels_dir}/film_funcs_kernel.cpp
  ${generated_kernels_dir}/film_mergesamplebuffer_funcs_kernel.cpp
  ${generated_kernels_dir}/filmdenoiser_funcs_kernel.cpp
  ${generated_kernels_dir}/sampleresult_types_kernel.cpp
  ${generated_kernels_dir}/plugin_backgroundimg_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_bloom_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_cameraresponse_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_coloraberration_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_gammacorrection_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_gaussianblur3x3_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_objectidmask_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_optixdenoiser_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_premultiplyalpha_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_vignetting_funcs_kernel.cpp
  ${generated_kernels_dir}/plugin_whitebalance_funcs_kernel.cpp
  ${generated_kernels_dir}/tonemap_reduce_funcs_kernel.cpp
  ${generated_kernels_dir}/tonemap_autolinear_funcs_kernel.cpp
  ${generated_kernels_dir}/tonemap_linear_funcs_kernel.cpp
  ${generated_kernels_dir}/tonemap_luxlinear_funcs_kernel.cpp
  ${generated_kernels_dir}/tonemap_reinhard02_funcs_kernel.cpp
  ${generated_kernels_dir}/sampleresult_funcs_kernel.cpp
  ${generated_kernels_dir}/filter_types_kernel.cpp
  ${generated_kernels_dir}/filter_funcs_kernel.cpp
  ${generated_kernels_dir}/camera_types_kernel.cpp
  ${generated_kernels_dir}/camera_funcs_kernel.cpp
  ${generated_kernels_dir}/bsdf_types_kernel.cpp
  ${generated_kernels_dir}/bsdf_funcs_kernel.cpp
  ${generated_kernels_dir}/bsdfutils_funcs_kernel.cpp
  ${generated_kernels_dir}/imagemap_types_kernel.cpp
  ${generated_kernels_dir}/imagemap_funcs_kernel.cpp
  ${generated_kernels_dir}/material_types_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_generic_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_default_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_archglass_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_carpaint_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_clearvol_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_cloth_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_disney_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_glass_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_glossy2_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_glossycoating_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_glossytranslucent_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_heterogeneousvol_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_homogeneousvol_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_matte_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_matte_translucent_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_metal2_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_mirror_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_mix_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_null_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_roughglass_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_roughmatte_translucent_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_thinfilmcoating_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_twosided_kernel.cpp
  ${generated_kernels_dir}/materialdefs_funcs_velvet_kernel.cpp
  ${generated_kernels_dir}/material_funcs_evalops_kernel.cpp
  ${generated_kernels_dir}/material_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_types_kernel.cpp
  ${generated_kernels_dir}/texture_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_funcs_evalops_kernel.cpp
  ${generated_kernels_dir}/texture_blender_noise_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_blender_noise_funcs2_kernel.cpp
  ${generated_kernels_dir}/texture_blender_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_bump_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_noise_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_abs_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_bilerp_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_blackbody_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_bombing_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_brick_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_clamp_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_colordepth_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_densitygrid_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_distort_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_fresnelcolor_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_fresnelconst_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_hitpoint_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_hsv_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_irregulardata_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_imagemap_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_others_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_random_funcs_kernel.cpp
  ${generated_kernels_dir}/texture_triplanar_funcs_kernel.cpp
  ${generated_kernels_dir}/evalstack_funcs_kernel.cpp
  ${generated_kernels_dir}/mortoncurve_funcs_kernel.cpp
  ${generated_kernels_dir}/pathdepthinfo_types_kernel.cpp
  ${generated_kernels_dir}/pathdepthinfo_funcs_kernel.cpp
  ${generated_kernels_dir}/pathvolumeinfo_types_kernel.cpp
  ${generated_kernels_dir}/pathvolumeinfo_funcs_kernel.cpp
  ${generated_kernels_dir}/pathinfo_types_kernel.cpp
  ${generated_kernels_dir}/pathinfo_funcs_kernel.cpp
  ${generated_kernels_dir}/varianceclamping_funcs_kernel.cpp
  ${generated_kernels_dir}/volume_types_kernel.cpp
  ${generated_kernels_dir}/volume_funcs_kernel.cpp
  ${generated_kernels_dir}/light_types_kernel.cpp
  ${generated_kernels_dir}/light_funcs_kernel.cpp
  ${generated_kernels_dir}/dlsc_types_kernel.cpp
  ${generated_kernels_dir}/dlsc_funcs_kernel.cpp
  ${generated_kernels_dir}/elvc_types_kernel.cpp
  ${generated_kernels_dir}/elvc_funcs_kernel.cpp
  ${generated_kernels_dir}/lightstrategy_funcs_kernel.cpp
  ${generated_kernels_dir}/scene_types_kernel.cpp
  ${generated_kernels_dir}/scene_funcs_kernel.cpp
  ${generated_kernels_dir}/sceneobject_types_kernel.cpp
  ${generated_kernels_dir}/mapping_types_kernel.cpp
  ${generated_kernels_dir}/mapping_funcs_kernel.cpp
  ${generated_kernels_dir}/hitpoint_types_kernel.cpp
  ${generated_kernels_dir}/hitpoint_funcs_kernel.cpp
  ${generated_kernels_dir}/pgic_types_kernel.cpp
  ${generated_kernels_dir}/pgic_funcs_kernel.cpp
  ${generated_kernels_dir}/pathoclbase_datatypes_kernel.cpp
  ${generated_kernels_dir}/pathoclbase_funcs_kernel.cpp
  ${generated_kernels_dir}/pathoclbase_kernels_micro_kernel.cpp
  ${generated_kernels_dir}/pathtracer_types_kernel.cpp
)

SOURCE_GROUP("Source Files\\SLG Kernels\\Generated" FILES ${SLG_KERNEL_SRCS})

add_library(slg-kernels STATIC ${SLG_KERNELS} ${SLG_KERNEL_SRCS})

target_link_libraries(slg-kernels PRIVATE boost::boost)

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
    boost::boost
    OpenColorIO::OpenColorIO
    embree
    oidn::oidn
    openimageio::openimageio
)

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
