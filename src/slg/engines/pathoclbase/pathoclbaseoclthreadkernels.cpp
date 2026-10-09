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

#include "luxrays/usings.h"
#include <limits>
#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <mutex>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/devices/ocldevice.h"
#include "luxrays/kernels/kernels.h"

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/kernels/kernelsources.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread kernels related methods
//------------------------------------------------------------------------------


std::tuple<HardwareDeviceKernelUPtr, size_t>
PathOCLBaseOCLRenderThread::CompileKernel(
		HardwareIntersectionDeviceRef device,
		HardwareDeviceProgramRef program,
		const std::string &name
) {
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	size_t workGroupSize;
	auto kernel = device.GetKernel(program, name.c_str());

	if (device.GetDeviceDesc().GetForceWorkGroupSize() > 0) {
		workGroupSize = device.GetDeviceDesc().GetForceWorkGroupSize();
	}
	else {
		workGroupSize = device.GetKernelWorkGroupSize(kernel);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] "
				<< name << " workgroup size: " << workGroupSize);
	}

	return std::make_tuple(std::move(kernel), workGroupSize);
}

void PathOCLBaseOCLRenderThread::GetKernelParamters(
	std::vector<std::string> &params,
	HardwareIntersectionDeviceRef intersectionDevice,
	const string renderEngineType,
	const float epsilonMin, const float epsilonMax
) {
	slg::ocl::GetKernelParamters(params, renderEngineType, epsilonMin, epsilonMax);

	try {
		const auto& oclDeviceDesc =
			dynamic_cast<OpenCLDeviceDescriptionConstRef>(intersectionDevice.GetDeviceDesc());
		if (oclDeviceDesc.IsAMDPlatform())
			params.push_back("-D LUXCORE_AMD_OPENCL");
		else if (oclDeviceDesc.IsNVIDIAPlatform())
			params.push_back("-D LUXCORE_NVIDIA_OPENCL");
		else
			params.push_back("-D LUXCORE_GENERIC_OPENCL");
	}
	catch (std::bad_cast&) {}
}

string PathOCLBaseOCLRenderThread::GetKernelSources() {
	return slg::ocl::GetPathOCLBaseKernelSources();
}

void PathOCLBaseOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// A safety check
	switch (intersectionDevice.GetAccelerator()->GetType()) {
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
	const string microKernelSource = slg::ocl::KernelSource_pathoclbase_kernels_micro;
	// The full source (i.e. including micro-kernels) is used for the hash and
	// the debug dump
	const string fullKernelSource = kernelSource + microKernelSource;

	if (renderEngine->writeKernelsToFile) {
		// Some debug code to write the OpenCL kernel source to a file
		const string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".cl";
		ofstream kernelFile(kernelFileName.c_str());
		string kernelDefs = oclKernelPersistentCache::ToOptsString(kernelsParameters);
		boost::replace_all(kernelDefs, "-D", "\n#define");
		boost::replace_all(kernelDefs, "=", " ");
		kernelFile << kernelDefs << endl << endl << fullKernelSource << endl;
		kernelFile.close();
	}

	if ((renderEngine->additionalOpenCLKernelOptions.size() > 0) &&
			(intersectionDevice.GetDeviceDesc().GetType() & DEVICE_TYPE_OPENCL_ALL))
		kernelsParameters.insert(kernelsParameters.end(), renderEngine->additionalOpenCLKernelOptions.begin(), renderEngine->additionalOpenCLKernelOptions.end());
	if ((renderEngine->additionalCUDAKernelOptions.size() > 0) &&
			(intersectionDevice.GetDeviceDesc().GetType() & DEVICE_TYPE_CUDA_ALL))
		kernelsParameters.insert(kernelsParameters.end(), renderEngine->additionalCUDAKernelOptions.begin(), renderEngine->additionalCUDAKernelOptions.end());

	// Build the kernel source/parameters hash
	const string newKernelSrcHash = oclKernelPersistentCache::HashString(oclKernelPersistentCache::ToOptsString(kernelsParameters))
			+ "-" +
			oclKernelPersistentCache::HashString(fullKernelSource);
	if (newKernelSrcHash == kernelSrcHash) {
		// There is no need to re-compile the kernel
		return;
	} else
		kernelSrcHash = newKernelSrcHash;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling kernels ");

	// On OpenCL devices, split the micro-kernels in single-kernel programs so
	// that they can be compiled in parallel by the driver. It is worth because
	// drivers running the compiler on the host (i.e. AMD, Intel) can compile
	// multiple programs concurrently. NVIDIA users are expected to use the
	// CUDA path (where the monolithic source is kept: nvrtc is fast and the
	// driver serializes concurrent builds anyway).
	//
	// The feature can be disabled with opencl.splitkernels.enable = 0 (e.g. in
	// case of a driver deadlocking on concurrent builds).
	//
	// If there is an embedded pre-compiled SPIR-V module for the monolithic
	// source, the split is not used: all the kernels are loaded from the one
	// program obtained by translating the SPIR-V module (there is nothing
	// left to compile in parallel).
	const bool isCUDADevice = (intersectionDevice.GetDeviceDesc().GetType() & DEVICE_TYPE_CUDA_ALL) != 0;
	const bool hasEmbeddedSPIRV = intersectionDevice.HasEmbeddedSPIRV(kernelsParameters, fullKernelSource);
	const bool splitKernels = !isCUDADevice && !hasEmbeddedSPIRV &&
			renderEngine->renderConfig.GetConfig().Get(
				Property("opencl.splitkernels.enable")(true)).Get<bool>();

	std::vector<HardwareDevice::ProgramRequest> requests;
	std::unordered_map<std::string, std::size_t> microKernelIndex;

	if (splitKernels) {
		const auto microKernelSources = ocl::SplitMicroKernelSources(microKernelSource);

		for (std::size_t i = 0; i < microKernelSources.size(); ++i)
			microKernelIndex[microKernelSources[i].first] = i;

		// The first request is the program with the regular kernels (i.e.
		// Film_Clear, InitSeed, Init, etc.), the following ones are one program
		// per micro-kernel. Each micro-kernel program includes the same base
		// source (i.e. all shared functions and types) as the first program.
		requests.reserve(1 + microKernelSources.size());
		requests.push_back({ kernelsParameters, kernelSource, "PathOCL kernel" });
		for (const auto &[name, source] : microKernelSources)
			requests.push_back({ kernelsParameters, kernelSource + source, "PathOCL kernel " + name });
	} else
		requests.push_back({ kernelsParameters, fullKernelSource, "PathOCL kernel" });

	auto programs = intersectionDevice.CompilePrograms(requests);

	std::tuple<HardwareDeviceKernelUPtr&, size_t&, const char *>
	kernels[] = {
		{filmClearKernel, filmClearWorkGroupSize, "Film_Clear"},
		{initSeedKernel, initWorkGroupSize, "InitSeed"},
		{initKernel, initWorkGroupSize, "Init"},
	};

	for (auto& [kernel, workGroupSize, name] : kernels) {
		std::tie(kernel, workGroupSize) = CompileKernel(intersectionDevice, *programs[0], name);
	}


	// AdvancePaths kernel (Micro-Kernels)
	std::tuple<HardwareDeviceKernelUPtr&, const char *>
	microKernels[] = {
		{advancePathsKernel_MK_RT_NEXT_VERTEX, "AdvancePaths_MK_RT_NEXT_VERTEX"},
		{advancePathsKernel_MK_HIT_NOTHING, "AdvancePaths_MK_HIT_NOTHING"},
		{advancePathsKernel_MK_HIT_OBJECT, "AdvancePaths_MK_HIT_OBJECT"},
		{advancePathsKernel_MK_RT_DL, "AdvancePaths_MK_RT_DL"},
		{advancePathsKernel_MK_DL_ILLUMINATE, "AdvancePaths_MK_DL_ILLUMINATE"},
		{advancePathsKernel_MK_DL_SAMPLE_BSDF, "AdvancePaths_MK_DL_SAMPLE_BSDF"},
		{advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, "AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY"},
		{advancePathsKernel_MK_SPLAT_SAMPLE, "AdvancePaths_MK_SPLAT_SAMPLE"},
		{advancePathsKernel_MK_NEXT_SAMPLE, "AdvancePaths_MK_NEXT_SAMPLE"},
		{advancePathsKernel_MK_GENERATE_CAMERA_RAY, "AdvancePaths_MK_GENERATE_CAMERA_RAY"},
	};

	advancePathsWorkGroupSize = std::numeric_limits<size_t>::max();

	for (auto& [microKernel, name] : microKernels) {
		// Find the program with this micro-kernel (all the micro-kernels are
		// in the same program unless the source has been split)
		const std::size_t programIndex = microKernelIndex.empty() ? 0 : 1 + microKernelIndex[name];

		// Compile kernel
		auto [kernel, workGroupSize] = CompileKernel(intersectionDevice, *programs[programIndex], name);

		// Assign to class members
		microKernel = std::move(kernel);
		advancePathsWorkGroupSize = std::min(advancePathsWorkGroupSize, workGroupSize);
	}

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex
			<< "] AdvancePaths_MK_* workgroup size: "
			<< advancePathsWorkGroupSize
	);

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

}

void PathOCLBaseOCLRenderThread::SetInitKernelArgs(const u_int filmIndex) {
	// initSeedKernel kernel
	u_int argIndex = 0;
	intersectionDevice.SetKernelArg(initSeedKernel, argIndex++, tasksBuff);
	intersectionDevice.SetKernelArg(initSeedKernel, argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);

	// initKernel kernel
	argIndex = 0;
	intersectionDevice.SetKernelArg(initKernel, argIndex++, taskConfigBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, tasksBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, tasksDirectLightBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, tasksStateBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, taskStatsBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, samplerSharedDataBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, samplesBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, sampleDataBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, sampleResultsBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, eyePathInfosBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, pixelFilterBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, raysBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, cameraBuff);
	intersectionDevice.SetKernelArg(initKernel, argIndex++, cameraBokehDistributionBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(intersectionDevice, initKernel, argIndex);

	initKernelArgsCount = argIndex;
}

void PathOCLBaseOCLRenderThread::SetAdvancePathsKernelArgs(
	HardwareDeviceKernelRPtr advancePathsKernel, const u_int filmIndex
) {
	CompiledScene *cscene = renderEngine->compiledScene;

	u_int argIndex = 0;
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, taskConfigBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, tasksBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, tasksDirectLightBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, tasksStateBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, taskStatsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, pixelFilterBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, samplerSharedDataBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, samplesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, sampleDataBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, sampleResultsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, eyePathInfosBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, directLightVolInfosBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, raysBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, hitsBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(intersectionDevice, advancePathsKernel, argIndex);

	// Scene parameters
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.center.x);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.center.y);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.center.z);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->worldBSphere.rad);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, materialsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, materialEvalOpsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, materialEvalStackBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->maxMaterialEvalStackSize);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, texturesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, textureEvalOpsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, textureEvalStackBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->maxTextureEvalStackSize);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, scnObjsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, meshDescsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, vertsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, normalsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, triNormalsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, uvsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, colsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, alphasBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, vertexAOVBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, triAOVBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, trianglesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, interpolatedTransformsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cameraBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cameraBokehDistributionBuff);
	// Lights
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, lightsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, envLightIndicesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, (u_int)cscene->envLightIndices.size());
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, lightIndexOffsetByMeshIndexBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, lightIndexByTriIndexBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, envLightDistributionsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, lightsDistributionBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, infiniteLightSourcesDistributionBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, dlscAllEntriesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, dlscDistributionsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, dlscBVHNodesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->dlscRadius2);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->dlscNormalCosAngle);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, elvcAllEntriesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, elvcDistributionsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, elvcTileDistributionOffsetsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, elvcBVHNodesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcRadius2);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcNormalCosAngle);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcTilesXCount);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->elvcTilesYCount);

	// Images
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, imageMapDescsBuff);
	for (u_int i = 0; i < 8; ++i) {
		if (i < imageMapsBuff.size())
			intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, imageMapsBuff[i]);
		else
			intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, nullptr);
	}

	// PhotonGI cache
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, pgicRadiancePhotonsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, cscene->pgicLightGroupCounts);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, pgicRadiancePhotonsValuesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, pgicRadiancePhotonsBVHNodesBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, pgicCausticPhotonsBuff);
	intersectionDevice.SetKernelArg(advancePathsKernel, argIndex++, pgicCausticPhotonsBVHNodesBuff);
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

	std::unique_lock<std::mutex> lock(renderEngine->setKernelArgsMutex);

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
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_RT_NEXT_VERTEX,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_HIT_NOTHING,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_HIT_OBJECT,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_RT_DL,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_DL_ILLUMINATE,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_DL_SAMPLE_BSDF,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_SPLAT_SAMPLE,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_NEXT_SAMPLE,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
	intersectionDevice.EnqueueKernel(advancePathsKernel_MK_GENERATE_CAMERA_RAY,
			HardwareDeviceRange(taskCount), HardwareDeviceRange(advancePathsWorkGroupSize));
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
