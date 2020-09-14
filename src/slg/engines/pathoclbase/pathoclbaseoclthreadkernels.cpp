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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/devices/ocldevice.h"
#include "luxrays/kernels/kernels.h"

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread kernels related methods
//------------------------------------------------------------------------------

void PathOCLBaseOCLRenderThread::CompileKernel(HardwareIntersectionDevice *device,
			HardwareDeviceProgram *program,
			HardwareDeviceKernel **kernel,
			size_t *workGroupSize, const std::string &name) {
	delete *kernel;
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	device->GetKernel(program, kernel, name.c_str());

	if (device->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
		*workGroupSize = device->GetDeviceDesc()->GetForceWorkGroupSize();
	else {
		*workGroupSize = device->GetKernelWorkGroupSize(*kernel); 
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workGroupSize);
	}
}

void PathOCLBaseOCLRenderThread::GetKernelParamters(
		vector<string> &params,
		HardwareIntersectionDevice *intersectionDevice,
		const string renderEngineType,
		const float epsilonMin, const float epsilonMax) {
	params.push_back("-D LUXRAYS_OPENCL_KERNEL");
	params.push_back("-D SLG_OPENCL_KERNEL");
	params.push_back("-D RENDER_ENGINE_" + renderEngineType);
	params.push_back("-D PARAM_RAY_EPSILON_MIN=" + ToString(epsilonMin) + "f");
	params.push_back("-D PARAM_RAY_EPSILON_MAX=" + ToString(epsilonMax) + "f");

	const OpenCLDeviceDescription *oclDeviceDesc = dynamic_cast<const OpenCLDeviceDescription *>(intersectionDevice->GetDeviceDesc());
	if (oclDeviceDesc) {
		if (oclDeviceDesc->IsAMDPlatform())
			params.push_back("-D LUXCORE_AMD_OPENCL");
		else if (oclDeviceDesc->IsNVIDIAPlatform())
			params.push_back("-D LUXCORE_NVIDIA_OPENCL");
		else
			params.push_back("-D LUXCORE_GENERIC_OPENCL");
	}
}

string PathOCLBaseOCLRenderThread::GetKernelSources() {
	// Compile sources
	stringstream ssKernel;
	ssKernel <<
			// OpenCL LuxRays Types
			luxrays::ocl::KernelSource_luxrays_types <<
			luxrays::ocl::KernelSource_randomgen_types <<
			luxrays::ocl::KernelSource_uv_types <<
			luxrays::ocl::KernelSource_point_types <<
			luxrays::ocl::KernelSource_vector_types <<
			luxrays::ocl::KernelSource_normal_types <<
			luxrays::ocl::KernelSource_triangle_types <<
			luxrays::ocl::KernelSource_ray_types <<
			luxrays::ocl::KernelSource_bbox_types <<
			luxrays::ocl::KernelSource_epsilon_types <<
			luxrays::ocl::KernelSource_color_types <<
			luxrays::ocl::KernelSource_frame_types <<
			luxrays::ocl::KernelSource_matrix4x4_types <<
			luxrays::ocl::KernelSource_quaternion_types <<
			luxrays::ocl::KernelSource_transform_types <<
			luxrays::ocl::KernelSource_motionsystem_types <<
			luxrays::ocl::KernelSource_trianglemesh_types <<
			luxrays::ocl::KernelSource_exttrianglemesh_types <<
			// OpenCL LuxRays Funcs
			luxrays::ocl::KernelSource_randomgen_funcs <<
			luxrays::ocl::KernelSource_atomic_funcs <<
			luxrays::ocl::KernelSource_epsilon_funcs <<
			luxrays::ocl::KernelSource_utils_funcs <<
			luxrays::ocl::KernelSource_mc_funcs <<
			luxrays::ocl::KernelSource_vector_funcs <<
			luxrays::ocl::KernelSource_ray_funcs <<
			luxrays::ocl::KernelSource_bbox_funcs <<
			luxrays::ocl::KernelSource_color_funcs <<
			luxrays::ocl::KernelSource_frame_funcs <<
			luxrays::ocl::KernelSource_matrix4x4_funcs <<
			luxrays::ocl::KernelSource_quaternion_funcs <<
			luxrays::ocl::KernelSource_transform_funcs <<
			luxrays::ocl::KernelSource_motionsystem_funcs <<
			luxrays::ocl::KernelSource_triangle_funcs <<
			luxrays::ocl::KernelSource_exttrianglemesh_funcs <<
			// OpenCL SLG Types
			slg::ocl::KernelSource_sceneobject_types <<
			slg::ocl::KernelSource_scene_types <<
			slg::ocl::KernelSource_hitpoint_types <<
			slg::ocl::KernelSource_imagemap_types <<
			slg::ocl::KernelSource_mapping_types <<
			slg::ocl::KernelSource_texture_types <<
			slg::ocl::KernelSource_bsdf_types <<
			slg::ocl::KernelSource_material_types <<
			slg::ocl::KernelSource_volume_types <<
			slg::ocl::KernelSource_sampleresult_types <<
			slg::ocl::KernelSource_film_types <<
			slg::ocl::KernelSource_filter_types <<
			slg::ocl::KernelSource_sampler_types <<
			slg::ocl::KernelSource_camera_types <<
			slg::ocl::KernelSource_light_types <<
			slg::ocl::KernelSource_indexbvh_types <<
			slg::ocl::KernelSource_dlsc_types <<
			slg::ocl::KernelSource_elvc_types <<
			slg::ocl::KernelSource_pgic_types <<
			// OpenCL SLG Funcs
			slg::ocl::KernelSource_mortoncurve_funcs <<
			slg::ocl::KernelSource_evalstack_funcs <<
			slg::ocl::KernelSource_hitpoint_funcs << // Required by mapping funcs
			slg::ocl::KernelSource_mapping_funcs <<
			slg::ocl::KernelSource_imagemap_funcs <<
			slg::ocl::KernelSource_texture_bump_funcs <<
			slg::ocl::KernelSource_texture_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs2 <<
			slg::ocl::KernelSource_texture_blender_funcs <<
			slg::ocl::KernelSource_texture_abs_funcs <<
			slg::ocl::KernelSource_texture_bilerp_funcs <<
			slg::ocl::KernelSource_texture_blackbody_funcs <<
			slg::ocl::KernelSource_texture_brick_funcs <<
			slg::ocl::KernelSource_texture_clamp_funcs <<
			slg::ocl::KernelSource_texture_colordepth_funcs <<
			slg::ocl::KernelSource_texture_densitygrid_funcs <<
			slg::ocl::KernelSource_texture_distort_funcs <<
			slg::ocl::KernelSource_texture_fresnelcolor_funcs <<
			slg::ocl::KernelSource_texture_fresnelconst_funcs <<
			slg::ocl::KernelSource_texture_hitpoint_funcs <<
			slg::ocl::KernelSource_texture_hsv_funcs <<
			slg::ocl::KernelSource_texture_irregulardata_funcs <<
			slg::ocl::KernelSource_texture_triplanar_funcs <<
			slg::ocl::KernelSource_texture_imagemap_funcs <<
			slg::ocl::KernelSource_texture_others_funcs <<
			slg::ocl::KernelSource_texture_random_funcs <<
			slg::ocl::KernelSource_texture_funcs_evalops <<
			slg::ocl::KernelSource_texture_funcs;

	ssKernel <<
			slg::ocl::KernelSource_materialdefs_funcs_generic <<
			slg::ocl::KernelSource_materialdefs_funcs_default <<
			slg::ocl::KernelSource_materialdefs_funcs_thinfilmcoating <<
			slg::ocl::KernelSource_materialdefs_funcs_archglass <<
			slg::ocl::KernelSource_materialdefs_funcs_carpaint <<
			slg::ocl::KernelSource_materialdefs_funcs_clearvol <<
			slg::ocl::KernelSource_materialdefs_funcs_cloth <<
			slg::ocl::KernelSource_materialdefs_funcs_disney <<
			slg::ocl::KernelSource_materialdefs_funcs_glass <<
			slg::ocl::KernelSource_materialdefs_funcs_glossy2 <<
			slg::ocl::KernelSource_materialdefs_funcs_glossycoating <<
			slg::ocl::KernelSource_materialdefs_funcs_glossytranslucent <<
			slg::ocl::KernelSource_materialdefs_funcs_heterogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_homogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_matte <<
			slg::ocl::KernelSource_materialdefs_funcs_matte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_metal2 <<
			slg::ocl::KernelSource_materialdefs_funcs_mirror <<
			slg::ocl::KernelSource_materialdefs_funcs_mix <<
			slg::ocl::KernelSource_materialdefs_funcs_null <<
			slg::ocl::KernelSource_materialdefs_funcs_roughglass <<
			slg::ocl::KernelSource_materialdefs_funcs_roughmatte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_twosided <<
			slg::ocl::KernelSource_materialdefs_funcs_velvet <<
			slg::ocl::KernelSource_material_funcs_evalops <<
			slg::ocl::KernelSource_material_funcs;

	ssKernel <<
			slg::ocl::KernelSource_pathdepthinfo_types <<
			slg::ocl::KernelSource_pathvolumeinfo_types <<
			slg::ocl::KernelSource_pathinfo_types <<
			slg::ocl::KernelSource_pathtracer_types <<
			// PathOCL types
			slg::ocl::KernelSource_pathoclbase_datatypes;

	ssKernel <<
			slg::ocl::KernelSource_bsdfutils_funcs << // Must be before volumeinfo_funcs
			slg::ocl::KernelSource_volume_funcs <<
			slg::ocl::KernelSource_pathdepthinfo_funcs <<
			slg::ocl::KernelSource_pathvolumeinfo_funcs <<
			slg::ocl::KernelSource_pathinfo_funcs <<
			slg::ocl::KernelSource_camera_funcs <<
			slg::ocl::KernelSource_dlsc_funcs <<
			slg::ocl::KernelSource_elvc_funcs <<
			slg::ocl::KernelSource_lightstrategy_funcs <<
			slg::ocl::KernelSource_light_funcs <<
			slg::ocl::KernelSource_filter_funcs <<
			slg::ocl::KernelSource_sampleresult_funcs <<
			slg::ocl::KernelSource_filmdenoiser_funcs <<
			slg::ocl::KernelSource_film_funcs <<
			slg::ocl::KernelSource_varianceclamping_funcs <<
			slg::ocl::KernelSource_sampler_random_funcs <<
			slg::ocl::KernelSource_sampler_sobol_funcs <<
			slg::ocl::KernelSource_sampler_metropolis_funcs <<
			slg::ocl::KernelSource_sampler_tilepath_funcs <<
			slg::ocl::KernelSource_sampler_funcs <<
			slg::ocl::KernelSource_bsdf_funcs <<
			slg::ocl::KernelSource_scene_funcs <<
			slg::ocl::KernelSource_pgic_funcs <<
			// PathOCL Funcs
			slg::ocl::KernelSource_pathoclbase_funcs <<
			slg::ocl::KernelSource_pathoclbase_kernels_micro;

	return ssKernel.str();
}

void PathOCLBaseOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// A safety check
	switch (intersectionDevice->GetAccelerator()->GetType()) {
		case ACCEL_BVH:
			break;
		case ACCEL_MBVH:
			break;
		case ACCEL_EMBREE:
		throw runtime_error("EMBREE accelerator is not supported in PathOCLBaseRenderThread::InitKernels()");
		case ACCEL_OPTIX:
			break;
		default:
			throw runtime_error("Unknown accelerator in PathOCLBaseRenderThread::InitKernels()");
	}

	vector<string> kernelsParameters;
	GetKernelParamters(kernelsParameters, intersectionDevice,
			RenderEngine::RenderEngineType2String(renderEngine->GetType()),
			MachineEpsilon::GetMin(), MachineEpsilon::GetMax());

	const string kernelSource = GetKernelSources();

	if (renderEngine->writeKernelsToFile) {
		// Some debug code to write the OpenCL kernel source to a file
		const string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".cl";
		ofstream kernelFile(kernelFileName.c_str());
		string kernelDefs = oclKernelPersistentCache::ToOptsString(kernelsParameters);
		boost::replace_all(kernelDefs, "-D", "\n#define");
		boost::replace_all(kernelDefs, "=", " ");
		kernelFile << kernelDefs << endl << endl << kernelSource << endl;
		kernelFile.close();
	}

	if ((renderEngine->additionalOpenCLKernelOptions.size() > 0) &&
			(intersectionDevice->GetDeviceDesc()->GetType() & DEVICE_TYPE_OPENCL_ALL))
		kernelsParameters.insert(kernelsParameters.end(), renderEngine->additionalOpenCLKernelOptions.begin(), renderEngine->additionalOpenCLKernelOptions.end());
	if ((renderEngine->additionalCUDAKernelOptions.size() > 0) &&
			(intersectionDevice->GetDeviceDesc()->GetType() & DEVICE_TYPE_CUDA_ALL))
		kernelsParameters.insert(kernelsParameters.end(), renderEngine->additionalCUDAKernelOptions.begin(), renderEngine->additionalCUDAKernelOptions.end());

	// Build the kernel source/parameters hash
	const string newKernelSrcHash = oclKernelPersistentCache::HashString(oclKernelPersistentCache::ToOptsString(kernelsParameters))
			+ "-" +
			oclKernelPersistentCache::HashString(kernelSource);
	if (newKernelSrcHash == kernelSrcHash) {
		// There is no need to re-compile the kernel
		return;
	} else
		kernelSrcHash = newKernelSrcHash;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling kernels ");

	HardwareDeviceProgram *program = nullptr;
	intersectionDevice->CompileProgram(&program, kernelsParameters, kernelSource, "PathOCL kernel");

	// Film clear kernel
	CompileKernel(intersectionDevice, program, &filmClearKernel, &filmClearWorkGroupSize, "Film_Clear");

	// Init kernel

	CompileKernel(intersectionDevice, program, &initSeedKernel, &initWorkGroupSize, "InitSeed");
	CompileKernel(intersectionDevice, program, &initKernel, &initWorkGroupSize, "Init");

	// AdvancePaths kernel (Micro-Kernels)

	size_t workGroupSize;
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_RT_NEXT_VERTEX, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_NEXT_VERTEX");
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_HIT_NOTHING, &workGroupSize,
			"AdvancePaths_MK_HIT_NOTHING");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_HIT_OBJECT, &workGroupSize,
			"AdvancePaths_MK_HIT_OBJECT");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_RT_DL, &workGroupSize,
			"AdvancePaths_MK_RT_DL");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_DL_ILLUMINATE, &workGroupSize,
			"AdvancePaths_MK_DL_ILLUMINATE");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_DL_SAMPLE_BSDF, &workGroupSize,
			"AdvancePaths_MK_DL_SAMPLE_BSDF");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, &workGroupSize,
			"AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_SPLAT_SAMPLE, &workGroupSize,
			"AdvancePaths_MK_SPLAT_SAMPLE");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_NEXT_SAMPLE, &workGroupSize,
			"AdvancePaths_MK_NEXT_SAMPLE");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(intersectionDevice, program, &advancePathsKernel_MK_GENERATE_CAMERA_RAY, &workGroupSize,
			"AdvancePaths_MK_GENERATE_CAMERA_RAY");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] AdvancePaths_MK_* workgroup size: " << advancePathsWorkGroupSize);

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

	delete program;
}

void PathOCLBaseOCLRenderThread::SetInitKernelArgs(const u_int filmIndex) {
	// initSeedKernel kernel
	u_int argIndex = 0;
	intersectionDevice->SetKernelArg(initSeedKernel, argIndex++, tasksBuff);
	intersectionDevice->SetKernelArg(initSeedKernel, argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);

	// initKernel kernel
	argIndex = 0;
	intersectionDevice->SetKernelArg(initKernel, argIndex++, taskConfigBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, tasksBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, tasksDirectLightBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, tasksStateBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, taskStatsBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, samplerSharedDataBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, samplesBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, sampleDataBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, sampleResultsBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, eyePathInfosBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, pixelFilterBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, raysBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, cameraBuff);
	intersectionDevice->SetKernelArg(initKernel, argIndex++, cameraBokehDistributionBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(intersectionDevice, initKernel, argIndex);

	initKernelArgsCount = argIndex;
}

void PathOCLBaseOCLRenderThread::SetAdvancePathsKernelArgs(HardwareDeviceKernel *advancePathsKernel, const u_int filmIndex) {
	CompiledScene *cscene = renderEngine->compiledScene;

	u_int argIndex = 0;
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, taskConfigBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, tasksBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, tasksDirectLightBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, tasksStateBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, taskStatsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, pixelFilterBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, samplerSharedDataBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, samplesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, sampleDataBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, sampleResultsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, eyePathInfosBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, directLightVolInfosBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, raysBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, hitsBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(intersectionDevice, advancePathsKernel, argIndex);

	// Scene parameters
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.center.x);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.center.y);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.center.z);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.rad);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, materialsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, materialEvalOpsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, materialEvalStackBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->maxMaterialEvalStackSize);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, texturesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, textureEvalOpsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, textureEvalStackBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->maxTextureEvalStackSize);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, scnObjsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, meshDescsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, vertsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, normalsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, triNormalsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, uvsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, colsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, alphasBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, vertexAOVBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, triAOVBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, trianglesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, interpolatedTransformsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cameraBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cameraBokehDistributionBuff);
	// Lights
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, lightsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, envLightIndicesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, (u_int)cscene->envLightIndices.size());
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, lightIndexOffsetByMeshIndexBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, lightIndexByTriIndexBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, envLightDistributionsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, lightsDistributionBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, infiniteLightSourcesDistributionBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, dlscAllEntriesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, dlscDistributionsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, dlscBVHNodesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->dlscRadius2);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->dlscNormalCosAngle);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, elvcAllEntriesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, elvcDistributionsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, elvcTileDistributionOffsetsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, elvcBVHNodesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcRadius2);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcNormalCosAngle);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcTilesXCount);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcTilesYCount);

	// Images
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, imageMapDescsBuff);
	for (u_int i = 0; i < 8; ++i) {
		if (i < imageMapsBuff.size())
			intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, imageMapsBuff[i]);
		else
			intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, nullptr);
	}

	// PhotonGI cache
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, pgicRadiancePhotonsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, cscene->pgicLightGroupCounts);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, pgicRadiancePhotonsValuesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, pgicRadiancePhotonsBVHNodesBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, pgicCausticPhotonsBuff);
	intersectionDevice->SetKernelArg(advancePathsKernel, argIndex++, pgicCausticPhotonsBVHNodesBuff);
}

void PathOCLBaseOCLRenderThread::SetAllAdvancePathsKernelArgs(const u_int filmIndex) {
	if (advancePathsKernel_MK_RT_NEXT_VERTEX)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_NEXT_VERTEX, filmIndex);
	if (advancePathsKernel_MK_HIT_NOTHING)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_NOTHING, filmIndex);
	if (advancePathsKernel_MK_HIT_OBJECT)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_OBJECT, filmIndex);
	if (advancePathsKernel_MK_RT_DL)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_DL, filmIndex);
	if (advancePathsKernel_MK_DL_ILLUMINATE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_ILLUMINATE, filmIndex);
	if (advancePathsKernel_MK_DL_SAMPLE_BSDF)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_SAMPLE_BSDF, filmIndex);
	if (advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, filmIndex);
	if (advancePathsKernel_MK_SPLAT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_SPLAT_SAMPLE, filmIndex);
	if (advancePathsKernel_MK_NEXT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_NEXT_SAMPLE, filmIndex);
	if (advancePathsKernel_MK_GENERATE_CAMERA_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_CAMERA_RAY, filmIndex);
}

void PathOCLBaseOCLRenderThread::SetKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only non thread safe function in OpenCL 1.1 so
	// I need to use a mutex here

	boost::unique_lock<boost::mutex> lock(renderEngine->setKernelArgsMutex);

	//--------------------------------------------------------------------------
	// advancePathsKernels
	//--------------------------------------------------------------------------

	SetAllAdvancePathsKernelArgs(0);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	SetInitKernelArgs(0);
}

void PathOCLBaseOCLRenderThread::EnqueueAdvancePathsKernel() {
	const u_int taskCount = renderEngine->taskCount;

	// Micro kernels version
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_RT_NEXT_VERTEX,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_HIT_NOTHING,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_HIT_OBJECT,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_RT_DL,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_DL_ILLUMINATE,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_DL_SAMPLE_BSDF,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_SPLAT_SAMPLE,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_NEXT_SAMPLE,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice->EnqueueKernel(advancePathsKernel_MK_GENERATE_CAMERA_RAY,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
}

#endif
