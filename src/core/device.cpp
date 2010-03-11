/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "luxrays/core/device.h"
#include "luxrays/core/context.h"
#include "luxrays/kernels/kernels.h"

#if defined (__linux__)
#include <pthread.h>
#endif

namespace luxrays {

//------------------------------------------------------------------------------
// DeviceDescription
//------------------------------------------------------------------------------

void DeviceDescription::FilterOne(std::vector<DeviceDescription *> &deviceDescriptions) {
	int gpuIndex = -1;
	int cpuIndex = -1;
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		if ((cpuIndex == -1) && (deviceDescriptions[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD))
			cpuIndex = i;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if ((gpuIndex == -1) && (deviceDescriptions[i]->GetType() == DEVICE_TYPE_OPENCL) &&
				(((OpenCLDeviceDescription *)deviceDescriptions[i])->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)) {
			gpuIndex = i;
			break;
		}
#endif
	}

	if (gpuIndex != -1) {
		std::vector<DeviceDescription *> selectedDev;
		selectedDev.push_back(deviceDescriptions[gpuIndex]);
		deviceDescriptions = selectedDev;
	} else if (gpuIndex != -1) {
		std::vector<DeviceDescription *> selectedDev;
		selectedDev.push_back(deviceDescriptions[cpuIndex]);
		deviceDescriptions = selectedDev;
	} else
		deviceDescriptions.clear();
}

void DeviceDescription::Filter(const DeviceType type,
		std::vector<DeviceDescription *> &deviceDescriptions) {
	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		if ((type != DEVICE_TYPE_ALL) && (deviceDescriptions[i]->GetType() != type)) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else
			++i;
	}
}

std::string DeviceDescription::GetDeviceType(const DeviceType type) {
	switch (type) {
		case DEVICE_TYPE_ALL:
			return "ALL";
		case DEVICE_TYPE_NATIVE_THREAD:
			return "NATIVE_THREAD";
		case DEVICE_TYPE_OPENCL:
			return "OPENCL";
		case DEVICE_TYPE_VIRTUAL:
			return "VIRTUAL";
		default:
			return "UNKNOWN";
	}
}

//------------------------------------------------------------------------------
// IntersectionDevice
//------------------------------------------------------------------------------

IntersectionDevice::IntersectionDevice(const Context *context, const DeviceType type, const unsigned int index) :
	deviceContext(context), deviceType(type) {
	deviceIndex = index;
	started = false;
	dataSet = NULL;
}

IntersectionDevice::~IntersectionDevice() {
}

void IntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	assert (!started);
	assert (newDataSet != NULL);
	assert (newDataSet->IsPreprocessed());

	dataSet = newDataSet;
}

void IntersectionDevice::Start() {
	assert (!started);
	assert (dataSet != NULL);

	statsStartTime = WallClockTime();
	statsTotalRayCount = 0.0;
	statsDeviceIdleTime = 0.0;
	statsDeviceTotalTime = 0.0;
	started = true;
}

void IntersectionDevice::Stop() {
	assert (started);
	started = false;
}

}
