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

#include "luxrays/devices/nativeintersectiondevice.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Native Device Description
//------------------------------------------------------------------------------

void NativeIntersectionDeviceDescription::AddDeviceDescs(std::vector<DeviceDescription *> &descriptions) {
	descriptions.push_back(new NativeIntersectionDeviceDescription("Native"));
}

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

NativeIntersectionDevice::NativeIntersectionDevice(const Context *context,
		NativeIntersectionDeviceDescription *desc,
		const size_t devIndex) :
	Device(context, devIndex), deviceDesc(desc) {

	deviceName = std::string("NativeIntersect");
}

NativeIntersectionDevice::~NativeIntersectionDevice() {
}

void NativeIntersectionDevice::SetDataSet(DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);

	if (dataSet) {
		const AcceleratorType accelType = dataSet->GetAcceleratorType();
		if (accelType != ACCEL_AUTO)
			accel = dataSet->GetAccelerator(accelType);
		else
			accel = dataSet->GetAccelerator(ACCEL_EMBREE);
	}
}

}
