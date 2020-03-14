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

#include "luxrays/core/intersectiondevice.h"

namespace luxrays {

//------------------------------------------------------------------------------
// IntersectionDevice
//------------------------------------------------------------------------------

IntersectionDevice::IntersectionDevice() : dataSet(NULL) {
}

IntersectionDevice::~IntersectionDevice() {
	if (started)
		Stop();
}

void IntersectionDevice::SetDataSet(DataSet *newDataSet) {
	assert (!started);

	dataSet = newDataSet;
}

void IntersectionDevice::Start() {
	assert (dataSet != NULL);

	Device::Start();

	statsStartTime = WallClockTime();
	statsTotalSerialRayCount = 0.0;
	statsTotalDataParallelRayCount = 0.0;
}

}
