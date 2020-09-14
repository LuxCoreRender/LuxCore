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

#include <vector>

#include "luxrays/core/hardwaredevice.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// HardwareDevice
//------------------------------------------------------------------------------

HardwareDevice::HardwareDevice() {
	usedMemory = 0;
}

HardwareDevice::~HardwareDevice() {
	if (usedMemory != 0)
		LR_LOG(deviceContext, "WARNING: there is a memory leak in LuxRays HardwareDevice " << GetName() << ": " << ToString(usedMemory) << "bytes");
}

void HardwareDevice::SetAdditionalCompileOpts(const vector<string> &opts) {
	additionalCompileOpts = opts;
}

const vector<string> &HardwareDevice::GetAdditionalCompileOpts() {
	return additionalCompileOpts;
}

template <>
void HardwareDevice::SetKernelArg<HardwareDeviceBufferPtr>(HardwareDeviceKernel *kernel, const u_int index, const HardwareDeviceBufferPtr &buff) {
	SetKernelArgBuffer(kernel, index, buff);
}
