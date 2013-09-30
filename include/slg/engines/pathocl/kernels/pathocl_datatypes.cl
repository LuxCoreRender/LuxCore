#line 2 "pathocl_datatypes.cl"

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

typedef enum {
	RT_NEXT_VERTEX,
	GENERATE_DL_RAY,
	RT_DL,
	GENERATE_NEXT_VERTEX_RAY,
	SPLAT_SAMPLE
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

	BSDFEvent pathBSDFEvent, lastBSDFEvent;
	float lastPdfW;

#if (PARAM_DL_LIGHT_COUNT > 0)
	// This is used by TriangleLight_Illuminate() to temporary store the
	// point on the light sources
	HitPoint tmpHitPoint;
#endif
} PathStateDirectLight;

typedef struct {
	float passThroughEvent; // The passthrough sample used for the shadow ray
	BSDF passThroughBsdf;
} PathStateDirectLightPassThrough;

typedef struct {
	// The task seed
	Seed seed;

	// The state used to keep track of the rendered path
	PathStateBase pathStateBase;
	PathStateDirectLight directLightState;
#if defined(PARAM_HAS_PASSTHROUGH)
	PathStateDirectLightPassThrough passThroughState;
#endif
} GPUTask;

#endif

typedef struct {
	unsigned int sampleCount;
} GPUTaskStats;
