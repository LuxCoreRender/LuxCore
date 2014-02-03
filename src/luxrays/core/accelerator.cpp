/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "luxrays/core/accelerator.h"

using namespace luxrays;

std::string Accelerator::AcceleratorType2String(const AcceleratorType type) {
	switch(type) {
		case ACCEL_AUTO:
			return "AUTO";
		case ACCEL_BVH:
			return "BVH";
		case ACCEL_QBVH:
			return "QBVH";
		case ACCEL_MQBVH:
			return "MQBVH";
		case ACCEL_MBVH:
			return "MBVH";
		default:
			throw std::runtime_error("Unknown AcceleratorType in AcceleratorType2String()");
	}
}
