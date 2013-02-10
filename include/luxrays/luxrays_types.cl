#line 2 "luxrays_types.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#define NULL_INDEX (0xffffffffu)

#if defined(LUXRAYS_OPENCL_KERNEL)
#if defined(__APPLE_FIX__)

float2 VLOAD2F(__global float *p) {
	return (float2)(p[0], p[1]);
}

void VSTORE2F(const float2 v, __global float *p) {
	p[0] = v.x;
	p[1] = v.y;
}

float3 VLOAD3F(__global float *p) {
	return (float3)(p[0], p[1], p[2]);
}

void VSTORE3F(const float3 v, __global float *p) {
	p[0] = v.x;
	p[1] = v.y;
	p[2] = v.z;
}

float4 VLOAD4F(__global float *p) {
	return (float4)(p[0], p[1], p[2], p[3]);
}

void VSTORE4F(const float4 v, __global float *p) {
	p[0] = v.x;
	p[1] = v.y;
	p[2] = v.z;
	p[3] = v.w;
}

#else

float2 VLOAD2F(__global float *p) {
	return vload2(0, p);
}

void VSTORE2F(const float2 v, __global float *p) {
	vstore2(v, 0, p);
}

float3 VLOAD3F(__global float *p) {
	return vload3(0, p);
}

void VSTORE3F(const float3 v, __global float *p) {
	vstore3(v, 0, p);
}

float4 VLOAD4F(__global float *p) {
	return vload4(0, p);
}

void VSTORE4F(const float4 v, __global float *p) {
	vstore4(v, 0, p);
}

#endif
#endif
