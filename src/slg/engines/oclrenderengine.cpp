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

#include "slg/engines/oclrenderengine.h"

#include "luxrays/core/intersectiondevice.h"
#if !defined(LUXRAYS_DISABLE_OPENCL)
#include "luxrays/devices/ocldevice.h"
#endif

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <Windows.h>
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// OCLRenderEngine
//------------------------------------------------------------------------------

OCLRenderEngine::OCLRenderEngine(const RenderConfig *rcfg,
		const bool supportsNativeThreads) : RenderEngine(rcfg) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	const Properties &cfg = renderConfig->cfg;

	const bool useCPUs = cfg.Get(GetDefaultProps().Get("opencl.cpu.use")).Get<bool>();
	const bool useGPUs = cfg.Get(GetDefaultProps().Get("opencl.gpu.use")).Get<bool>();

	// 0 means use the value suggested by the OpenCL driver
	const u_int forceCPUWorkSize = cfg.Get(GetDefaultProps().Get("opencl.cpu.workgroup.size")).Get<u_int>();
	// 0 means use the value suggested by the OpenCL driver
	// Note: I'm using 32 because some driver (i.e. NVIDIA) suggests a value and than
	// throws a clEnqueueNDRangeKernel(CL_OUT_OF_RESOURCES)
	const u_int forceGPUWorkSize = cfg.Get(GetDefaultProps().Get("opencl.gpu.workgroup.size")).Get<u_int>();

	const string oclDeviceConfig = cfg.Get(GetDefaultProps().Get("opencl.devices.select")).Get<string>();

	const bool useOutOfCoreMemory = cfg.Get(Property("opencl.outofcore.enable")(false)).Get<bool>();
	ctx->SetUseOutOfCoreBuffers(useOutOfCoreMemory);

	//--------------------------------------------------------------------------
	// Get OpenCL device descriptions
	//--------------------------------------------------------------------------

	vector<DeviceDescription *> oclDescs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_OPENCL_ALL, oclDescs);

	vector<DeviceDescription *> cudaDescs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_CUDA_ALL, cudaDescs);

	vector<DeviceDescription *> descs;
	descs.insert(descs.end(), oclDescs.begin(), oclDescs.end());
	descs.insert(descs.end(), cudaDescs.begin(), cudaDescs.end());

	// Device info
	bool haveSelectionString = (oclDeviceConfig.length() > 0);
	if (haveSelectionString && (oclDeviceConfig.length() != descs.size())) {
		stringstream ss;
		ss << "Hardware device selection string has the wrong length, must be " <<
				descs.size() << " instead of " << oclDeviceConfig.length();
		throw runtime_error(ss.str().c_str());
	}

	bool hasCUDADevice = false;
	for (size_t i = 0; i < descs.size(); ++i) {
		DeviceDescription *desc = descs[i];

		bool selected = false;
		if (haveSelectionString) {
			if (oclDeviceConfig.at(i) == '1') {
				if (desc->GetType() & (DEVICE_TYPE_OPENCL_GPU | DEVICE_TYPE_CUDA_GPU))
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				else if (desc->GetType() & DEVICE_TYPE_OPENCL_CPU)
					desc->SetForceWorkGroupSize(forceCPUWorkSize);

				selectedDeviceDescs.push_back(desc);
				selected = true;
			}
		} else {
			if ((useCPUs && (desc->GetType() & DEVICE_TYPE_OPENCL_CPU)) ||
					(useGPUs && desc->GetType() & (DEVICE_TYPE_OPENCL_GPU | DEVICE_TYPE_CUDA_GPU))) {
				if (desc->GetType() & (DEVICE_TYPE_OPENCL_GPU | DEVICE_TYPE_CUDA_GPU))
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				else if (desc->GetType() & DEVICE_TYPE_OPENCL_CPU)
					desc->SetForceWorkGroupSize(forceCPUWorkSize);

				selectedDeviceDescs.push_back(desc);
				selected = true;
			}
		}

		if (selected && (desc->GetType() & DEVICE_TYPE_CUDA_ALL))
			hasCUDADevice = true;
	}
	
	if (!haveSelectionString && hasCUDADevice) {
		// If there is, at least, a CUDA device selected, use only CUDA devices
		DeviceDescription::Filter(DEVICE_TYPE_CUDA_ALL, selectedDeviceDescs);
	}

	oclRenderThreadCount = selectedDeviceDescs.size();
#endif

	if (selectedDeviceDescs.size() == 0)
		throw runtime_error("No hardware device selected or available");

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (supportsNativeThreads) {
		//----------------------------------------------------------------------
		// Get native device descriptions
		//----------------------------------------------------------------------

		vector<DeviceDescription *> nativeDescs = ctx->GetAvailableDeviceDescriptions();
		DeviceDescription::Filter(DEVICE_TYPE_NATIVE, nativeDescs);
		nativeDescs.resize(1);

		nativeRenderThreadCount = cfg.Get(GetDefaultProps().Get("opencl.native.threads.count")).Get<u_int>();
		if (nativeRenderThreadCount > 0)
			selectedDeviceDescs.resize(selectedDeviceDescs.size() + nativeRenderThreadCount, nativeDescs[0]);
	} else
		nativeRenderThreadCount = 0;
#endif
}

Properties OCLRenderEngine::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("opencl.cpu.use")) <<
			cfg.Get(GetDefaultProps().Get("opencl.gpu.use")) <<
			cfg.Get(GetDefaultProps().Get("opencl.cpu.workgroup.size")) <<
			cfg.Get(GetDefaultProps().Get("opencl.gpu.workgroup.size")) <<
			cfg.Get(GetDefaultProps().Get("opencl.devices.select")) <<
			cfg.Get(GetDefaultProps().Get("opencl.native.threads.count")) <<
			cfg.Get(GetDefaultProps().Get("opencl.outofcore.enable"));
}

const Properties &OCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			RenderEngine::GetDefaultProps() <<
			Property("opencl.cpu.use")(false) <<
			Property("opencl.gpu.use")(true) <<
#if defined(__APPLE__)	
			Property("opencl.cpu.workgroup.size")(1) <<
#else
			Property("opencl.cpu.workgroup.size")(0) <<
#endif
			Property("opencl.gpu.workgroup.size")(32) <<
			Property("opencl.devices.select")("") <<
//For Windows version greater than Windows 7,modern way of calculating processor count is used 
//May not work with Windows version prior to Windows 7
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
			Property("opencl.native.threads.count")((int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)) <<
#else
			Property("opencl.native.threads.count")(boost::thread::hardware_concurrency()) <<
#endif
			Property("opencl.outofcore.enable")(false);

	return props;
}
