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

#include "luxrays/utils/strutils.h"
#include "luxrays/core/accelerator.h"

using namespace std;
using namespace luxrays;

string Accelerator::AcceleratorType2String(const AcceleratorType type) {
	switch(type) {
		case ACCEL_AUTO:
			return "AUTO";
		case ACCEL_BVH:
			return "BVH";
		case ACCEL_MBVH:
			return "MBVH";
		case ACCEL_EMBREE:
			return "EMBREE";
		case ACCEL_OPTIX:
			return "OPTIX";
		default:
			throw runtime_error("Unknown accelerator type in AcceleratorType2String(): " + ToString(type));
	}
}

AcceleratorType Accelerator::String2AcceleratorType(const string &type) {
	if (type == "AUTO")
		return ACCEL_AUTO;
	else if (type == "BVH")
		return ACCEL_BVH;
	else if (type == "MBVH")
		return ACCEL_MBVH;
	else if (type == "EMBREE")
		return ACCEL_EMBREE;
	else if (type == "OPTIX")
		return ACCEL_OPTIX;
	else
		throw runtime_error("Unknown accelerator type in String2AcceleratorType(): " + type);
}
