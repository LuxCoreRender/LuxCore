#line 2 "sampler_types.cl"

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
// Indices of Sample related u[] array
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define IDX_SCREEN_X 0
#define IDX_SCREEN_Y 1
#if defined(PARAM_CAMERA_HAS_DOF) && defined(PARAM_HAS_PASSTHROUGH)
#define IDX_EYE_PASSTHROUGH 2
#define IDX_DOF_X 3
#define IDX_DOF_Y 4
#define IDX_BSDF_OFFSET 5
#elif defined(PARAM_CAMERA_HAS_DOF)
#define IDX_DOF_X 2
#define IDX_DOF_Y 3
#define IDX_BSDF_OFFSET 4
#elif defined(PARAM_HAS_PASSTHROUGH)
#define IDX_EYE_PASSTHROUGH 2
#define IDX_BSDF_OFFSET 3
#else
#define IDX_BSDF_OFFSET 2
#endif

// Relative to IDX_BSDF_OFFSET + PathDepth * VERTEX_SAMPLE_SIZE
#if defined(PARAM_HAS_PASSTHROUGH)

#define IDX_PASSTHROUGH 0
#define IDX_BSDF_X 1
#define IDX_BSDF_Y 2
#define IDX_DIRECTLIGHT_X 3
#define IDX_DIRECTLIGHT_Y 4
#define IDX_DIRECTLIGHT_Z 5
#define IDX_DIRECTLIGHT_W 6
#define IDX_DIRECTLIGHT_A 7
#define IDX_RR 8

#define VERTEX_SAMPLE_SIZE 9

#else

#define IDX_BSDF_X 0
#define IDX_BSDF_Y 1
#define IDX_DIRECTLIGHT_X 2
#define IDX_DIRECTLIGHT_Y 3
#define IDX_DIRECTLIGHT_Z 4
#define IDX_DIRECTLIGHT_W 5
#define IDX_RR 6

#define VERTEX_SAMPLE_SIZE 7
#endif

#if (PARAM_SAMPLER_TYPE == 0) || (PARAM_SAMPLER_TYPE == 2)
#define TOTAL_U_SIZE 2
#endif

#if (PARAM_SAMPLER_TYPE == 1)
#define TOTAL_U_SIZE (IDX_BSDF_OFFSET + PARAM_MAX_PATH_DEPTH * VERTEX_SAMPLE_SIZE)
#endif

#endif

//------------------------------------------------------------------------------
// Sample data types
//------------------------------------------------------------------------------

// This is defined only under OpenCL because of variable size structures
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	Spectrum radiance;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	float alpha;
#endif
} RandomSample;

typedef struct {
	Spectrum radiance;
	float alpha;

	float totalI;

	// Using ushort here totally freeze the ATI driver
	unsigned int largeMutationCount, smallMutationCount;
	unsigned int current, proposed, consecutiveRejects;

	float weight;
	Spectrum currentRadiance;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	float currentAlpha;
#endif
} MetropolisSample;

typedef struct {
	float rng0, rng1;
	unsigned int pixelIndex, pass;

	Spectrum radiance;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	float alpha;
#endif
} SobolSample;

#if (PARAM_SAMPLER_TYPE == 0)
typedef RandomSample Sample;
#endif

#if (PARAM_SAMPLER_TYPE == 1)
typedef MetropolisSample Sample;
#endif

#if (PARAM_SAMPLER_TYPE == 2)
typedef SobolSample Sample;
#endif

#endif

//------------------------------------------------------------------------------
// Sampler data types
//------------------------------------------------------------------------------

typedef enum {
	RANDOM = 0,
	METROPOLIS = 1,
	SOBOL = 2
} SamplerType;

typedef struct {
	SamplerType type;
	union {
		struct {
			float largeMutationProbability, imageMutationRange;
			unsigned int maxRejects;
		} metropolis;
	};
} Sampler;

#define SOBOL_BITS 32
#define SOBOL_MAX_DIMENSIONS 21201
