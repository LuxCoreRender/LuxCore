#line 2 "biaspathocl_datatypes.cl"

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

	BSDF bsdfPathVertex1;
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo volInfoPathVertex1;
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
	// This is used by TriangleLight_Illuminate() to temporary store the
	// point on the light sources
	// Also used by Scene_Intersect() for evaluating volume textures.
	HitPoint tmpHitPoint;
#endif
} GPUTask;

typedef struct {
	BSDF directLightBSDF;
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo directLightVolInfo;
#endif
} GPUTaskDirectLight;

typedef struct {
	BSDF bsdfPathVertexN;
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo volInfoPathVertexN;
#endif
} GPUTaskPathVertexN;

#endif

typedef struct {
	unsigned int raysCount;
} GPUTaskStats;
