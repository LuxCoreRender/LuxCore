/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_KERNELS_H
#define	_SLG_KERNELS_H

#include <string>

namespace slg { namespace ocl {

extern std::string KernelSource_pathoclbase_funcs;
extern std::string KernelSource_pathoclbase_datatypes;
extern std::string KernelSource_pathoclbase_funcs;
extern std::string KernelSource_pathoclbase_kernels_micro;
extern std::string KernelSource_tilepathocl_datatypes;
extern std::string KernelSource_tilepathocl_funcs;
extern std::string KernelSource_tilepathocl_sampleresult_funcs;
extern std::string KernelSource_tilepathocl_funcs;
extern std::string KernelSource_tilepathocl_kernels_common;
extern std::string KernelSource_tilepathocl_kernels_micro;
extern std::string KernelSource_sampler_types;
extern std::string KernelSource_sampler_funcs;
extern std::string KernelSource_sampler_random_funcs;
extern std::string KernelSource_sampler_sobol_funcs;
extern std::string KernelSource_sampler_metropolis_funcs;
extern std::string KernelSource_sampler_tilepath_funcs;
extern std::string KernelSource_film_types;
extern std::string KernelSource_film_funcs;
extern std::string KernelSource_filmdenoiser_funcs;
extern std::string KernelSource_filter_types;
extern std::string KernelSource_filter_funcs;
extern std::string KernelSource_camera_types;
extern std::string KernelSource_camera_funcs;
extern std::string KernelSource_bsdf_types;
extern std::string KernelSource_bsdf_funcs;
extern std::string KernelSource_bsdfutils_funcs;
extern std::string KernelSource_indexbvh_types;
extern std::string KernelSource_imagemap_types;
extern std::string KernelSource_imagemap_funcs;
extern std::string KernelSource_material_types;
extern std::string KernelSource_materialdefs_funcs_generic;
extern std::string KernelSource_materialdefs_funcs_default;
extern std::string KernelSource_materialdefs_funcs_thinfilmcoating;
extern std::string KernelSource_materialdefs_funcs_archglass;
extern std::string KernelSource_materialdefs_funcs_carpaint;
extern std::string KernelSource_materialdefs_funcs_clearvol;
extern std::string KernelSource_materialdefs_funcs_cloth;
extern std::string KernelSource_materialdefs_funcs_disney;
extern std::string KernelSource_materialdefs_funcs_glass;
extern std::string KernelSource_materialdefs_funcs_glossy2;
extern std::string KernelSource_materialdefs_funcs_glossycoating;
extern std::string KernelSource_materialdefs_funcs_glossytranslucent;
extern std::string KernelSource_materialdefs_funcs_heterogeneousvol;
extern std::string KernelSource_materialdefs_funcs_homogeneousvol;
extern std::string KernelSource_materialdefs_funcs_matte;
extern std::string KernelSource_materialdefs_funcs_matte_translucent;
extern std::string KernelSource_materialdefs_funcs_metal2;
extern std::string KernelSource_materialdefs_funcs_mirror;
extern std::string KernelSource_materialdefs_funcs_mix;
extern std::string KernelSource_materialdefs_funcs_null;
extern std::string KernelSource_materialdefs_funcs_roughglass;
extern std::string KernelSource_materialdefs_funcs_roughmatte_translucent;
extern std::string KernelSource_materialdefs_funcs_twosided;
extern std::string KernelSource_materialdefs_funcs_velvet;
extern std::string KernelSource_material_funcs_evalops;
extern std::string KernelSource_material_funcs;
extern std::string KernelSource_mortoncurve_funcs;
extern std::string KernelSource_sampleresult_funcs;
extern std::string KernelSource_sceneobject_types;
extern std::string KernelSource_texture_noise_funcs;
extern std::string KernelSource_texture_blender_noise_funcs;
extern std::string KernelSource_texture_blender_noise_funcs2;
extern std::string KernelSource_texture_blender_funcs;
extern std::string KernelSource_texture_bump_funcs;
extern std::string KernelSource_texture_types;
extern std::string KernelSource_texture_abs_funcs;
extern std::string KernelSource_texture_bilerp_funcs;
extern std::string KernelSource_texture_blackbody_funcs;
extern std::string KernelSource_texture_bombing_funcs;
extern std::string KernelSource_texture_brick_funcs;
extern std::string KernelSource_texture_clamp_funcs;
extern std::string KernelSource_texture_colordepth_funcs;
extern std::string KernelSource_texture_densitygrid_funcs;
extern std::string KernelSource_texture_distort_funcs;
extern std::string KernelSource_texture_fresnelcolor_funcs;
extern std::string KernelSource_texture_fresnelconst_funcs;
extern std::string KernelSource_texture_hitpoint_funcs;
extern std::string KernelSource_texture_hsv_funcs;
extern std::string KernelSource_texture_irregulardata_funcs;
extern std::string KernelSource_texture_imagemap_funcs;
extern std::string KernelSource_texture_others_funcs;
extern std::string KernelSource_texture_triplanar_funcs;
extern std::string KernelSource_texture_random_funcs;
extern std::string KernelSource_texture_funcs;
extern std::string KernelSource_texture_funcs_evalops;
extern std::string KernelSource_sampleresult_types;
extern std::string KernelSource_evalstack_funcs;
extern std::string KernelSource_pathdepthinfo_types;
extern std::string KernelSource_pathdepthinfo_funcs;
extern std::string KernelSource_pathvolumeinfo_types;
extern std::string KernelSource_pathvolumeinfo_funcs;
extern std::string KernelSource_pathinfo_types;
extern std::string KernelSource_pathinfo_funcs;
extern std::string KernelSource_varianceclamping_funcs;
extern std::string KernelSource_volume_types;
extern std::string KernelSource_volume_funcs;
extern std::string KernelSource_light_types;
extern std::string KernelSource_light_funcs;
extern std::string KernelSource_lightstrategy_funcs;
extern std::string KernelSource_dlsc_types;
extern std::string KernelSource_dlsc_funcs;
extern std::string KernelSource_elvc_types;
extern std::string KernelSource_elvc_funcs;
extern std::string KernelSource_scene_types;
extern std::string KernelSource_scene_funcs;
extern std::string KernelSource_mapping_types;
extern std::string KernelSource_mapping_funcs;
extern std::string KernelSource_hitpoint_types;
extern std::string KernelSource_hitpoint_funcs;
extern std::string KernelSource_pgic_types;
extern std::string KernelSource_pgic_funcs;
extern std::string KernelSource_pathtracer_types;

// Film and image pipeline kernels
extern std::string KernelSource_film_mergesamplebuffer_funcs;
extern std::string KernelSource_plugin_backgroundimg_funcs;
extern std::string KernelSource_plugin_bloom_funcs;
extern std::string KernelSource_plugin_cameraresponse_funcs;
extern std::string KernelSource_plugin_coloraberration_funcs;
extern std::string KernelSource_plugin_gammacorrection_funcs;
extern std::string KernelSource_plugin_gaussianblur3x3_funcs;
extern std::string KernelSource_plugin_objectidmask_funcs;
extern std::string KernelSource_plugin_optixdenoiser_funcs;
extern std::string KernelSource_plugin_premultiplyalpha_funcs;
extern std::string KernelSource_plugin_vignetting_funcs;
extern std::string KernelSource_plugin_whitebalance_funcs;
extern std::string KernelSource_tonemap_reduce_funcs;
extern std::string KernelSource_tonemap_autolinear_funcs;
extern std::string KernelSource_tonemap_linear_funcs;
extern std::string KernelSource_tonemap_luxlinear_funcs;
extern std::string KernelSource_tonemap_reinhard02_funcs;

} }

#endif	/* _SLG_KERNELS_H */
