/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/device.h"
#include <functional>
#include <sstream>
#include <ranges>

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// DeviceDescription
//------------------------------------------------------------------------------

void DeviceDescription::FilterOne(vector<DeviceDescription *> &deviceDescriptions) {
	int gpuIndex = -1;
	int cpuIndex = -1;
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		if ((cpuIndex == -1) && (deviceDescriptions[i]->GetType() &
			DEVICE_TYPE_NATIVE))
			cpuIndex = (int)i;
		else if ((gpuIndex == -1) && (deviceDescriptions[i]->GetType() &
			DEVICE_TYPE_OPENCL_GPU)) {
			gpuIndex = (int)i;
			break;
		}
	}

	if (gpuIndex != -1) {
		vector<DeviceDescription *> selectedDev;
		selectedDev.push_back(deviceDescriptions[gpuIndex]);
		deviceDescriptions = selectedDev;
	} else if (gpuIndex != -1) {
		vector<DeviceDescription *> selectedDev;
		selectedDev.push_back(deviceDescriptions[cpuIndex]);
		deviceDescriptions = selectedDev;
	} else
		deviceDescriptions.clear();
}

void DeviceDescription::Filter(
	const DeviceType type,
	std::vector<std::reference_wrapper<DeviceDescription>> &deviceDescriptions
) {
	if (type == DEVICE_TYPE_ALL)
		return;
	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		DeviceDescriptionRef desc = deviceDescriptions[i];
		if ((desc.GetType() & type) == 0) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else {
			++i;
		}
	}
}

std::string DeviceDescription::GetDeviceType(const DeviceType type) {
	switch (type) {
		case DEVICE_TYPE_ALL:
			return "ALL";
		case DEVICE_TYPE_NATIVE:
			return "NATIVE_THREAD";
		case DEVICE_TYPE_OPENCL_ALL:
			return "OPENCL_ALL";
		case DEVICE_TYPE_OPENCL_DEFAULT:
			return "OPENCL_DEFAULT";
		case DEVICE_TYPE_OPENCL_CPU:
			return "OPENCL_CPU";
		case DEVICE_TYPE_OPENCL_GPU:
			return "OPENCL_GPU";
		case DEVICE_TYPE_OPENCL_UNKNOWN:
			return "OPENCL_UNKNOWN";
		case DEVICE_TYPE_CUDA_GPU:
			return "CUDA_GPU";
		default:
			throw runtime_error("Unknown device type in DeviceDescription::GetDeviceType(): " + ::ToString(type));
	}
}

std::string DeviceDescription::ToString() {
	std::stringstream ss;
	ss
		<< "{"
		<< "'" << name << "'" << ", "
		<< GetDeviceType(type)
		<< "}";
	return ss.str();
}


std::ostream& luxrays::operator<< (std::ostream& stream, const luxrays::DeviceDescriptions& descriptions) {

	auto toString = std::mem_fn(&DeviceDescription::ToString);

	auto joined = descriptions
		| std::views::transform(toString)
		| std::views::join_with(',');
	std::ranges::copy(joined, std::ostream_iterator<char>(stream));
	return stream;
}

//------------------------------------------------------------------------------
// Device
//------------------------------------------------------------------------------

Device::Device(const Context & context, const size_t index) :
	deviceContext(context) {
	deviceIndex = index;
	started = false;
}

Device::~Device() {
}

void Device::Start() {
	assert (!started);
	started = true;
}

void Device::Interrupt() {
	assert (started);
}

void Device::Stop() {
	assert (started);
	started = false;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
