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

#ifndef LINKERTEST_H
#define LINKERTEST_H

class LinkerVectorTest {
public:
	__host__ __device__ LinkerVectorTest(float _x = 0.f, float _y = 0.f, float _z = 0.f);
		
	__host__ __device__ LinkerVectorTest operator+(const LinkerVectorTest &v) const;

	__host__ __device__ bool operator!=(const LinkerVectorTest &v) const {
		return (x != v.x) || (y != v.y) || (z != v.z);
	}

	float x, y, z;
};

extern __global__ void LinkerVectorTestKernel2(LinkerVectorTest *va, LinkerVectorTest *vb, LinkerVectorTest *vc);

#endif /* LINKERTEST_H */

