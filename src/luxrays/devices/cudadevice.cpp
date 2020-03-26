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

#if defined(LUXRAYS_ENABLE_CUDA)

#include "luxrays/devices/cudadevice.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

CUDADeviceDescription::CUDADeviceDescription(CUdevice dev, const size_t devIndex) :
		DeviceDescription("CUDAInitializingDevice", DEVICE_TYPE_CUDA_GPU),
		deviceIndex(devIndex), cudaDevice(dev) {
	char buff[128];
    CHECK_CUDA_ERROR(cuDeviceGetName(buff, 128, cudaDevice));
	name = string(buff);
	
	CHECK_CUDA_ERROR(cuDevicePrimaryCtxRetain(&cudaContext, cudaDevice));
}

CUDADeviceDescription::~CUDADeviceDescription() {
	CHECK_CUDA_ERROR(cuDevicePrimaryCtxRelease(cudaDevice));
}

int CUDADeviceDescription::GetComputeUnits() const {
	int major;
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cudaDevice));
	int minor;
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cudaDevice));
	
	typedef struct {
		int SM;
		int Cores;
	} SM2Cores;

	static SM2Cores archCoresPerSM[] = {
		// 0xMm (hexidecimal notation) where M = SM Major version and m = SM minor version
		{0x30, 192},
		{0x32, 192},
		{0x35, 192},
		{0x37, 192},
		{0x50, 128},
		{0x52, 128},
		{0x53, 128},
		{0x60, 64},
		{0x61, 128},
		{0x62, 128},
		{0x70, 64},
		{0x72, 64},
		{0x75, 64},
		{-1, -1}
	};

	int index = 0;
	while (archCoresPerSM[index].SM != -1) {
		if (archCoresPerSM[index].SM == ((major << 4) + minor))
			return archCoresPerSM[index].Cores;

		index++;
	}

	return DeviceDescription::GetComputeUnits();
}

u_int CUDADeviceDescription::GetNativeVectorWidthFloat() const {
	return 1;
}

size_t CUDADeviceDescription::GetMaxMemory() const {
	size_t bytes;
	CHECK_CUDA_ERROR(cuDeviceTotalMem(&bytes, cudaDevice));
	
	return bytes;
}

size_t CUDADeviceDescription::GetMaxMemoryAllocSize() const {
	return GetMaxMemory();
}

void CUDADeviceDescription::AddDeviceDescs(vector<DeviceDescription *> &descriptions) {
	int devCount;
	CHECK_CUDA_ERROR(cuDeviceGetCount(&devCount));

	for (int i = 0; i < devCount; i++) {
		CUdevice device;
		CHECK_CUDA_ERROR(cuDeviceGet(&device, i));

		CUDADeviceDescription *desc = new CUDADeviceDescription(device, i);

		descriptions.push_back(desc);
	}
}

#endif
