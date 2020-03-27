#line 2 "cudadevice_oclemul.cl"

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



#define __kernel extern "C" __global__
#define __global

#define uint unsigned int

#define INFINITY __int_as_float(0x7f800000)

__device__ inline size_t get_global_id(const uint dimIndex) {
	switch (dimIndex) {
		case 0:
			return blockIdx.x;
		case 1:
			return blockIdx.y;
		case 2:
			return blockIdx.z;
		default:
			return 0;
	}
}
