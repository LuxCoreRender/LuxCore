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

#if defined(__APPLE__)
// OSX version detection
#include <sys/sysctl.h>
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread kernels related methods
//------------------------------------------------------------------------------

void PathOCLBaseOCLRenderThread::CompileKernel(cl::Program *program, cl::Kernel **kernel,
		size_t *workgroupSize, const string &name) {
	delete *kernel;
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	*kernel = new cl::Kernel(*program, name.c_str());

	const OpenCLDeviceDescription *oclDeviceDesc = (const OpenCLDeviceDescription *)intersectionDevice->GetDeviceDesc();
	if (oclDeviceDesc->GetForceWorkGroupSize() > 0)
		*workgroupSize = oclDeviceDesc->GetForceWorkGroupSize();
	else {
		cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
		(*kernel)->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, workgroupSize);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workgroupSize);
	}
}

string PathOCLBaseOCLRenderThread::GetKernelParamters(
		OpenCLIntersectionDevice *intersectionDevice,
		const string renderEngineType,
		const float epsilonMin, const float epsilonMax,
		const bool usePixelAtomics) {
	// Set #define symbols
	stringstream ssParams;
	ssParams.precision(6);
	ssParams << scientific <<
			" -D LUXRAYS_OPENCL_KERNEL" <<
			" -D SLG_OPENCL_KERNEL" <<
			" -D RENDER_ENGINE_" << renderEngineType <<
			" -D PARAM_RAY_EPSILON_MIN=" << epsilonMin << "f"
			" -D PARAM_RAY_EPSILON_MAX=" << epsilonMax << "f"
			;

	if (usePixelAtomics)
		ssParams << " -D PARAM_USE_PIXEL_ATOMICS";

	const OpenCLDeviceDescription *oclDeviceDesc = dynamic_cast<const OpenCLDeviceDescription *>(intersectionDevice->GetDeviceDesc());
	if (oclDeviceDesc) {
		if (oclDeviceDesc->IsAMDPlatform())
			ssParams << " -D LUXCORE_AMD_OPENCL";
		else if (oclDeviceDesc->IsNVIDIAPlatform())
			ssParams << " -D LUXCORE_NVIDIA_OPENCL";
		else
			ssParams << " -D LUXCORE_GENERIC_OPENCL";
	}

	return ssParams.str();
}

string PathOCLBaseOCLRenderThread::GetKernelSources() {
	// Compile sources
	stringstream ssKernel;
	ssKernel <<
			luxrays::ocl::KernelSource_ocldevice_funcs +
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
			slg::ocl::KernelSource_texture_fresnelcolor_funcs <<
			slg::ocl::KernelSource_texture_fresnelconst_funcs <<
			slg::ocl::KernelSource_texture_hitpoint_funcs <<
			slg::ocl::KernelSource_texture_hsv_funcs <<
			slg::ocl::KernelSource_texture_irregulardata_funcs <<
			slg::ocl::KernelSource_texture_triplanar_funcs <<
			slg::ocl::KernelSource_texture_others_funcs <<
			slg::ocl::KernelSource_texture_random_funcs <<
			slg::ocl::KernelSource_texture_funcs_evalops <<
			slg::ocl::KernelSource_texture_funcs;

	ssKernel <<
			slg::ocl::KernelSource_materialdefs_funcs_generic <<
			slg::ocl::KernelSource_materialdefs_funcs_default <<
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
	
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	// A safety check
	switch (intersectionDevice->GetAccelerator()->GetType()) {
		case ACCEL_BVH:
			break;
		case ACCEL_MBVH:
			break;
		case ACCEL_EMBREE:
			throw runtime_error("EMBREE accelerator is not supported in PathOCLBaseRenderThread::InitKernels()");
		default:
			throw runtime_error("Unknown accelerator in PathOCLBaseRenderThread::InitKernels()");
	}

	kernelsParameters = GetKernelParamters(intersectionDevice,
			RenderEngine::RenderEngineType2String(renderEngine->GetType()),
			MachineEpsilon::GetMin(), MachineEpsilon::GetMax(),
			renderEngine->usePixelAtomics);

	const string kernelSource = GetKernelSources();

	// Build the kernel source/parameters hash
	const string newKernelSrcHash = oclKernelPersistentCache::HashString(kernelsParameters) + "-" +
			oclKernelPersistentCache::HashString(kernelSource);
	if (newKernelSrcHash == kernelSrcHash) {
		// There is no need to re-compile the kernel
		return;
	} else
		kernelSrcHash = newKernelSrcHash;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling kernels ");

	if (renderEngine->writeKernelsToFile) {
		// Some debug code to write the OpenCL kernel source to a file
		const string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".cl";
		ofstream kernelFile(kernelFileName.c_str());
		string kernelDefs = kernelsParameters;
		boost::replace_all(kernelDefs, "-D", "\n#define");
		boost::replace_all(kernelDefs, "=", " ");
		kernelFile << kernelDefs << endl << endl << kernelSource << endl;
		kernelFile.close();
	}

	bool cached;
	cl::STRING_CLASS error;
	cl::Program *program = kernelCache->Compile(oclContext, oclDevice,
			kernelsParameters, kernelSource,
			&cached, &error);

	if (!program) {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] PathOCL kernel compilation error" << endl << error);

		throw runtime_error("PathOCLBase kernel compilation error:\n" + error);
	}

	if (cached) {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels cached");
	} else {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels not cached");
	}

	// Film clear kernel
	CompileKernel(program, &filmClearKernel, &filmClearWorkGroupSize, "Film_Clear");

	// Init kernel

	CompileKernel(program, &initSeedKernel, &initWorkGroupSize, "InitSeed");
	CompileKernel(program, &initKernel, &initWorkGroupSize, "Init");

	// AdvancePaths kernel (Micro-Kernels)

	size_t workGroupSize;
	CompileKernel(program, &advancePathsKernel_MK_RT_NEXT_VERTEX, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_NEXT_VERTEX");
	CompileKernel(program, &advancePathsKernel_MK_HIT_NOTHING, &workGroupSize,
			"AdvancePaths_MK_HIT_NOTHING");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_HIT_OBJECT, &workGroupSize,
			"AdvancePaths_MK_HIT_OBJECT");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_RT_DL, &workGroupSize,
			"AdvancePaths_MK_RT_DL");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_DL_ILLUMINATE, &workGroupSize,
			"AdvancePaths_MK_DL_ILLUMINATE");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_DL_SAMPLE_BSDF, &workGroupSize,
			"AdvancePaths_MK_DL_SAMPLE_BSDF");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, &workGroupSize,
			"AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_SPLAT_SAMPLE, &workGroupSize,
			"AdvancePaths_MK_SPLAT_SAMPLE");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_NEXT_SAMPLE, &workGroupSize,
			"AdvancePaths_MK_NEXT_SAMPLE");
	advancePathsWorkGroupSize = Min(advancePathsWorkGroupSize, workGroupSize);
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_CAMERA_RAY, &workGroupSize,
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
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initSeedKernel, argIndex++, tasksBuff);
	initSeedKernel->setArg(argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);

	// initKernel kernel
	argIndex = 0;
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, taskConfigBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, tasksBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, tasksDirectLightBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, tasksStateBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, taskStatsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, samplerSharedDataBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, samplesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, sampleDataBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, sampleResultsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, eyePathInfosBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, pixelFilterBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, raysBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*initKernel, argIndex++, cameraBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*initKernel, argIndex);

	initKernelArgsCount = argIndex;
}

void PathOCLBaseOCLRenderThread::SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel, const u_int filmIndex) {
	CompiledScene *cscene = renderEngine->compiledScene;

	u_int argIndex = 0;
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, taskConfigBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, tasksBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, tasksDirectLightBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, tasksStateBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, taskStatsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, pixelFilterBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, samplerSharedDataBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, samplesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, sampleDataBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, sampleResultsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, eyePathInfosBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, directLightVolInfosBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, raysBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, hitsBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*advancePathsKernel, argIndex);

	// Scene parameters
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, materialsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, materialEvalOpsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, materialEvalStackBuff);
	advancePathsKernel->setArg(argIndex++, cscene->maxMaterialEvalStackSize);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, texturesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, textureEvalOpsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, textureEvalStackBuff);
	advancePathsKernel->setArg(argIndex++, cscene->maxTextureEvalStackSize);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, scnObjsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, meshDescsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, vertsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, normalsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, triNormalsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, uvsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, colsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, alphasBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, vertexAOVBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, triAOVBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, trianglesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, interpolatedTransformsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, cameraBuff);
	// Lights
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, lightsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, envLightIndicesBuff);
	advancePathsKernel->setArg(argIndex++, (u_int)cscene->envLightIndices.size());
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, lightIndexOffsetByMeshIndexBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, lightIndexByTriIndexBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, envLightDistributionsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, lightsDistributionBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, infiniteLightSourcesDistributionBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, dlscAllEntriesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, dlscDistributionsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, dlscBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, cscene->dlscRadius2);
	advancePathsKernel->setArg(argIndex++, cscene->dlscNormalCosAngle);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, elvcAllEntriesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, elvcDistributionsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, elvcTileDistributionOffsetsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, elvcBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, cscene->elvcRadius2);
	advancePathsKernel->setArg(argIndex++, cscene->elvcNormalCosAngle);
	advancePathsKernel->setArg(argIndex++, cscene->elvcTilesXCount);
	advancePathsKernel->setArg(argIndex++, cscene->elvcTilesYCount);

	// Images
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, imageMapDescsBuff);
	for (u_int i = 0; i < 8; ++i) {
		if (i < imageMapsBuff.size())
			OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, imageMapsBuff[i]);
		else
			OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, nullptr);
	}

	// PhotonGI cache
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, pgicRadiancePhotonsBuff);
	advancePathsKernel->setArg(argIndex++, cscene->pgicLightGroupCounts);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, pgicRadiancePhotonsValuesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, pgicRadiancePhotonsBVHNodesBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, pgicCausticPhotonsBuff);
	OpenCLDeviceBuffer::tmpSetKernelkArg(*advancePathsKernel, argIndex++, pgicCausticPhotonsBVHNodesBuff);
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

void PathOCLBaseOCLRenderThread::EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue) {
	const u_int taskCount = renderEngine->taskCount;

	// Micro kernels version
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_NEXT_VERTEX, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_NOTHING, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_OBJECT, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_DL, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_ILLUMINATE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_SAMPLE_BSDF, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_SPLAT_SAMPLE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_NEXT_SAMPLE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_CAMERA_RAY, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
}

#endif
