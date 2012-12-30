#line 1 "pathocl_kernel_datatypes.cl"

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
	// Only IDX_SCREEN_X and IDX_SCREEN_Y need to be saved
	float u[2];
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
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS) && defined(PARAM_DIRECT_LIGHT_SAMPLING)

#define IDX_TEX_ALPHA 0
#define IDX_BSDF_X 1
#define IDX_BSDF_Y 2
#define IDX_BSDF_Z 3
#define IDX_DIRECTLIGHT_X 4
#define IDX_DIRECTLIGHT_Y 5
#define IDX_DIRECTLIGHT_Z 6
#define IDX_RR 7

#define SAMPLE_SIZE 8

#elif defined(PARAM_HAS_ALPHA_TEXTUREMAPS)

#define IDX_TEX_ALPHA 0
#define IDX_BSDF_X 1
#define IDX_BSDF_Y 2
#define IDX_BSDF_Z 3
#define IDX_RR 4

#define SAMPLE_SIZE 5

#elif defined(PARAM_DIRECT_LIGHT_SAMPLING)

#define IDX_BSDF_X 0
#define IDX_BSDF_Y 1
#define IDX_BSDF_Z 2
#define IDX_DIRECTLIGHT_X 3
#define IDX_DIRECTLIGHT_Y 4
#define IDX_DIRECTLIGHT_Z 5
#define IDX_RR 6

#define SAMPLE_SIZE 7

#else

#define IDX_BSDF_X 0
#define IDX_BSDF_Y 1
#define IDX_BSDF_Z 2
#define IDX_RR 3

#define SAMPLE_SIZE 4

#endif

#if (PARAM_SAMPLER_TYPE == 0)
#define TOTAL_U_SIZE (IDX_BSDF_OFFSET)
#endif

#if (PARAM_SAMPLER_TYPE == 1)
#define TOTAL_U_SIZE (IDX_BSDF_OFFSET + PARAM_MAX_PATH_DEPTH * SAMPLE_SIZE)
#endif

#endif

//------------------------------------------------------------------------------
// GPUTask data types
//------------------------------------------------------------------------------

typedef enum {
	GENERATE_SAMPLE,
	RT_NEXT_VERTEX,
	GENERATE_DL_RAY,
	RT_DL_RAY,
	GENERATE_NEXT_VERTXE_RAY,
	SPLAT_SAMPLE
} PathState;

typedef struct {
	unsigned int state;
	unsigned int depth;

	Spectrum throughput;
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

#define MAT_MATTE 0
#define MAT_AREALIGHT 1
#define MAT_MIRROR 2
#define MAT_GLASS 3
#define MAT_MATTEMIRROR 4
#define MAT_METAL 5
#define MAT_MATTEMETAL 6
#define MAT_ALLOY 7
#define MAT_ARCHGLASS 8
#define MAT_NULL 9

typedef struct {
    float r, g, b;
} MatteParam;

typedef struct {
    float gain_r, gain_g, gain_b;
} AreaLightParam;

typedef struct {
    float r, g, b;
    int specularBounce;
} MirrorParam;

typedef struct {
    float refl_r, refl_g, refl_b;
    float refrct_r, refrct_g, refrct_b;
    float ousideIor, ior;
    float R0;
    int reflectionSpecularBounce, transmitionSpecularBounce;
} GlassParam;

typedef struct {
	MatteParam matte;
	MirrorParam mirror;
	float matteFilter, totFilter, mattePdf, mirrorPdf;
} MatteMirrorParam;

typedef struct {
    float r, g, b;
    float exponent;
    int specularBounce;
} MetalParam;

typedef struct {
	MatteParam matte;
	MetalParam metal;
	float matteFilter, totFilter, mattePdf, metalPdf;
} MatteMetalParam;

typedef struct {
    float diff_r, diff_g, diff_b;
	float refl_r, refl_g, refl_b;
    float exponent;
    float R0;
    int specularBounce;
} AlloyParam;

typedef struct {
    float refl_r, refl_g, refl_b;
    float refrct_r, refrct_g, refrct_b;
	float transFilter, totFilter, reflPdf, transPdf;
	bool reflectionSpecularBounce, transmitionSpecularBounce;
} ArchGlassParam;

typedef struct {
	unsigned int type;
	union {
		MatteParam matte;
        AreaLightParam areaLight;
		MirrorParam mirror;
        GlassParam glass;
		MatteMirrorParam matteMirror;
        MetalParam metal;
        MatteMetalParam matteMetal;
        AlloyParam alloy;
        ArchGlassParam archGlass;
	} param;
} Material;

//------------------------------------------------------------------------------

typedef struct {
	Point v0, v1, v2;
	Normal normal;
	float area;
	float gain_r, gain_g, gain_b;
} TriangleLight;

typedef struct {
	float shiftU, shiftV;
	Spectrum gain;
	unsigned int width, height;
} InfiniteLight;

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

//------------------------------------------------------------------------------

typedef struct {
	unsigned int rgbPage, rgbPageOffset;
	unsigned int alphaPage, alphaPageOffset;
	unsigned int width, height;
} TexMap;

typedef struct {
	float uScale, vScale, uDelta, vDelta;
} TexMapInfo;

typedef struct {
	float uScale, vScale, uDelta, vDelta;
	float scale;
} BumpMapInfo;

typedef struct {
	float uScale, vScale, uDelta, vDelta;
} NormalMapInfo;
