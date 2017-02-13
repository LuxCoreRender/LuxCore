#line 2 "ray_types.cl"

/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#define RAY_FLAGS_NONE   0x00000000
#define RAY_FLAGS_MASKED 0x00000001

typedef struct {
	Point o;
	Vector d;
	float mint, maxt, time;

	unsigned int flags;
	float pad[2]; // Add padding to avoid size discrepancies between OpenCL and C++
} Ray;

typedef struct {
	float t, b1, b2;
	unsigned int meshIndex, triangleIndex;
} RayHit;
