################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
#
#   This file is part of LuxRender.
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
#
# Generate kernel files
#
###########################################################################

set(SLG_KERNELS
	${LuxRays_SOURCE_DIR}/include/slg/cameras/camera_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/cameras/camera_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathoclbase/pathoclbase_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathoclbase/rtpathoclbase_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/pathocl_datatypes.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/pathocl_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/pathocl_kernels_mega.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/pathocl/kernels/pathocl_kernels_micro.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/biaspathocl/kernels/biaspathocl_datatypes.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/biaspathocl/kernels/biaspathocl_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/biaspathocl/kernels/biaspathocl_kernels_mega.cl
	${LuxRays_SOURCE_DIR}/include/slg/engines/biaspathocl/kernels/biaspathocl_kernels_micro.cl
	${LuxRays_SOURCE_DIR}/include/slg/film/film_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/film/film_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/film/filters/filter_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/film/filters/filter_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/imagemap/imagemap_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/imagemap/imagemap_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/lights/light_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/lights/light_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/samplers/sampler_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/samplers/sampler_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/scene_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/mapping_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/mapping_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/hitpoint_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/bsdf_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/bsdf_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/bsdfutils_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/material_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_generic.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_archglass.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_carpaint.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_clearvol.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_cloth.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_glass.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_glossy2.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_glossytranslucent.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_heterogeneousvol.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_homogeneousvol.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_matte.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_matte_translucent.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_mix.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_metal2.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_mirror.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_null.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_roughglass.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_roughmatte_translucent.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/materialdefs_funcs_velvet.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/material_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/volume_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/volume_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/sdl/volumeinfo_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_types.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_blender_noise_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_blender_noise_funcs2.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_blender_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_bump_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_noise_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_blackbody_funcs.cl
	${LuxRays_SOURCE_DIR}/include/slg/textures/texture_irregulardata_funcs.cl
)

PreprocessOCLKernels(${LuxRays_SOURCE_DIR}/src/slg/kernels slg ${SLG_KERNELS})

#############################################################################
#
# SLG library
#
#############################################################################

set(SLG_SRCS
	${LuxRays_SOURCE_DIR}/src/slg/cameras/camera.cpp
	${LuxRays_SOURCE_DIR}/src/slg/cameras/perspective.cpp
	${LuxRays_SOURCE_DIR}/src/slg/core/sphericalfunction/sphericalfunction.cpp
	${LuxRays_SOURCE_DIR}/src/slg/core/sphericalfunction/photometricdataies.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidircpu/bidircpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidircpu/bidircputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirhybrid/bidirhybrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirhybrid/bidirhybridthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirhybrid/bidirstate.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/bidirvmcpu/hashgrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/cbidirhybrid/cbidirhybrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathoclbase/compiledscene.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbase.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbasethread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/pathocl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathocl/pathoclthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/rtpathocl/rtpathocl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/rtpathocl/rtpathoclthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/lightcpu/lightcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/lightcpu/lightcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathcpu/pathcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathcpu/pathcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/filesaver/filesaver.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathhybrid/pathhybrid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathhybrid/pathhybridthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/pathhybrid/pathstate.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/biaspathcpu/biaspathcpu.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/biaspathcpu/biaspathcputhread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/biaspathocl/biaspathocl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/biaspathocl/biaspathoclthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/rtbiaspathocl/rtbiaspathocl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/engines/rtbiaspathocl/rtbiaspathoclthread.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/film.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/filters/filter.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/imagepipeline.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/cameraresponse.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/contourlines.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/gammacorrection.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/gaussianblur3x3.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/nop.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/outputswitcher.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/autolinear.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/linear.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/luxlinear.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/reinhard02.cpp
	${LuxRays_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/tonemap.cpp
	${LuxRays_SOURCE_DIR}/src/slg/imagemap/imagemap.cpp
	${LuxRays_SOURCE_DIR}/src/slg/imagemap/imagemapcache.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/constantinfinitelight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/distantlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/infinitelight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/laserlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/light.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/lightsourcedefinitions.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/lightstrategy.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/mappointlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/pointlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/projectionlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/sharpdistantlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/sky2light.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/skylight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/spotlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/sunlight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/lights/trianglelight.cpp
	${LuxRays_SOURCE_DIR}/src/slg/rendersession.cpp
	${LuxRays_SOURCE_DIR}/src/slg/renderconfig.cpp
	${LuxRays_SOURCE_DIR}/src/slg/renderengine.cpp
	${LuxRays_SOURCE_DIR}/src/slg/samplers/sampler.cpp
	${LuxRays_SOURCE_DIR}/src/slg/samplers/random.cpp
	${LuxRays_SOURCE_DIR}/src/slg/samplers/sobol.cpp
	${LuxRays_SOURCE_DIR}/src/slg/samplers/soboldata.cpp
	${LuxRays_SOURCE_DIR}/src/slg/samplers/metropolis.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/bsdf.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/extmeshcache.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/material.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/sceneobject.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/scene.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/sdl.cpp
	${LuxRays_SOURCE_DIR}/src/slg/sdl/volume.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/blender_noiselib.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/blender_texture.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/texture.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/texturedefs.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/blackbody.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/constfloat.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/constfloat3.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/imagemaptex.cpp
	${LuxRays_SOURCE_DIR}/src/slg/textures/irregulardata.cpp
	${LuxRays_SOURCE_DIR}/src/slg/utils/convtest/pdiff/lpyramid.cpp
	${LuxRays_SOURCE_DIR}/src/slg/utils/convtest/pdiff/metric.cpp
	${LuxRays_SOURCE_DIR}/src/slg/utils/convtest/convtest.cpp
)
SOURCE_GROUP("Source Files\\SLG Library" FILES ${SLG_LIB_SRCS})	

set(SLG_SRCS_KERNELS
	${LuxRays_SOURCE_DIR}/src/slg/kernels/sampler_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/sampler_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/film_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/film_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/filter_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/filter_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/camera_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/camera_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/mc_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/bsdf_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/bsdf_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/bsdfutils_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/imagemap_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/imagemap_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/material_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_generic_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_archglass_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_carpaint_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_clearvol_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_cloth_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_glass_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_glossy2_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_glossytranslucent_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_heterogeneousvol_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_homogeneousvol_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_matte_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_matte_translucent_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_mix_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_metal2_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_mirror_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_null_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_roughglass_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_roughmatte_translucent_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/materialdefs_funcs_velvet_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/material_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_blender_noise_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_blender_noise_funcs2_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_blender_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_bump_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_noise_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_blackbody_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/texture_irregulardata_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/volume_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/volume_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/volumeinfo_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/light_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/light_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/scene_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/mapping_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/mapping_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/hitpoint_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/pathoclbase_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/rtpathoclbase_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/pathocl_datatypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/pathocl_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/pathocl_kernels_mega_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/pathocl_kernels_micro_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/biaspathocl_datatypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/biaspathocl_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/biaspathocl_kernels_mega_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/slg/kernels/biaspathocl_kernels_micro_kernel.cpp
)
SOURCE_GROUP("Source Files\\SLG Kernels" FILES ${SLG_SRCS_KERNELS})

set(SLG_LIB_SRCS
	${SLG_SRCS}
	${SLG_SRCS_KERNELS}
)

add_library(smallluxgpu STATIC ${SLG_LIB_SRCS})

link_directories (${LuxRays_LIB_DIR})

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")
