#line 2 "cudadevice_oclemul_funcs.cl"

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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// OpenCL functions
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// get_global_id()
//------------------------------------------------------------------------------

__forceinline__ size_t get_global_id(const uint dimIndex) {
	switch (dimIndex) {
		case 0:
			return blockIdx.x * blockDim.x + threadIdx.x;
		case 1:
			return blockIdx.y * blockDim.y + threadIdx.y;
		case 2:
			return blockIdx.z * blockDim.z + threadIdx.z;
		default:
			return 0;
	}
}

//------------------------------------------------------------------------------
// get_local_id()
//------------------------------------------------------------------------------

__forceinline__ size_t get_local_id(const uint dimIndex) {
	switch (dimIndex) {
		case 0:
			return threadIdx.x;
		case 1:
			return threadIdx.y;
		case 2:
			return threadIdx.z;
		default:
			return 0;
	}
}

//------------------------------------------------------------------------------
// get_local_size()
//------------------------------------------------------------------------------

__forceinline__ size_t get_local_size(const uint dimIndex) {
	switch (dimIndex) {
		case 0:
			return blockDim.x;
		case 1:
			return blockDim.y;
		case 2:
			return blockDim.z;
		default:
			return 0;
	}
}

//------------------------------------------------------------------------------
// get_group_id()
//------------------------------------------------------------------------------

__forceinline__ size_t get_group_id(const uint dimIndex) {
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

//------------------------------------------------------------------------------
// barrier()
//------------------------------------------------------------------------------

#define CLK_LOCAL_MEM_FENCE 0
//#define CLK_GLOBAL_MEM_FENCE 1

__forceinline__ void barrier(const uint flag) {
	// This works for CLK_LOCAL_MEM_FENCE
	__syncthreads();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Math functions
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// MAKE_FLOATn()
//------------------------------------------------------------------------------

#define MAKE_FLOAT2(x, y) make_float2(x, y)
#define MAKE_FLOAT3(x, y, z) make_float3(x, y, z)
#define MAKE_FLOAT4(x, y, z, w) make_float4(x, y, z, w)

__forceinline__ float2 TO_FLOAT2(const float x) {
	return make_float2(x, x);
}

__forceinline__ float3 TO_FLOAT3(const float x) {
	return make_float3(x, x, x);
}

__forceinline__ float4 TO_FLOAT4(const float x) {
	return make_float4(x, x, x, x);
}

//------------------------------------------------------------------------------
// vload_half()
//------------------------------------------------------------------------------

typedef unsigned short half;

__forceinline__ float __half2float(const half a) {
	float val;

    asm("{  cvt.f32.f16 %0, %1;}\n" : "=f"(val) : "h"(*(reinterpret_cast<const unsigned short *>(&a))));

    return val;
}

__forceinline__ float vload_half(const size_t offset, const half *p) {
	return __half2float(p[offset]);
}

//------------------------------------------------------------------------------
// vloadn()
//------------------------------------------------------------------------------

__forceinline__ float2 vload2(const size_t offset, const float *p) {
	return make_float2(p[offset], p[offset + 1]);
}

__forceinline__ float3 vload3(const size_t offset, const float *p) {
	return make_float3(p[offset], p[offset + 1], p[offset + 2]);
}

__forceinline__ float4 vload4(const size_t offset, const float *p) {
	return make_float4(p[offset], p[offset + 1], p[offset + 2], p[offset + 3]);
}

//------------------------------------------------------------------------------
// vstoren()
//------------------------------------------------------------------------------

__forceinline__ void vstore2(const float2 data, const size_t offset, float *p) {
	p[offset] = data.x;
	p[offset + 1] = data.y;
}

__forceinline__ void vstore3(const float3 data, const size_t offset, float *p) {
	p[offset] = data.x;
	p[offset + 1] = data.y;
	p[offset + 2] = data.z;
}

__forceinline__ void vstore4(const float4 data, const size_t offset, float *p) {
	p[offset] = data.x;
	p[offset + 1] = data.y;
	p[offset + 2] = data.z;
	p[offset + 2] = data.w;
}

//------------------------------------------------------------------------------
// mix()
//------------------------------------------------------------------------------

__forceinline__ float mix(const float x, const float y, const float a) {
	return x + (y - x) * a;
}

__forceinline__ float2 mix(const float2 x, const float2 y, const float a) {
	return x + (y - x) * a;
}
__forceinline__ float2 mix(const float2 x, const float2 y, const float2 a) {
	return x + (y - x) * a;
}

__forceinline__ float3 mix(const float3 x, const float3 y, const float a) {
	return x + (y - x) * a;
}
__forceinline__ float3 mix(const float3 x, const float3 y, const float3 a) {
	return x + (y - x) * a;
}

__forceinline__ float4 mix(const float4 x, const float4 y, const float a) {
	return x + (y - x) * a;
}
__forceinline__ float4 mix(const float4 x, const float4 y, const float4 a) {
	return x + (y - x) * a;
}

//------------------------------------------------------------------------------
// isequal()
//------------------------------------------------------------------------------

__forceinline__ int isequal(const float x, const float y) {
	return x == y;
}

__forceinline__ int2 isequal(const float2 x, const float2 y) {
	return make_int2(x.x == y.x ? -1 : 0, x.y == y.y ? -1 : 0);
}

__forceinline__ int3 isequal(const float3 x, const float3 y) {
	return make_int3(x.x == y.x ? -1 : 0, x.y == y.y ? -1 : 0, x.z == y.z ? -1 : 0);
}

__forceinline__ int4 isequal(const float4 x, const float4 y) {
	return make_int4(x.x == y.x ? -1 : 0, x.y == y.y ? -1 : 0, x.z == y.z ? -1 : 0, x.w == y.w ? -1 : 0);
}

//------------------------------------------------------------------------------
// all()
//------------------------------------------------------------------------------

__forceinline__ int all(const int x) {
	return x & -1;
}

__forceinline__ int all(const int2 x) {
	return all(x.x) && all(x.y);
}

__forceinline__ int all(const int3 x) {
	return all(x.x) && all(x.y) && all(x.z);
}

__forceinline__ int all(const int4 x) {
	return all(x.x) && all(x.y) && all(x.z) && all(x.w);
}

//------------------------------------------------------------------------------
// any()
//------------------------------------------------------------------------------

__forceinline__ int any(const int x) {
	return x & -1;
}

__forceinline__ int any(const int2 x) {
	return any(x.x) || any(x.y);
}

__forceinline__ int any(const int3 x) {
	return any(x.x) || any(x.y) || any(x.z);
}

__forceinline__ int any(const int4 x) {
	return any(x.x) || any(x.y) || any(x.z) || any(x.w);
}

//------------------------------------------------------------------------------
// isnan()
//------------------------------------------------------------------------------

__forceinline__ int2 isnan(const float2 x) {
	return make_int2(isnan(x.x), isnan(x.y));
}

__forceinline__ int3 isnan(const float3 x) {
	return make_int3(isnan(x.x), isnan(x.y), isnan(x.z));
}

__forceinline__ int4 isnan(const float4 x) {
	return make_int4(isnan(x.x), isnan(x.y), isnan(x.z), isnan(x.w));
}

//------------------------------------------------------------------------------
// isinf()
//------------------------------------------------------------------------------

__forceinline__ int2 isinf(const float2 x) {
	return make_int2(isinf(x.x), isinf(x.y));
}

__forceinline__ int3 isinf(const float3 x) {
	return make_int3(isinf(x.x), isinf(x.y), isinf(x.z));
}

__forceinline__ int4 isinf(const float4 x) {
	return make_int4(isinf(x.x), isinf(x.y), isinf(x.z), isinf(x.w));
}

//------------------------------------------------------------------------------
// sqrt()
//------------------------------------------------------------------------------

__forceinline__ float sqrt(const float x) {
	return sqrtf(x);
}

//------------------------------------------------------------------------------
// pow()
//------------------------------------------------------------------------------

__forceinline__ float pow(const float x, const float y) {
	return powf(x, y);
}

//------------------------------------------------------------------------------
// native_powr()
//------------------------------------------------------------------------------

__forceinline__ float native_powr(const float x, const float y) {
	return powf(x, y);
}

//------------------------------------------------------------------------------
// exp()
//------------------------------------------------------------------------------

__forceinline__ float exp(const float x) {
	return expf(x);
}

//------------------------------------------------------------------------------
// native_exp()
//------------------------------------------------------------------------------

__forceinline__ float native_exp(const float x) {
	return expf(x);
}

//------------------------------------------------------------------------------
// log()
//------------------------------------------------------------------------------

__forceinline__ float log(const float x) {
	return logf(x);
}

__forceinline__ float3 log(const float3 x) {
	return MAKE_FLOAT3(logf(x.x), logf(x.y), logf(x.z));
}

//------------------------------------------------------------------------------
// native_log()
//------------------------------------------------------------------------------

__forceinline__ float native_log(const float x) {
	return logf(x);
}

//------------------------------------------------------------------------------
// sin()
//------------------------------------------------------------------------------

__forceinline__ float sin(const float x) {
	return sinf(x);
}

//------------------------------------------------------------------------------
// cos()
//------------------------------------------------------------------------------

__forceinline__ float cos(const float x) {
	return cosf(x);
}

//------------------------------------------------------------------------------
// atan()
//------------------------------------------------------------------------------

__forceinline__ float atan(const float x) {
	return atanf(x);
}

//------------------------------------------------------------------------------
// atan2()
//------------------------------------------------------------------------------

__forceinline__ float atan2(const float x, const float y) {
	return atan2f(x, y);
}

//------------------------------------------------------------------------------
// atanh()
//------------------------------------------------------------------------------

__forceinline__ float atanh(const float x) {
	return atanhf(x);
}

//------------------------------------------------------------------------------
// fabs()
//------------------------------------------------------------------------------

__forceinline__ float fabs(const float x) {
	return fabsf(x);
}

//------------------------------------------------------------------------------
// fmax()
//------------------------------------------------------------------------------

__forceinline__ float fmax(const float x, const float y) {
	return fmaxf(x, y);
}


__forceinline__ float3 fmax(const float3 x, const float3 y) {
	return MAKE_FLOAT3(fmaxf(x.x, y.x), fmaxf(x.y, y.y), fmaxf(x.z, y.z));
}

//------------------------------------------------------------------------------
// fmin()
//------------------------------------------------------------------------------

__forceinline__ float fmin(const float x, const float y) {
	return fminf(x, y);
}

__forceinline__ float3 fmin(const float3 x, const float3 y) {
	return MAKE_FLOAT3(fminf(x.x, y.x), fminf(x.y, y.y), fminf(x.z, y.z));
}

//------------------------------------------------------------------------------
// as_*()
//------------------------------------------------------------------------------

template<typename T>
__forceinline__ int as_int(const T v) {
	union {
		T src;
		int dst;
	} val;
	
	val.src = v;

	return val.dst;
}

template<typename T>
__forceinline__ uint as_uint(const T v) {
	union {
		T src;
		uint dst;
	} val;
	
	val.src = v;

	return val.dst;
}

template<typename T>
__forceinline__ float as_float(const T v) {
	union {
		T src;
		float dst;
	} val;
	
	val.src = v;

	return val.dst;
}

//------------------------------------------------------------------------------
// atomic_*()
//------------------------------------------------------------------------------

uint atomic_cmpxchg(uint *p, uint cmp, uint val) {
	return atomicCAS(p, cmp, val);
}

uint atomic_inc(uint *p) {
	return atomicAdd(p, 1u);
}

int atomic_inc(int *p) {
	return atomicAdd(p, 1);
}

uint atomic_dec(uint *p) {
	return atomicSub(p, 1u);
}

int atomic_dec(int *p) {
	return atomicSub(p, 1);
}
