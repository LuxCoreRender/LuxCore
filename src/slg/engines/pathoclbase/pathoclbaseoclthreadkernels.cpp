/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"

#if defined(__APPLE__)
//OSX version detection
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

	if (intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
		*workgroupSize = intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize();
	else {
		cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
		(*kernel)->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, workgroupSize);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workgroupSize);
	}
}

void PathOCLBaseOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	// Set #define symbols
	stringstream ssParams;
	ssParams.precision(6);
	ssParams << scientific <<
			" -D LUXRAYS_OPENCL_KERNEL" <<
			" -D SLG_OPENCL_KERNEL" <<
			" -D RENDER_ENGINE_" << RenderEngine::RenderEngineType2String(renderEngine->GetType()) <<
			" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
			" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f"
			;

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

	if (renderEngine->usePixelAtomics)
		ssParams << " -D PARAM_USE_PIXEL_ATOMICS";

	//--------------------------------------------------------------------------
	// Materials
	//--------------------------------------------------------------------------

	if (cscene->IsMaterialCompiled(MATTE))
		ssParams << " -D PARAM_ENABLE_MAT_MATTE";
	if (cscene->IsMaterialCompiled(ROUGHMATTE))
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHMATTE";
	if (cscene->IsMaterialCompiled(VELVET))
		ssParams << " -D PARAM_ENABLE_MAT_VELVET";
	if (cscene->IsMaterialCompiled(MIRROR))
		ssParams << " -D PARAM_ENABLE_MAT_MIRROR";
	if (cscene->IsMaterialCompiled(GLASS))
		ssParams << " -D PARAM_ENABLE_MAT_GLASS";
	if (cscene->IsMaterialCompiled(ARCHGLASS))
		ssParams << " -D PARAM_ENABLE_MAT_ARCHGLASS";
	if (cscene->IsMaterialCompiled(MIX))
		ssParams << " -D PARAM_ENABLE_MAT_MIX";
	if (cscene->IsMaterialCompiled(NULLMAT))
		ssParams << " -D PARAM_ENABLE_MAT_NULL";
	if (cscene->IsMaterialCompiled(MATTETRANSLUCENT))
		ssParams << " -D PARAM_ENABLE_MAT_MATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(ROUGHMATTETRANSLUCENT))
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(GLOSSY2)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2";

		if (cscene->IsMaterialCompiled(GLOSSY2_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSY2_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSY2_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSY2_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(METAL2)) {
		ssParams << " -D PARAM_ENABLE_MAT_METAL2";
		if (cscene->IsMaterialCompiled(METAL2_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_METAL2_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(ROUGHGLASS)) {
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHGLASS";
		if (cscene->IsMaterialCompiled(ROUGHGLASS_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(CLOTH))
		ssParams << " -D PARAM_ENABLE_MAT_CLOTH";
	if (cscene->IsMaterialCompiled(CARPAINT))
		ssParams << " -D PARAM_ENABLE_MAT_CARPAINT";
	if (cscene->IsMaterialCompiled(CLEAR_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_CLEAR_VOL";
	if (cscene->IsMaterialCompiled(HOMOGENEOUS_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_HOMOGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(HETEROGENEOUS_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_HETEROGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT";

		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(GLOSSYCOATING)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING";

		if (cscene->IsMaterialCompiled(GLOSSYCOATING_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(DISNEY))
		ssParams << " -D PARAM_ENABLE_MAT_DISNEY";

	//--------------------------------------------------------------------------

	// Check the OpenCL vendor and use some specific compiler options

#if defined(__APPLE__)
	// Starting with 10.10 (darwin 14.x.x), opencl mix() function is fixed
	char darwin_ver[10];
	size_t len = sizeof(darwin_ver);
	sysctlbyname("kern.osrelease", &darwin_ver, &len, NULL, 0);
	if(darwin_ver[0] == '1' && darwin_ver[1] < '4') {
		ssParams << " -D __APPLE_CL__";
	}
#else
        if (intersectionDevice->GetDeviceDesc()->IsAMDPlatform())
            ssParams << " -D LUXCORE_AMD_OPENCL";
        else if (intersectionDevice->GetDeviceDesc()->IsNVIDIAPlatform())
            ssParams << " -D LUXCORE_NVIDIA_OPENCL";
        else
            ssParams << " -D LUXCORE_GENERIC_OPENCL";
#endif

	//--------------------------------------------------------------------------

	// This is a workaround to NVIDIA long compilation time
	string forceInlineDirective;
	if (intersectionDevice->GetDeviceDesc()->IsNVIDIAPlatform()) {
		//SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] NVIDIA platform: using inline workaround");
		forceInlineDirective =
				"#define OPENCL_FORCE_NOT_INLINE __attribute__((noinline))\n"
				"#define OPENCL_FORCE_INLINE __attribute__((always_inline))\n";
	} else {
		//SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Not NVIDIA platform: not using inline workaround");
		forceInlineDirective =
				"#define OPENCL_FORCE_NOT_INLINE\n"
				"#define OPENCL_FORCE_INLINE\n";
	}

	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	kernelsParameters = ssParams.str();
	// This is a workaround for an Apple OpenCL by Arve Nygard. The double space
	// causes clBuildProgram() to fail with CL_INVALID_BUILD_OPTIONS on OSX.
	boost::replace_all(kernelsParameters, "  ", " ");

	// Compile sources
	stringstream ssKernel;
	ssKernel <<
			forceInlineDirective <<
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
			slg::ocl::KernelSource_mapping_funcs <<
			slg::ocl::KernelSource_imagemap_funcs <<
			slg::ocl::KernelSource_texture_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs2 <<
			slg::ocl::KernelSource_texture_blender_funcs <<
			slg::ocl::KernelSource_texture_abs_funcs <<
			slg::ocl::KernelSource_texture_bilerp_funcs <<
			slg::ocl::KernelSource_texture_blackbody_funcs <<
			slg::ocl::KernelSource_texture_clamp_funcs <<
			slg::ocl::KernelSource_texture_colordepth_funcs <<
			slg::ocl::KernelSource_texture_densitygrid_funcs <<
			slg::ocl::KernelSource_texture_fresnelcolor_funcs <<
			slg::ocl::KernelSource_texture_fresnelconst_funcs <<
			slg::ocl::KernelSource_texture_hsv_funcs <<
			slg::ocl::KernelSource_texture_irregulardata_funcs <<
			slg::ocl::KernelSource_texture_funcs <<
			slg::ocl::KernelSource_texture_eval_funcs;

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
			slg::ocl::KernelSource_materialdefs_funcs_glossytranslucent <<
			slg::ocl::KernelSource_materialdefs_funcs_heterogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_homogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_matte <<
			slg::ocl::KernelSource_materialdefs_funcs_matte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_metal2 <<
			slg::ocl::KernelSource_materialdefs_funcs_mirror <<
			slg::ocl::KernelSource_materialdefs_funcs_null <<
			slg::ocl::KernelSource_materialdefs_funcs_roughglass <<
			slg::ocl::KernelSource_materialdefs_funcs_roughmatte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_velvet <<
			slg::ocl::KernelSource_material_main_withoutdynamic;

	// Generate the code to evaluate the materials
	ssKernel <<
			// This is the dynamic generated code (aka "WithDynamic")
			"#line 2 \"Material evaluation code form CompiledScene::GetMaterialsEvaluationSourceCode()\"\n" <<
			cscene->GetMaterialsEvaluationSourceCode() <<
			"\n" <<
			slg::ocl::KernelSource_material_main;

	ssKernel <<
			slg::ocl::KernelSource_pathdepthinfo_types <<
			slg::ocl::KernelSource_pathvolumeinfo_types <<
			slg::ocl::KernelSource_pathinfo_types <<
			slg::ocl::KernelSource_pathtracer_types <<
			// PathOCL types
			slg::ocl::KernelSource_pathoclbase_datatypes;

	ssKernel <<
			slg::ocl::KernelSource_hitpoint_funcs <<
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

	string kernelSource = ssKernel.str();

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

	CompileKernel(program, &advancePathsKernel_MK_RT_NEXT_VERTEX, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_NEXT_VERTEX");
	CompileKernel(program, &advancePathsKernel_MK_HIT_NOTHING, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_HIT_NOTHING");
	CompileKernel(program, &advancePathsKernel_MK_HIT_OBJECT, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_HIT_OBJECT");
	CompileKernel(program, &advancePathsKernel_MK_RT_DL, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_DL");
	CompileKernel(program, &advancePathsKernel_MK_DL_ILLUMINATE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_DL_ILLUMINATE");
	CompileKernel(program, &advancePathsKernel_MK_DL_SAMPLE_BSDF, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_DL_SAMPLE_BSDF");
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY");
	CompileKernel(program, &advancePathsKernel_MK_SPLAT_SAMPLE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_SPLAT_SAMPLE");
	CompileKernel(program, &advancePathsKernel_MK_NEXT_SAMPLE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_NEXT_SAMPLE");
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_CAMERA_RAY, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_GENERATE_CAMERA_RAY");

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

	delete program;
}

void PathOCLBaseOCLRenderThread::SetInitKernelArgs(const u_int filmIndex) {
	// initSeedKernel kernel
	u_int argIndex = 0;
	initSeedKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	initSeedKernel->setArg(argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);

	// initKernel kernel
	argIndex = 0;
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), taskConfigBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksDirectLightBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksStateBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), taskStatsBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), samplerSharedDataBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), samplesBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleDataBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleResultsBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), eyePathInfosBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), pixelFilterBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), raysBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), cameraBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*initKernel, argIndex);

	initKernelArgsCount = argIndex;
}

void PathOCLBaseOCLRenderThread::SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel, const u_int filmIndex) {
	CompiledScene *cscene = renderEngine->compiledScene;

	u_int argIndex = 0;
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), taskConfigBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksDirectLightBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksStateBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), taskStatsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pixelFilterBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), samplerSharedDataBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), samplesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleDataBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleResultsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), eyePathInfosBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), directLightVolInfosBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), raysBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), hitsBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*advancePathsKernel, argIndex);

	// Scene parameters
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), materialsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), texturesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), textureEvalOpsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), textureEvalStackBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), scnObjsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), meshDescsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), vertsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), normalsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), triNormalsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), uvsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), colsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), alphasBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), trianglesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), interpolatedTransformsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), cameraBuff);
	// Lights
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), envLightIndicesBuff);
	advancePathsKernel->setArg(argIndex++, (u_int)cscene->envLightIndices.size());
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightIndexOffsetByMeshIndexBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightIndexByTriIndexBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), envLightDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightsDistributionBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), infiniteLightSourcesDistributionBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), dlscAllEntriesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), dlscDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), dlscBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, cscene->dlscRadius2);
	advancePathsKernel->setArg(argIndex++, cscene->dlscNormalCosAngle);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), elvcAllEntriesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), elvcDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), elvcBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, cscene->elvcRadius2);
	advancePathsKernel->setArg(argIndex++, cscene->elvcNormalCosAngle);

	// Images
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), imageMapDescsBuff);
	for (u_int i = 0; i < 8; ++i) {
		if (i < imageMapsBuff.size())
			advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), imageMapsBuff[i]);
		else
			advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), nullptr);
	}

	// PhotonGI cache
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicRadiancePhotonsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicRadiancePhotonsBVHNodesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicCausticPhotonsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pgicCausticPhotonsBVHNodesBuff);
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
