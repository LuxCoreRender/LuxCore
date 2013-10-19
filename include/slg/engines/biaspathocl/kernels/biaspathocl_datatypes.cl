#line 2 "biaspathocl_datatypes.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

//------------------------------------------------------------------------------
// Some OpenCL specific definition
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#if defined(PARAM_USE_PIXEL_ATOMICS)
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif

//------------------------------------------------------------------------------
// GPUTask data types
//------------------------------------------------------------------------------

#define NEXT_VERTEX_TRACE_RAY (1<<0)
#define NEXT_VERTEX_GENERATE_RAY (1<<1)
#define DIRECT_LIGHT_TRACE_RAY (1<<2)
#define DIRECT_LIGHT_GENERATE_RAY (1<<3)
#define LOW_STATE_MASK 0xffff

#define PATH_VERTEX_1 (1<<16)
#define PATH_VERTEX_N (1<<17)
#define DONE (1<<18)
#define HIGH_STATE_MASK 0xffff0000

// This is defined only under OpenCL because of variable size structures
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	// The task seed
	Seed seed;

	Spectrum throughputPathVertex1;
	BSDF bsdfPathVertex1;
} GPUTask;

typedef struct {
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
	unsigned int lightIndex, lightSampleIndex;
#endif

	Spectrum directLightThroughput;
	BSDF directLightBSDF;
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	// This is used by TriangleLight_Illuminate() to temporary store the
	// point on the light sources
	HitPoint directLightHitPoint;
#endif

	// Direct light sampling. Radiance to add to the result
	// if light source is visible.
	Spectrum lightRadiance;
	unsigned int lightID;
} GPUTaskDirectLight;

typedef struct {
	// DIFFUSE, GLOSSY and SPECULAR BSDF sampling
	BSDFEvent vertex1SampleComponent;
	unsigned int vertex1SampleIndex;

	Spectrum throughputPathVertexN;
	BSDF bsdfPathVertexN;
} GPUTaskPathVertexN;

#endif

typedef struct {
	unsigned int raysCount;
} GPUTaskStats;
