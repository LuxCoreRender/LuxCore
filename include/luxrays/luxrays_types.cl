#line 2 "luxrays_types.cl"

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

#define NULL_INDEX (0xffffffffu)

#if defined(LUXRAYS_OPENCL_KERNEL)

// Used when the source are not compiled by SLG library
#if !defined(OPENCL_FORCE_NOT_INLINE)
#define OPENCL_FORCE_NOT_INLINE
#endif

#if !defined(OPENCL_FORCE_INLINE)
#define OPENCL_FORCE_INLINE
#endif

#define NULL 0

#if defined(__APPLE_CL__)
OPENCL_FORCE_INLINE float3 __OVERLOAD__ mix(float3 a, float3 b, float t)
{
	return a + ( b - a ) * t;
}
#endif

#if defined(__APPLE_FIX__)

OPENCL_FORCE_INLINE float2 VLOAD2F(const __global float *p) {
	return (float2)(p[0], p[1]);
}

OPENCL_FORCE_INLINE void VSTORE2F(const float2 v, __global float *p) {
	p[0] = v.x;
	p[1] = v.y;
}

OPENCL_FORCE_INLINE float3 VLOAD3F(const __global float *p) {
	return (float3)(p[0], p[1], p[2]);
}

OPENCL_FORCE_INLINE float3 VLOAD3F_Private(const float *p) {
	return (float3)(p[0], p[1], p[2]);
}

OPENCL_FORCE_INLINE void VSTORE3F(const float3 v, __global float *p) {
	p[0] = v.x;
	p[1] = v.y;
	p[2] = v.z;
}

OPENCL_FORCE_INLINE float4 VLOAD4F(const __global float *p) {
	return (float4)(p[0], p[1], p[2], p[3]);
}

OPENCL_FORCE_INLINE float4 VLOAD4F_Private(const float *p) {
	return (float4)(p[0], p[1], p[2], p[3]);
}

OPENCL_FORCE_INLINE void VSTORE4F(const float4 v, __global float *p) {
	p[0] = v.x;
	p[1] = v.y;
	p[2] = v.z;
	p[3] = v.w;
}

#else

OPENCL_FORCE_INLINE float2 VLOAD2F(const __global float *p) {
	return vload2(0, p);
}

OPENCL_FORCE_INLINE void VSTORE2F(const float2 v, __global float *p) {
	vstore2(v, 0, p);
}

OPENCL_FORCE_INLINE float3 VLOAD3F(const __global float *p) {
	return vload3(0, p);
}

OPENCL_FORCE_INLINE float3 VLOAD3F_Private(const float *p) {
	return vload3(0, p);
}

OPENCL_FORCE_INLINE void VSTORE3F(const float3 v, __global float *p) {
	vstore3(v, 0, p);
}

OPENCL_FORCE_INLINE float4 VLOAD4F(const __global float *p) {
	return vload4(0, p);
}

// Input address must be aligned to 16B
// This performs better than vload4()
OPENCL_FORCE_INLINE float4 VLOAD4F_Align(const __global float *p) {
	return *((const __global float4 *)p);
}

OPENCL_FORCE_INLINE float4 VLOAD4F_Private(const float *p) {
	return vload4(0, p);
}

OPENCL_FORCE_INLINE void VSTORE4F(const float4 v, __global float *p) {
	vstore4(v, 0, p);
}

#endif

OPENCL_FORCE_INLINE void VADD3F(__global float *p, const float3 v) {
	VSTORE3F(VLOAD3F(p) + v, p);
}

#endif
