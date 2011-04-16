/***************************************************************************
 *   Copyright (C) 1998-2011 by authors (see AUTHORS.txt )                 *
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

// List of symbols defined at compile time:
//  PARAM_TASK_COUNT
//  PARAM_IMAGE_WIDTH
//  PARAM_IMAGE_HEIGHT
//  PARAM_STARTLINE
//  PARAM_RASTER2CAMERA_IJ (Matrix4x4)
//  PARAM_RAY_EPSILON
//  PARAM_CLIP_YON
//  PARAM_CLIP_HITHER
//  PARAM_CAMERA2WORLD_IJ (Matrix4x4)
//  PARAM_SEED
//  PARAM_MAX_PATH_DEPTH
//  PARAM_MAX_RR_DEPTH
//  PARAM_MAX_RR_CAP
//  PARAM_HAS_TEXTUREMAPS
//  PARAM_HAS_ALPHA_TEXTUREMAPS
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_WORLD_RADIUS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH

// To enable single material suopport (work around for ATI compiler problems)
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_AREALIGHT
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_MATTEMIRROR
//  PARAM_ENABLE_MAT_METAL
//  PARAM_ENABLE_MAT_MATTEMETAL
//  PARAM_ENABLE_MAT_ALLOY
//  PARAM_ENABLE_MAT_ARCHGLASS

// (optional)
//  PARAM_DIRECT_LIGHT_SAMPLING
//  PARAM_DL_LIGHT_COUNT

// (optional)
//  PARAM_CAMERA_HAS_DOF
//  PARAM_CAMERA_LENS_RADIUS
//  PARAM_CAMERA_FOCAL_DISTANCE

// (optional)
//  PARAM_HAS_INFINITELIGHT

// (optional, requires PARAM_DIRECT_LIGHT_SAMPLING)
//  PARAM_HAS_SUNLIGHT

// (optional)
//  PARAM_HAS_SKYLIGHT

// (optional)
//  PARAM_IMAGE_FILTER_TYPE (0 = No filter, 1 = Box, 2 = Gaussian, 3 = Mitchell, 4 = MitchellSS)
//  PARAM_IMAGE_FILTER_WIDTH_X
//  PARAM_IMAGE_FILTER_WIDTH_Y
// (Box filter)
// (Gaussian filter)
//  PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA
// (Mitchell filter) & (MitchellSS filter)
//  PARAM_IMAGE_FILTER_MITCHELL_B
//  PARAM_IMAGE_FILTER_MITCHELL_C

// (optional)
//  PARAM_SAMPLER_TYPE (0 = Inlined Random, 1 = Random, 2 = Metropolis, 3 = Stratified)
// (Metropolis)
//  PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE
//  PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT
//  PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY
// (Stratified)
//  PARAM_SAMPLER_STRATIFIED_X_SAMPLES
//  PARAM_SAMPLER_STRATIFIED_Y_SAMPLES

// TODO: IDX_BSDF_Z used only if needed

// TODO: to fix
#define PARAM_STARTLINE 0

//#define PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY 1

#pragma OPENCL EXTENSION cl_amd_printf : enable
#if defined(PARAM_USE_PIXEL_ATOMICS)
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#endif

#if defined(PARAM_HAS_SUNLIGHT) & !defined(PARAM_DIRECT_LIGHT_SAMPLING)
Error: PARAM_HAS_SUNLIGHT requires PARAM_DIRECT_LIGHT_SAMPLING !
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef INV_PI
#define INV_PI  0.31830988618379067154f
#endif

#ifndef INV_TWOPI
#define INV_TWOPI  0.15915494309189533577f
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

typedef struct {
	float u, v;
} UV;

typedef struct {
	float r, g, b;
} Spectrum;

typedef struct {
	float x, y, z;
} Point;

typedef struct {
	float x, y, z;
} Vector;

typedef struct {
	uint v0, v1, v2;
} Triangle;

typedef struct {
	Point o;
	Vector d;
	float mint, maxt;
} Ray;

typedef struct {
	float t;
	float b1, b2; // Barycentric coordinates of the hit point
	uint index;
} RayHit;

//------------------------------------------------------------------------------

typedef struct {
	unsigned int s1, s2, s3;
} Seed;

// Indices of Sample.u[] array
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

#define TOTAL_U_SIZE (IDX_BSDF_OFFSET + PARAM_MAX_PATH_DEPTH * SAMPLE_SIZE)

typedef struct {
	Spectrum radiance;

#if (PARAM_SAMPLER_TYPE == 0)
	uint pixelIndex;

	// Only IDX_SCREEN_X and IDX_SCREEN_Y need to be saved
	float u[2];
#elif (PARAM_SAMPLER_TYPE == 1)
	uint pixelIndex;

	float u[TOTAL_U_SIZE];
#elif (PARAM_SAMPLER_TYPE == 2)
	float totalI;

	// Using ushort here totally freeze the ATI driver
	uint largeMutationCount, smallMutationCount;
	uint current, proposed, consecutiveRejects;

	float weight;
	Spectrum currentRadiance;

	float u[2][TOTAL_U_SIZE];
#elif (PARAM_SAMPLER_TYPE == 3)
	uint pixelIndex;

	float stratifiedScreen2D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES * PARAM_SAMPLER_STRATIFIED_Y_SAMPLES * 2];
#if defined(PARAM_CAMERA_HAS_DOF)
	float stratifiedDof2D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES * PARAM_SAMPLER_STRATIFIED_Y_SAMPLES * 2];
#endif
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
	float stratifiedAlpha1D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES];
#endif

	float stratifiedBSDF2D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES * PARAM_SAMPLER_STRATIFIED_Y_SAMPLES * 2];
	float stratifiedBSDF1D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES];

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	float stratifiedLight2D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES * PARAM_SAMPLER_STRATIFIED_Y_SAMPLES * 2];
	float stratifiedLight1D[PARAM_SAMPLER_STRATIFIED_X_SAMPLES];
#endif

	float u[TOTAL_U_SIZE];
#endif
} Sample;

#define PATH_STATE_GENERATE_EYE_RAY 0
#define PATH_STATE_DONE 1
#define PATH_STATE_NEXT_VERTEX 2
#define PATH_STATE_SAMPLE_LIGHT 3

typedef struct {
	uint state;
	uint depth;
	Spectrum throughput;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	float bouncePdf;
	int specularBounce;

	Ray nextPathRay;
	Spectrum nextThroughput;
	// Radiance to add to the result if light source is visible
	Spectrum lightRadiance;
#endif
} PathState;

// The memory layout of this structure is highly UNoptimized for GPUs !
typedef struct {
	// The task seed
	Seed seed;

	// The set of Samples assigned to this task
	Sample sample;

	// The state used to keep track of the rendered path
	PathState pathState;
} GPUTask;

typedef struct {
	uint sampleCount;
} GPUTaskStats;

typedef struct {
	Spectrum c;
	float count;
} Pixel;

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
	Vector normal;
	float area;
	float gain_r, gain_g, gain_b;
} TriangleLight;

typedef struct {
	float shiftU, shiftV;
	Spectrum gain;
	uint width, height;
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
	uint rgbOffset, alphaOffset;
	uint width, height;
} TexMap;

typedef struct {
	uint vertsOffset;
	uint trisOffset;

	float trans[4][4];
	float invTrans[4][4];
} Mesh;
