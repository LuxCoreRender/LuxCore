/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/luxrays.h"

#include "linkertest.h"

//------------------------------------------------------------------------------
// LinkerTest()
//------------------------------------------------------------------------------

__host__ __device__ LinkerVectorTest::LinkerVectorTest(float _x, float _y, float _z) :
		x(_x), y(_y), z(_z) {
}

__global__ void LinkerVectorTestKernel2(LinkerVectorTest *va, LinkerVectorTest *vb, LinkerVectorTest *vc) {
	const u_int index = blockIdx.x * blockDim.x + threadIdx.x;

	vc[index] = va[index] + vb[index];
}
