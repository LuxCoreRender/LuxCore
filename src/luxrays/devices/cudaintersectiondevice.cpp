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

#if !defined(LUXRAYS_DISABLE_CUDA)

#include "luxrays/devices/cudaintersectiondevice.h"

using namespace std;

namespace luxrays {

//------------------------------------------------------------------------------
// CUDA IntersectionDevice
//------------------------------------------------------------------------------

CUDAIntersectionDevice::CUDAIntersectionDevice(
		const Context *context,
		CUDADeviceDescription *desc,
		const size_t devIndex) :
		Device(context, devIndex), CUDADevice(context, desc, devIndex),
		HardwareIntersectionDevice(), kernel(nullptr) {
}

CUDAIntersectionDevice::~CUDAIntersectionDevice() {
}

void CUDAIntersectionDevice::SetDataSet(DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);

	if (dataSet) {
		const AcceleratorType accelType = dataSet->GetAcceleratorType();
		if (accelType != ACCEL_AUTO) {
			accel = dataSet->GetAccelerator(accelType);
		} else {
			if (dataSet->RequiresInstanceSupport() || dataSet->RequiresMotionBlurSupport()) {
				if (optixContext)
					accel = dataSet->GetAccelerator(ACCEL_OPTIX);
				else
					accel = dataSet->GetAccelerator(ACCEL_MBVH);
			} else {
				if (optixContext)
					accel = dataSet->GetAccelerator(ACCEL_OPTIX);
				else
					accel = dataSet->GetAccelerator(ACCEL_BVH);
			}
		}
	}
}

void CUDAIntersectionDevice::Update() {
	kernel->Update(dataSet);
}

void CUDAIntersectionDevice::Start() {
	CUDADevice::Start();

	// Compile required kernel
	kernel = accel->NewHardwareIntersectionKernel(*this);
}

void CUDAIntersectionDevice::Stop() {
	delete kernel;
	kernel = nullptr;

	CUDADevice::Stop();
}

void CUDAIntersectionDevice::EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff,
			const unsigned int rayCount) {
	// Enqueue the intersection kernel
	kernel->EnqueueTraceRayBuffer(rayBuff, rayHitBuff, rayCount);
	statsTotalDataParallelRayCount += rayCount;
}

}

#endif
