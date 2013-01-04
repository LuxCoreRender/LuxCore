#line 2 "datatypes.cl"

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

//------------------------------------------------------------------------------
// Some OpenCL specific definition
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#if defined(PARAM_USE_PIXEL_ATOMICS)
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#endif

#if defined(PARAM_HAS_SUNLIGHT) & !defined(PARAM_DIRECT_LIGHT_SAMPLING)
Error: PARAM_HAS_SUNLIGHT requires PARAM_DIRECT_LIGHT_SAMPLING !
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif

//------------------------------------------------------------------------------
// Frame buffer data types
//------------------------------------------------------------------------------

typedef struct {
	Spectrum c;
	float count;
} Pixel;

typedef struct {
	float alpha;
} AlphaPixel;

//------------------------------------------------------------------------------
// Sample data types
//------------------------------------------------------------------------------

typedef struct {
	Spectrum radiance;

	unsigned int pixelIndex;
} RandomSample;

typedef struct {
	Spectrum radiance;

	float totalI;

	// Using ushort here totally freeze the ATI driver
	unsigned int largeMutationCount, smallMutationCount;
	unsigned int current, proposed, consecutiveRejects;

	float weight;
	Spectrum currentRadiance;
	float currentAlpha;
} MetropolisSampleWithAlphaChannel;

typedef struct {
	Spectrum radiance;

	float totalI;

	// Using ushort here totally freeze the ATI driver
	unsigned int largeMutationCount, smallMutationCount;
	unsigned int current, proposed, consecutiveRejects;

	float weight;
	Spectrum currentRadiance;
} MetropolisSampleWithoutAlphaChannel;

#if defined(SLG_OPENCL_KERNEL)

#if (PARAM_SAMPLER_TYPE == 0)
typedef RandomSample Sample;
#endif

#if (PARAM_SAMPLER_TYPE == 1)
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
typedef MetropolisSampleWithAlphaChannel Sample;
#else
typedef MetropolisSampleWithoutAlphaChannel Sample;
#endif
#endif

#endif

//------------------------------------------------------------------------------
// Indices of Sample related u[] array
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define IDX_SCREEN_X 0
#define IDX_SCREEN_Y 1
#if defined(PARAM_CAMERA_HAS_DOF)
#define IDX_DOF_X 2
#define IDX_DOF_Y 3
#define IDX_BSDF_OFFSET 4
#else
#define IDX_BSDF_OFFSET 2
#endif

// Relative to IDX_BSDF_OFFSET + PathDepth * SAMPLE_SIZE
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) && defined(PARAM_HAS_PASSTHROUGHT)

#define IDX_PASSTROUGHT 0
#define IDX_BSDF_X 1
#define IDX_BSDF_Y 2
#define IDX_DIRECTLIGHT_X 3
#define IDX_DIRECTLIGHT_Y 4
#define IDX_DIRECTLIGHT_Z 5
#define IDX_DIRECTLIGHT_W 6
#define IDX_DIRECTLIGHT_A 7
#define IDX_RR 8

#define SAMPLE_SIZE 9

#elif defined(PARAM_DIRECT_LIGHT_SAMPLING)

#define IDX_BSDF_X 0
#define IDX_BSDF_Y 1
#define IDX_DIRECTLIGHT_X 2
#define IDX_DIRECTLIGHT_Y 3
#define IDX_DIRECTLIGHT_Z 4
#define IDX_DIRECTLIGHT_W 5
#define IDX_DIRECTLIGHT_A 6
#define IDX_RR 7

#define SAMPLE_SIZE 8

#elif defined(PARAM_HAS_PASSTHROUGHT)

#define IDX_PASSTROUGHT 0
#define IDX_BSDF_X 1
#define IDX_BSDF_Y 2
#define IDX_RR 3

#define SAMPLE_SIZE 4

#else

#define IDX_BSDF_X 0
#define IDX_BSDF_Y 1
#define IDX_RR 2

#define SAMPLE_SIZE 3

#endif

#if (PARAM_SAMPLER_TYPE == 0)
#define TOTAL_U_SIZE 2
#endif

#if (PARAM_SAMPLER_TYPE == 1)
#define TOTAL_U_SIZE (IDX_BSDF_OFFSET + PARAM_MAX_PATH_DEPTH * SAMPLE_SIZE)
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

typedef struct {
	PathState state;
	unsigned int depth;

	Spectrum throughput;
	BSDF bsdf;
} PathStateBase;

typedef struct {
	float bouncePdf;
	int specularBounce;

	Ray nextPathRay;
	Spectrum nextThroughput;
	// Radiance to add to the result if light source is visible
	Spectrum lightRadiance;
} PathStateDirectLight;

typedef struct {
	unsigned int vertexCount;
	float alpha;
} PathStateAlphaChannel;

// This is defined only under OpenCL
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	// The task seed
	Seed seed;

	// The set of Samples assigned to this task
	Sample sample;

	// The state used to keep track of the rendered path
	PathStateBase pathStateBase;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	PathStateDirectLight directLightState;
#endif
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	PathStateAlphaChannel alphaChannelState;
#endif
} GPUTask;

#endif

typedef struct {
	unsigned int sampleCount;
} GPUTaskStats;

//------------------------------------------------------------------------------

typedef struct {
	Vector sundir;
	Spectrum gain;
	float turbidity;
	float relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float cosThetaMax;
	Spectrum suncolor;
} SunLight;

typedef struct {
	Spectrum gain;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
} SkyLight;
