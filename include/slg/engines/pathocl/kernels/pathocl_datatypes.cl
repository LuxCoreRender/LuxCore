#line 2 "pathocl_datatypes.cl"

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

typedef enum {
	// Mega-kernel states
	RT_NEXT_VERTEX = 0,
	GENERATE_DL_RAY = 1,
	RT_DL = 2,
	GENERATE_NEXT_VERTEX_RAY = 3,
	SPLAT_SAMPLE = 4,
			
	// Micro-kernel states
	MK_RT_NEXT_VERTEX = 0, // Must have the same value of RT_NEXT_VERTEX
	MK_GENERATE_DL_RAY = 1,
	MK_RT_DL = 2,
	MK_GENERATE_NEXT_VERTEX_RAY = 3,
	MK_SPLAT_SAMPLE = 4,
	MK_GENERATE_CAMERA_RAY = 5
} PathState;

// This is defined only under OpenCL because of variable size structures
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	PathState state;
	unsigned int depth;

	Spectrum throughput;
	BSDF bsdf; // Variable size structure
} PathStateBase;

typedef struct {
	// Radiance to add to the result if light source is visible
	Spectrum lightRadiance;
	uint lightID;

	BSDFEvent lastBSDFEvent;
	float lastPdfW;
} PathStateDirectLight;

typedef struct {
	// The task seed
	Seed seed;

	// The state used to keep track of the rendered path
	PathStateBase pathStateBase;
	PathStateDirectLight directLightState;

#if defined(PARAM_HAS_PASSTHROUGH)
	float directLightRayPassThroughEvent;
#endif

	// Space for temporary storage
#if defined(PARAM_HAS_PASSTHROUGH) || defined(PARAM_HAS_VOLUMES)
	BSDF tmpBsdf; // Variable size structure
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
	// This is used by TriangleLight_Illuminate() to temporary store the
	// point on the light sources.
	// Also used by Scene_Intersect() for evaluating volume textures.
	HitPoint tmpHitPoint;
#endif
} GPUTask;

#endif

typedef struct {
	unsigned int sampleCount;
} GPUTaskStats;
