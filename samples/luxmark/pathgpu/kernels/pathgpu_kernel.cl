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

//#pragma OPENCL EXTENSION cl_amd_printf : enable

// List of symbols defined at compile time:
//  PARAM_PATH_COUNT
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
//  PARAM_SAMPLE_PER_PIXEL
//  PARAM_CAMERA_HAS_DOF
//  PARAM_CAMERA_LENS_RADIUS
//  PARAM_CAMERA_FOCAL_DISTANCE
//  PARAM_OPENGL_INTEROP
//  PARAM_DIRECT_LIGHT_SAMPLING
//  PARAM_DL_LIGHT_COUNT
//  PARAM_CAMERA_DYNAMIC
//  PARAM_HAS_TEXTUREMAPS
//  PARAM_HAS_ALPHA_TEXTUREMAPS

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
//  PARAM_HAVE_INFINITELIGHT
//  PARAM_IL_GAIN_R
//  PARAM_IL_GAIN_G
//  PARAM_IL_GAIN_B
//  PARAM_IL_SHIFT_U
//  PARAM_IL_SHIFT_V
//  PARAM_IL_WIDTH
//  PARAM_IL_HEIGHT

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

typedef struct {
	uint s1, s2, s3;
} Seed;

#define PATH_STATE_NEXT_VERTEX 0
#define PATH_STATE_SAMPLE_LIGHT 1
#define PATH_STATE_CONTINUE_NEXT_VERTEX 2

typedef struct {
	Spectrum throughput;
	uint depth, pixelIndex, subpixelIndex;
	Seed seed;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	int specularBounce;
	int state;

	Ray pathRay;
	RayHit pathHit;
	Spectrum lightRadiance;
	Spectrum accumRadiance;
#endif
} Path;

typedef struct {
	Spectrum c;
	uint count;
} Pixel;

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

typedef struct {
	Point v0, v1, v2;
	Vector normal;
	float area;
	float gain_r, gain_g, gain_b;
} TriangleLight;

typedef struct {
	unsigned int rgbOffset, alphaOffset;
	unsigned int width, height;
} TexMap;

//------------------------------------------------------------------------------
// Random number generator
// maximally equidistributed combined Tausworthe generator
//------------------------------------------------------------------------------

#define FLOATMASK 0x00ffffffu

uint TAUSWORTHE(const uint s, const uint a,
	const uint b, const uint c,
	const uint d) {
	return ((s&c)<<d) ^ (((s << a) ^ s) >> b);
}

uint LCG(const uint x) { return x * 69069; }

uint ValidSeed(const uint x, const uint m) {
	return (x < m) ? (x + m) : x;
}

void InitRandomGenerator(uint seed, Seed *s) {
	// Avoid 0 value
	seed = (seed == 0) ? (seed + 0xffffffu) : seed;

	s->s1 = ValidSeed(LCG(seed), 1);
	s->s2 = ValidSeed(LCG(s->s1), 7);
	s->s3 = ValidSeed(LCG(s->s2), 15);
}

unsigned long RndUintValue(Seed *s) {
	s->s1 = TAUSWORTHE(s->s1, 13, 19, 4294967294UL, 12);
	s->s2 = TAUSWORTHE(s->s2, 2, 25, 4294967288UL, 4);
	s->s3 = TAUSWORTHE(s->s3, 3, 11, 4294967280UL, 17);

	return ((s->s1) ^ (s->s2) ^ (s->s3));
}

float RndFloatValue(Seed *s) {
	return (RndUintValue(s) & FLOATMASK) * (1.f / (FLOATMASK + 1UL));
}

//------------------------------------------------------------------------------

float Dot(const Vector *v0, const Vector *v1) {
	return v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;
}

void Normalize(Vector *v) {
	const float il = 1.f / sqrt(Dot(v, v));

	v->x *= il;
	v->y *= il;
	v->z *= il;
}

void Cross(Vector *v3, const Vector *v1, const Vector *v2) {
	v3->x = (v1->y * v2->z) - (v1->z * v2->y);
	v3->y = (v1->z * v2->x) - (v1->x * v2->z),
	v3->z = (v1->x * v2->y) - (v1->y * v2->x);
}

void ConcentricSampleDisk(const float u1, const float u2, float *dx, float *dy) {
	float r, theta;
	// Map uniform random numbers to $[-1,1]^2$
	float sx = 2.f * u1 - 1.f;
	float sy = 2.f * u2 - 1.f;
	// Map square to $(r,\theta)$
	// Handle degeneracy at the origin
	if (sx == 0.f && sy == 0.f) {
		*dx = 0.f;
		*dy = 0.f;
		return;
	}
	if (sx >= -sy) {
		if (sx > sy) {
			// Handle first region of disk
			r = sx;
			if (sy > 0.f)
				theta = sy / r;
			else
				theta = 8.f + sy / r;
		} else {
			// Handle second region of disk
			r = sy;
			theta = 2.f - sx / r;
		}
	} else {
		if (sx <= sy) {
			// Handle third region of disk
			r = -sx;
			theta = 4.f - sy / r;
		} else {
			// Handle fourth region of disk
			r = -sy;
			theta = 6.f + sx / r;
		}
	}
	theta *= M_PI / 4.f;
	*dx = r * cos(theta);
	*dy = r * sin(theta);
}

void CosineSampleHemisphere(Vector *ret, const float u1, const float u2) {
	ConcentricSampleDisk(u1, u2, &ret->x, &ret->y);
	ret->z = sqrt(max(0.f, 1.f - ret->x * ret->x - ret->y * ret->y));
}

void CoordinateSystem(const Vector *v1, Vector *v2, Vector *v3) {
	if (fabs(v1->x) > fabs(v1->y)) {
		float invLen = 1.f / sqrt(v1->x * v1->x + v1->z * v1->z);
		v2->x = -v1->z * invLen;
		v2->y = 0.f;
		v2->z = v1->x * invLen;
	} else {
		float invLen = 1.f / sqrt(v1->y * v1->y + v1->z * v1->z);
		v2->x = 0.f;
		v2->y = v1->z * invLen;
		v2->z = -v1->y * invLen;
	}

	Cross(v3, v1, v2);
}

//------------------------------------------------------------------------------

void GenerateRay(
		const uint pixelIndex,
		__global Ray *ray, Seed *seed
#if defined(PARAM_CAMERA_DYNAMIC)
		, __global float *cameraData
#endif
		) {

	/*// Gaussina distribution
	const float rad = filterRadius * sqrt(-log(1.f - RndFloatValue(seed)));
	const float angle = 2 * M_PI * RndFloatValue(seed);

	const float screenX = pixelIndex % PARAM_IMAGE_WIDTH + rad * cos(angle);
	const float screenY = pixelIndex / PARAM_IMAGE_WIDTH + rad * sin(angle);*/

	const float screenX = pixelIndex % PARAM_IMAGE_WIDTH + RndFloatValue(seed) - 0.5f;
	const float screenY = pixelIndex / PARAM_IMAGE_WIDTH + RndFloatValue(seed) - 0.5f;

	Point Pras;
	Pras.x = screenX;
	Pras.y = PARAM_IMAGE_HEIGHT - screenY - 1.f;
	Pras.z = 0;

	Point orig;
	// RasterToCamera(Pras, &orig);
#if defined(PARAM_CAMERA_DYNAMIC)
	const float iw = 1.f / (cameraData[12] * Pras.x + cameraData[13] * Pras.y + cameraData[14] * Pras.z + cameraData[15]);
	orig.x = (cameraData[0] * Pras.x + cameraData[1] * Pras.y + cameraData[2] * Pras.z + cameraData[3]) * iw;
	orig.y = (cameraData[4] * Pras.x + cameraData[5] * Pras.y + cameraData[6] * Pras.z + cameraData[7]) * iw;
	orig.z = (cameraData[8] * Pras.x + cameraData[9] * Pras.y + cameraData[10] * Pras.z + cameraData[11]) * iw;
#else
	const float iw = 1.f / (PARAM_RASTER2CAMERA_30 * Pras.x + PARAM_RASTER2CAMERA_31 * Pras.y + PARAM_RASTER2CAMERA_32 * Pras.z + PARAM_RASTER2CAMERA_33);
	orig.x = (PARAM_RASTER2CAMERA_00 * Pras.x + PARAM_RASTER2CAMERA_01 * Pras.y + PARAM_RASTER2CAMERA_02 * Pras.z + PARAM_RASTER2CAMERA_03) * iw;
	orig.y = (PARAM_RASTER2CAMERA_10 * Pras.x + PARAM_RASTER2CAMERA_11 * Pras.y + PARAM_RASTER2CAMERA_12 * Pras.z + PARAM_RASTER2CAMERA_13) * iw;
	orig.z = (PARAM_RASTER2CAMERA_20 * Pras.x + PARAM_RASTER2CAMERA_21 * Pras.y + PARAM_RASTER2CAMERA_22 * Pras.z + PARAM_RASTER2CAMERA_23) * iw;
#endif

	Vector dir;
	dir.x = orig.x;
	dir.y = orig.y;
	dir.z = orig.z;

#if defined(PARAM_CAMERA_HAS_DOF)
	// Sample point on lens
	float lensU, lensV;
	ConcentricSampleDisk(RndFloatValue(seed), RndFloatValue(seed), &lensU, &lensV);
	lensU *= PARAM_CAMERA_LENS_RADIUS;
	lensV *= PARAM_CAMERA_LENS_RADIUS;

	// Compute point on plane of focus
	const float ft = (PARAM_CAMERA_FOCAL_DISTANCE - PARAM_CLIP_HITHER) / dir.z;
	Point Pfocus;
	Pfocus.x = orig.x + dir.x * ft;
	Pfocus.y = orig.y + dir.y * ft;
	Pfocus.z = orig.z + dir.z * ft;

	// Update ray for effect of lens
	orig.x += lensU * ((PARAM_CAMERA_FOCAL_DISTANCE - PARAM_CLIP_HITHER) / PARAM_CAMERA_FOCAL_DISTANCE);
	orig.y += lensV * ((PARAM_CAMERA_FOCAL_DISTANCE - PARAM_CLIP_HITHER) / PARAM_CAMERA_FOCAL_DISTANCE);

	dir.x = Pfocus.x - orig.x;
	dir.y = Pfocus.y - orig.y;
	dir.z = Pfocus.z - orig.z;
#endif

	Normalize(&dir);

	// CameraToWorld(*ray, ray);
	Point torig;
#if defined(PARAM_CAMERA_DYNAMIC)
	const float iw2 = 1.f / (cameraData[16 + 12] * orig.x + cameraData[16 + 13] * orig.y + cameraData[16 + 14] * orig.z + cameraData[16 + 15]);
	torig.x = (cameraData[16 + 0] * orig.x + cameraData[16 + 1] * orig.y + cameraData[16 + 2] * orig.z + cameraData[16 + 3]) * iw2;
	torig.y = (cameraData[16 + 4] * orig.x + cameraData[16 + 5] * orig.y + cameraData[16 + 6] * orig.z + cameraData[16 + 7]) * iw2;
	torig.z = (cameraData[16 + 8] * orig.x + cameraData[16 + 9] * orig.y + cameraData[16 + 12] * orig.z + cameraData[16 + 11]) * iw2;
#else
	const float iw2 = 1.f / (PARAM_CAMERA2WORLD_30 * orig.x + PARAM_CAMERA2WORLD_31 * orig.y + PARAM_CAMERA2WORLD_32 * orig.z + PARAM_CAMERA2WORLD_33);
	torig.x = (PARAM_CAMERA2WORLD_00 * orig.x + PARAM_CAMERA2WORLD_01 * orig.y + PARAM_CAMERA2WORLD_02 * orig.z + PARAM_CAMERA2WORLD_03) * iw2;
	torig.y = (PARAM_CAMERA2WORLD_10 * orig.x + PARAM_CAMERA2WORLD_11 * orig.y + PARAM_CAMERA2WORLD_12 * orig.z + PARAM_CAMERA2WORLD_13) * iw2;
	torig.z = (PARAM_CAMERA2WORLD_20 * orig.x + PARAM_CAMERA2WORLD_21 * orig.y + PARAM_CAMERA2WORLD_22 * orig.z + PARAM_CAMERA2WORLD_23) * iw2;
#endif

	Vector tdir;
#if defined(PARAM_CAMERA_DYNAMIC)
	tdir.x = cameraData[16 + 0] * dir.x + cameraData[16 + 1] * dir.y + cameraData[16 + 2] * dir.z;
	tdir.y = cameraData[16 + 4] * dir.x + cameraData[16 + 5] * dir.y + cameraData[16 + 6] * dir.z;
	tdir.z = cameraData[16 + 8] * dir.x + cameraData[16 + 9] * dir.y + cameraData[16 + 10] * dir.z;
#else
	tdir.x = PARAM_CAMERA2WORLD_00 * dir.x + PARAM_CAMERA2WORLD_01 * dir.y + PARAM_CAMERA2WORLD_02 * dir.z;
	tdir.y = PARAM_CAMERA2WORLD_10 * dir.x + PARAM_CAMERA2WORLD_11 * dir.y + PARAM_CAMERA2WORLD_12 * dir.z;
	tdir.z = PARAM_CAMERA2WORLD_20 * dir.x + PARAM_CAMERA2WORLD_21 * dir.y + PARAM_CAMERA2WORLD_22 * dir.z;
#endif

	ray->o = torig;
	ray->d = tdir;
	ray->mint = PARAM_RAY_EPSILON;
	ray->maxt = (PARAM_CLIP_YON - PARAM_CLIP_HITHER) / dir.z;
}

//------------------------------------------------------------------------------

__kernel void Init(
		__global Path *paths,
		__global Ray *rays
#if defined(PARAM_CAMERA_DYNAMIC)
		, __global float *cameraData
#endif
		) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_PATH_COUNT)
		return;

	// Initialize the path
	__global Path *path = &paths[gid];
	path->throughput.r = 1.f;
	path->throughput.g = 1.f;
	path->throughput.b = 1.f;
	path->depth = 0;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	path->specularBounce = TRUE;
	path->state = PATH_STATE_NEXT_VERTEX;
	path->accumRadiance.r = 0.f;
	path->accumRadiance.g = 0.f;
	path->accumRadiance.b = 0.f;
#endif

	const uint pixelIndex = (PARAM_STARTLINE * PARAM_IMAGE_WIDTH + gid) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
	path->pixelIndex = pixelIndex;
	path->subpixelIndex = 0;

	// Initialize random number generator
	Seed seed;
	InitRandomGenerator(PARAM_SEED + gid, &seed);

	// Generate the eye ray
	GenerateRay(pixelIndex, &rays[gid], &seed
#if defined(PARAM_CAMERA_DYNAMIC)
		, cameraData
#endif
		);

	// Save the seed
	path->seed.s1 = seed.s1;
	path->seed.s2 = seed.s2;
	path->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------

__kernel void InitFrameBuffer(
		__global Pixel *frameBuffer
#if defined(PARAM_OPENGL_INTEROP)
		, const int clearPBO
		, __global uint *pbo
#endif
		) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT)
		return;

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0;

#if defined(PARAM_OPENGL_INTEROP)
	if (clearPBO)
		pbo[gid] = 0;
#endif
}

#if defined(PARAM_OPENGL_INTEROP)
uint Radiance2PixelUInt(const float x) {
	return (uint)(pow(clamp(x, 0.f, 1.f), 1.f / 2.2f) * 255.f + .5f);
}

__kernel void UpdatePixelBuffer(
		__global Pixel *frameBuffer,
		__global uint *pbo) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT)
		return;

	__global Pixel *p = &frameBuffer[gid];

	const uint count = p->count;
	if (count > 0) {
		const float invCount = 1.f / p->count;

		const uint r = Radiance2PixelUInt(p->c.r * invCount);
		const uint g = Radiance2PixelUInt(p->c.g * invCount);
		const uint b = Radiance2PixelUInt(p->c.b * invCount);
		pbo[gid] = r | (g << 8) | (b << 16);
	}
}

void AccumPixel(__global Pixel *p, float *r, float *g, float *b, const float k) {
	const uint count = p->count;

	if (count > 0) {
		const float invCount = k / p->count;

		*r += p->c.r * invCount;
		*g += p->c.g * invCount;
		*b += p->c.b * invCount;
	}
}

__kernel void UpdatePixelBufferBlured(
		__global Pixel *frameBuffer,
		__global uint *pbo) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT)
		return;

	__global Pixel *p = &frameBuffer[gid];

	const uint count = p->count;
	if (count > 0) {
		// Naive Gaussian Filter (i.e. not using local memory, etc.)

		const unsigned int x = gid % PARAM_IMAGE_WIDTH;
		const unsigned int y = gid / PARAM_IMAGE_WIDTH;

		float r = 0.f;
		float g = 0.f;
		float b = 0.f;

		if (y > 0) {
			if (x > 0) AccumPixel(&frameBuffer[gid - PARAM_IMAGE_WIDTH - 1], &r, &g, &b, 1.f / 16.f);
			AccumPixel(&frameBuffer[gid - PARAM_IMAGE_WIDTH], &r, &g, &b, 2.f / 16.f);
			if (x < PARAM_IMAGE_WIDTH - 1) AccumPixel(&frameBuffer[gid - PARAM_IMAGE_WIDTH + 1], &r, &g, &b, 1.f / 16.f);
		}

		if (x > 0) AccumPixel(&frameBuffer[gid - 1], &r, &g, &b, 2.f / 16.f);
		AccumPixel(&frameBuffer[gid], &r, &g, &b, 4.f / 16.f);
		if (x < PARAM_IMAGE_WIDTH - 1) AccumPixel(&frameBuffer[gid + 1], &r, &g, &b, 2.f / 16.f);

		if (y < PARAM_IMAGE_HEIGHT - 1) {
			if (x > 0) AccumPixel(&frameBuffer[gid + PARAM_IMAGE_WIDTH - 1], &r, &g, &b, 1.f / 16.f);
			AccumPixel(&frameBuffer[gid + PARAM_IMAGE_WIDTH], &r, &g, &b, 2.f / 16.f);
			if (x < PARAM_IMAGE_WIDTH - 1) AccumPixel(&frameBuffer[gid + PARAM_IMAGE_WIDTH + 1], &r, &g, &b, 1.f / 16.f);
		}

		const uint pr = Radiance2PixelUInt(r);
		const uint pg = Radiance2PixelUInt(g);
		const uint pb = Radiance2PixelUInt(b);
		pbo[gid] = pr | (pg << 8) | (pb << 16);
	}
}

#endif

//------------------------------------------------------------------------------

int Mod(int a, int b) {
	if (b == 0)
		b = 1;

	a %= b;
	if (a < 0)
		a += b;

	return a;
}

void TexMap_GetTexel(__global Spectrum *pixels, const uint width, const uint height,
		const int s, const int t, Spectrum *col) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const unsigned index = v * width + u;

	col->r = pixels[index].r;
	col->g = pixels[index].g;
	col->b = pixels[index].b;
}

float TexMap_GetAlphaTexel(__global float *alphas, const uint width, const uint height,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const unsigned index = v * width + u;

	return alphas[index];
}

void TexMap_GetColor(__global Spectrum *pixels, const uint width, const uint height,
		const float u, const float v, Spectrum *col) {
	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = (int)floor(s);
	const int t0 = (int)floor(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	Spectrum c0, c1, c2, c3;
	TexMap_GetTexel(pixels, width, height, s0, t0, &c0);
	TexMap_GetTexel(pixels, width, height, s0, t0 + 1, &c1);
	TexMap_GetTexel(pixels, width, height, s0 + 1, t0, &c2);
	TexMap_GetTexel(pixels, width, height, s0 + 1, t0 + 1, &c3);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	col->r = k0 * c0.r + k1 * c1.r + k2 * c2.r + k3 * c3.r;
	col->g = k0 * c0.g + k1 * c1.g + k2 * c2.g + k3 * c3.g;
	col->b = k0 * c0.b + k1 * c1.b + k2 * c2.b + k3 * c3.b;
}

float TexMap_GetAlpha(__global float *alphas, const uint width, const uint height,
		const float u, const float v) {
	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = (int)floor(s);
	const int t0 = (int)floor(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float c0 = TexMap_GetAlphaTexel(alphas, width, height, s0, t0);
	const float c1 = TexMap_GetAlphaTexel(alphas, width, height, s0, t0 + 1);
	const float c2 = TexMap_GetAlphaTexel(alphas, width, height, s0 + 1, t0);
	const float c3 = TexMap_GetAlphaTexel(alphas, width, height, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return k0 * c0 + k1 * c1 + k2 * c2 + k3 * c3;
}

float SphericalTheta(const Vector *v) {
	return acos(clamp(v->z, -1.f, 1.f));
}

float SphericalPhi(const Vector *v) {
	float p = atan2(v->y, v->x);
	return (p < 0.f) ? p + 2.f * M_PI : p;
}

#if defined(PARAM_HAVE_INFINITELIGHT)
void InfiniteLight_Le(__global Spectrum *infiniteLightMap, Spectrum *le, Vector *dir) {
	const float u = 1.f - SphericalPhi(dir) * INV_TWOPI +  PARAM_IL_SHIFT_U;
	const float v = SphericalTheta(dir) * INV_PI + PARAM_IL_SHIFT_V;

	TexMap_GetColor(infiniteLightMap, PARAM_IL_WIDTH, PARAM_IL_HEIGHT, u, v, le);

	le->r *= PARAM_IL_GAIN_R;
	le->g *= PARAM_IL_GAIN_G;
	le->b *= PARAM_IL_GAIN_B;
}
#endif

void Mesh_InterpolateColor(__global Spectrum *colors, __global Triangle *triangles,
		const uint triIndex, const float b1, const float b2, Spectrum *C) {
	__global Triangle *tri = &triangles[triIndex];

	const float b0 = 1.f - b1 - b2;
	C->r = b0 * colors[tri->v0].r + b1 * colors[tri->v1].r + b2 * colors[tri->v2].r;
	C->g = b0 * colors[tri->v0].g + b1 * colors[tri->v1].g + b2 * colors[tri->v2].g;
	C->b = b0 * colors[tri->v0].b + b1 * colors[tri->v1].b + b2 * colors[tri->v2].b;
}

void Mesh_InterpolateNormal(__global Vector *normals, __global Triangle *triangles,
		const uint triIndex, const float b1, const float b2, Vector *N) {
	__global Triangle *tri = &triangles[triIndex];

	const float b0 = 1.f - b1 - b2;
	N->x = b0 * normals[tri->v0].x + b1 * normals[tri->v1].x + b2 * normals[tri->v2].x;
	N->y = b0 * normals[tri->v0].y + b1 * normals[tri->v1].y + b2 * normals[tri->v2].y;
	N->z = b0 * normals[tri->v0].z + b1 * normals[tri->v1].z + b2 * normals[tri->v2].z;

	Normalize(N);
}

void Mesh_InterpolateUV(__global UV *uvs, __global Triangle *triangles,
		const uint triIndex, const float b1, const float b2, UV *uv) {
	__global Triangle *tri = &triangles[triIndex];

	const float b0 = 1.f - b1 - b2;
	uv->u = b0 * uvs[tri->v0].u + b1 * uvs[tri->v1].u + b2 * uvs[tri->v2].u;
	uv->v = b0 * uvs[tri->v0].v + b1 * uvs[tri->v1].v + b2 * uvs[tri->v2].v;
}

//------------------------------------------------------------------------------
// Materials
//------------------------------------------------------------------------------

void Matte_Sample_f(__global MatteParam *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN,
		const float u0, const float u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
		) {
	Vector dir;
	CosineSampleHemisphere(&dir, u0, u1);
	*pdf = dir.z * INV_PI;

	Vector v1, v2;
	CoordinateSystem(shadeN, &v1, &v2);

	wi->x = v1.x * dir.x + v2.x * dir.y + shadeN->x * dir.z;
	wi->y = v1.y * dir.x + v2.y * dir.y + shadeN->y * dir.z;
	wi->z = v1.z * dir.x + v2.z * dir.y + shadeN->z * dir.z;

	const float dp = Dot(shadeN, wi);
	// Using 0.0001 instead of 0.0 to cut down fireflies
	if (dp <= 0.0001f)
		*pdf = 0.f;
	else {
		*pdf /=  dp;

		f->r = mat->r * INV_PI;
		f->g = mat->g * INV_PI;
		f->b = mat->b * INV_PI;
	}

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	*specularBounce = FALSE;
#endif
}

void Mirror_Sample_f(__global MirrorParam *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
		) {
    const float k = 2.f * Dot(shadeN, wo);
	wi->x = k * shadeN->x - wo->x;
	wi->y = k * shadeN->y - wo->y;
	wi->z = k * shadeN->z - wo->z;

	*pdf = 1.f;

	f->r = mat->r;
	f->g = mat->g;
	f->b = mat->b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	*specularBounce = mat->specularBounce;
#endif
}

void Glass_Sample_f(__global GlassParam *mat,
    const Vector *wo, Vector *wi, float *pdf, Spectrum *f, const Vector *N, const Vector *shadeN,
    const float u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
        ) {
    Vector reflDir;
    const float k = 2.f * Dot(N, wo);
    reflDir.x = k * N->x - wo->x;
    reflDir.y = k * N->y - wo->y;
    reflDir.z = k * N->z - wo->z;

    // Ray from outside going in ?
    const bool into = (Dot(N, shadeN) > 0.f);

    const float nc = mat->ousideIor;
    const float nt = mat->ior;
    const float nnt = into ? (nc / nt) : (nt / nc);
    const float ddn = -Dot(wo, shadeN);
    const float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);

    // Total internal reflection
    if (cos2t < 0.f) {
        *wi = reflDir;
        *pdf = 1.f;

        f->r = mat->refl_r;
        f->g = mat->refl_g;
        f->b = mat->refl_b;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
        *specularBounce = mat->reflectionSpecularBounce;
#endif
    } else {
        const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrt(cos2t));
        Vector nkk = *N;
        nkk.x *= kk;
        nkk.y *= kk;
        nkk.z *= kk;

        Vector transDir;
        transDir.x = -nnt * wo->x - nkk.x;
        transDir.y = -nnt * wo->y - nkk.y;
        transDir.z = -nnt * wo->z - nkk.z;
        Normalize(&transDir);

        const float c = 1.f - (into ? -ddn : Dot(&transDir, N));

        const float R0 = mat->R0;
        const float Re = R0 + (1.f - R0) * c * c * c * c * c;
        const float Tr = 1.f - Re;
        const float P = .25f + .5f * Re;

        if (Tr == 0.f) {
            if (Re == 0.f)
                *pdf = 0.f;
            else {
                *wi = reflDir;
                *pdf = 1.f;

                f->r = mat->refl_r;
                f->g = mat->refl_g;
                f->b = mat->refl_b;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
                *specularBounce = mat->reflectionSpecularBounce;
#endif
            }
        } else if (Re == 0.f) {
            *wi = transDir;
            *pdf = 1.f;

            f->r = mat->refrct_r;
            f->g = mat->refrct_g;
            f->b = mat->refrct_b;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = mat->transmitionSpecularBounce;
#endif
        } else if (u0 < P) {
            *wi = reflDir;
            *pdf = P / Re;

            f->r = mat->refl_r;
            f->g = mat->refl_g;
            f->b = mat->refl_b;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = mat->reflectionSpecularBounce;
#endif
        } else {
            *wi = transDir;
            *pdf = (1.f - P) / Tr;

            f->r = mat->refrct_r;
            f->g = mat->refrct_g;
            f->b = mat->refrct_b;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = mat->transmitionSpecularBounce;
#endif
        }
    }
}

void MatteMirror_Sample_f(__global MatteMirrorParam *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN,
		const float u0, const float u1, const float u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
		) {
    const float totFilter = mat->totFilter;
    const float comp = u2 * totFilter;

    if (comp > mat->matteFilter) {
        Mirror_Sample_f(&mat->mirror, wo, wi, pdf, f, shadeN
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            , specularBounce
#endif
            );
        *pdf *= mat->mirrorPdf;
    } else {
        Matte_Sample_f(&mat->matte, wo, wi, pdf, f, shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            , specularBounce
#endif
            );
        *pdf *= mat->mattePdf;
    }
}

void GlossyReflection(const Vector *wo, Vector *wi, const float exponent,
		const Vector *shadeN, const float u0, const float u1) {
    const float phi = 2.f * M_PI * u0;
    const float cosTheta = pow(1.f - u1, exponent);
    const float sinTheta = sqrt(1.f - cosTheta * cosTheta);
    const float x = cos(phi) * sinTheta;
    const float y = sin(phi) * sinTheta;
    const float z = cosTheta;

    Vector w;
    const float RdotShadeN = Dot(shadeN, wo);
	w.x = (2.f * RdotShadeN) * shadeN->x - wo->x;
	w.y = (2.f * RdotShadeN) * shadeN->y - wo->y;
	w.z = (2.f * RdotShadeN) * shadeN->z - wo->z;

    Vector u, a;
    if (fabs(shadeN->x) > .1f) {
        a.x = 0.f;
        a.y = 1.f;
    } else {
        a.x = 1.f;
        a.y = 0.f;
    }
    a.z = 0.f;
    Cross(&u, &a, &w);
    Normalize(&u);
    Vector v;
    Cross(&v, &w, &u);

    wi->x = x * u.x + y * v.x + z * w.x;
    wi->y = x * u.y + y * v.y + z * w.y;
    wi->z = x * u.z + y * v.z + z * w.z;
}

void Metal_Sample_f(__global MetalParam *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN,
		const float u0, const float u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
		) {
        GlossyReflection(wo, wi, mat->exponent, shadeN, u0, u1);

		if (Dot(wi, shadeN) > 0.f) {
			*pdf = 1.f;

            f->r = mat->r;
            f->g = mat->g;
            f->b = mat->b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = mat->specularBounce;
#endif
		} else
			*pdf = 0.f;
}

void MatteMetal_Sample_f(__global MatteMetalParam *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN,
		const float u0, const float u1, const float u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
		) {
        const float totFilter = mat->totFilter;
        const float comp = u2 * totFilter;

		if (comp > mat->matteFilter) {
            Metal_Sample_f(&mat->metal, wo, wi, pdf, f, shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
                , specularBounce
#endif
                );
			*pdf *= mat->metalPdf;
		} else {
            Matte_Sample_f(&mat->matte, wo, wi, pdf, f, shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
                , specularBounce
#endif
                );
			*pdf *= mat->mattePdf;
		}
}

void Alloy_Sample_f(__global AlloyParam *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN,
		const float u0, const float u1, const float u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
		) {
    // Schilick's approximation
    const float c = 1.f - Dot(wo, shadeN);
    const float R0 = mat->R0;
    const float Re = R0 + (1.f - R0) * c * c * c * c * c;

    const float P = .25f + .5f * Re;

    if (u2 < P) {
        GlossyReflection(wo, wi, mat->exponent, shadeN, u0, u1);
        *pdf = P / Re;

        f->r = Re * mat->refl_r;
        f->g = Re * mat->refl_g;
        f->b = Re * mat->refl_b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
        *specularBounce = mat->specularBounce;
#endif
    } else {
        Vector dir;
        CosineSampleHemisphere(&dir, u0, u1);
        *pdf = dir.z * INV_PI;

        Vector v1, v2;
        CoordinateSystem(shadeN, &v1, &v2);

        wi->x = v1.x * dir.x + v2.x * dir.y + shadeN->x * dir.z;
        wi->y = v1.y * dir.x + v2.y * dir.y + shadeN->y * dir.z;
        wi->z = v1.z * dir.x + v2.z * dir.y + shadeN->z * dir.z;

        const float dp = Dot(shadeN, wi);
        // Using 0.0001 instead of 0.0 to cut down fireflies
        if (dp <= 0.0001f)
            *pdf = 0.f;
        else {
            *pdf /=  dp;

            const float iRe = 1.f - Re;
            *pdf *= (1.f - P) / iRe;

            f->r = iRe * mat->diff_r;
            f->g = iRe * mat->diff_g;
            f->b = iRe * mat->diff_b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = FALSE;
#endif
        }
    }
}

void ArchGlass_Sample_f(__global ArchGlassParam *mat,
    const Vector *wo, Vector *wi, float *pdf, Spectrum *f, const Vector *N, const Vector *shadeN,
    const float u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global int *specularBounce
#endif
        ) {
    // Ray from outside going in ?
    const bool into = (Dot(N, shadeN) > 0.f);

    if (!into) {
        // No internal reflections
        wi->x = -wo->x;
        wi->y = -wo->y;
        wi->z = -wo->z;
        *pdf = 1.f;

        f->r = mat->refrct_r;
        f->g = mat->refrct_g;
        f->b = mat->refrct_b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
        *specularBounce = mat->transmitionSpecularBounce;
#endif
    } else {
        // RR to choose if reflect the ray or go trough the glass
        const float comp = u0 * mat->totFilter;

        if (comp > mat->transFilter) {
            const float k = 2.f * Dot(N, wo);
            wi->x = k * N->x - wo->x;
            wi->y = k * N->y - wo->y;
            wi->z = k * N->z - wo->z;
            *pdf =  mat->reflPdf;

            f->r = mat->refl_r;
            f->g = mat->refl_g;
            f->b = mat->refl_b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = mat->reflectionSpecularBounce;
#endif
        } else {
            wi->x = -wo->x;
            wi->y = -wo->y;
            wi->z = -wo->z;
            *pdf =  mat->transPdf;

            f->r = mat->refrct_r;
            f->g = mat->refrct_g;
            f->b = mat->refrct_b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
            *specularBounce = mat->transmitionSpecularBounce;
#endif
        }
    }
}

//------------------------------------------------------------------------------
// Lights
//------------------------------------------------------------------------------

void AreaLight_Le(__global AreaLightParam *mat, const Vector *wo, const Vector *lightN, Spectrum *Le) {
	if (Dot(lightN, wo) > 0.f) {
        Le->r = mat->gain_r;
        Le->g = mat->gain_g;
        Le->b = mat->gain_b;
    }
}

void SampleTriangleLight(__global TriangleLight *light,	const float u0, const float u1, Point *p) {
	Point v0, v1, v2;
	v0 = light->v0;
	v1 = light->v1;
	v2 = light->v2;

	// UniformSampleTriangle(u0, u1, b0, b1);
	const float su1 = sqrt(u0);
	const float b0 = 1.f - su1;
	const float b1 = u1 * su1;
	const float b2 = 1.f - b0 - b1;

	p->x = b0 * v0.x + b1 * v1.x + b2 * v2.x;
	p->y = b0 * v0.y + b1 * v1.y + b2 * v2.y;
	p->z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
}

void TriangleLight_Sample_L(__global TriangleLight *l,
		const Vector *wo, const Point *hitPoint,
		float *pdf, Spectrum *f, Ray *shadowRay,
		const float u0, const float u1, const float u2) {
	Point samplePoint;
	SampleTriangleLight(l, u0, u1, &samplePoint);

	shadowRay->d.x = samplePoint.x - hitPoint->x;
	shadowRay->d.y = samplePoint.y - hitPoint->y;
	shadowRay->d.z = samplePoint.z - hitPoint->z;
	const float distanceSquared = Dot(&shadowRay->d, &shadowRay->d);
	const float distance = sqrt(distanceSquared);
	const float invDistance = 1.f / distance;
	shadowRay->d.x *= invDistance;
	shadowRay->d.y *= invDistance;
	shadowRay->d.z *= invDistance;

	Vector sampleN = l->normal;
	const float sampleNdotMinusWi = -Dot(&sampleN, &shadowRay->d);
	if (sampleNdotMinusWi <= 0.f)
		*pdf = 0.f;
	else {
		*pdf = distanceSquared / (sampleNdotMinusWi * l->area);

		// Using 0.1 instead of 0.0 to cut down fireflies
		if (*pdf <= 0.1f)
			*pdf = 0.f;
		else {
            shadowRay->o = *hitPoint;
            shadowRay->mint = PARAM_RAY_EPSILON;
            shadowRay->maxt = distance - PARAM_RAY_EPSILON;

            f->r = l->gain_r;
            f->g = l->gain_g;
            f->b = l->gain_b;
        }
	}
}

//------------------------------------------------------------------------------

void TerminatePath(__global Path *path, __global Ray *ray, __global Pixel *frameBuffer, Seed *seed, Spectrum *radiance
#if defined(PARAM_CAMERA_DYNAMIC)
		, __global float *cameraData
#endif
		) {
	// Add sample to the framebuffer

	const uint pixelIndex = path->pixelIndex;
	__global Pixel *pixel = &frameBuffer[pixelIndex];

	pixel->c.r += isnan(radiance->r) ? 0.f : radiance->r;
	pixel->c.g += isnan(radiance->g) ? 0.f : radiance->g;
	pixel->c.b += isnan(radiance->b) ? 0.f : radiance->b;
	pixel->count += 1;

    // Re-initialize the path

	uint newPixelIndex;
#if (PARAM_SAMPLE_PER_PIXEL > 0)
	newPixelIndex = (pixelIndex + PARAM_PATH_COUNT) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
	path->pixelIndex = newPixelIndex;
#else
	const uint subpixelIndex = path->subpixelIndex;
	if (subpixelIndex >= PARAM_SAMPLE_PER_PIXEL) {
		newPixelIndex = (pixelIndex + PARAM_PATH_COUNT) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
		path->pixelIndex = newPixelIndex;
		path->subpixelIndex = 0;
	} else {
		newPixelIndex = pixelIndex;
		path->subpixelIndex = subpixelIndex + 1;
	}
#endif

	GenerateRay(newPixelIndex, ray, seed
#if defined(PARAM_CAMERA_DYNAMIC)
		, cameraData
#endif
		);

	path->throughput.r = 1.f;
	path->throughput.g = 1.f;
	path->throughput.b = 1.f;
	path->depth = 0;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	path->specularBounce = TRUE;
	path->state = PATH_STATE_NEXT_VERTEX;
	path->accumRadiance.r = 0.f;
	path->accumRadiance.g = 0.f;
	path->accumRadiance.b = 0.f;
#endif
}

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
__kernel void AdvancePaths_Step1(
		__global Path *paths,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Material *mats,
		__global uint *meshMats,
		__global uint *meshIDs,
		__global Spectrum *vertColors,
		__global Vector *vertNormals,
		__global Triangle *triangles,
        __global TriangleLight *triLights
#if defined(PARAM_HAS_TEXTUREMAPS)
        , __global Spectrum *texMapRGBBuff
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
		, __global float *texMapAlphaBuff
#endif
        , __global TexMap *texMapDescBuff
        , __global unsigned int *meshTexsBuff
        , __global UV *vertUVs
#endif
		) {
	const int gid = get_global_id(0);
	__global Path *path = &paths[gid];

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	uint currentTriangleIndex = rayHit->index;

	// Read the seed
	Seed seed;
	seed.s1 = path->seed.s1;
	seed.s2 = path->seed.s2;
	seed.s3 = path->seed.s3;

	if (path->state == PATH_STATE_SAMPLE_LIGHT) {
		if (currentTriangleIndex == 0xffffffffu) {
			// Nothing was hit, the light is visible

			path->accumRadiance.r += path->lightRadiance.r;
			path->accumRadiance.g += path->lightRadiance.g;
			path->accumRadiance.b += path->lightRadiance.b;
		}
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS) || defined(PARAM_ENABLE_MAT_ARCHGLASS)
		else {
			const uint meshIndex = meshIDs[currentTriangleIndex];

#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
			// Check if is a mesh with alpha texture map applied

			const float hitPointB1 = rayHit->b1;
			const float hitPointB2 = rayHit->b2;

			// Check it the mesh has a texture map
			unsigned int texIndex = meshTexsBuff[meshIndex];
			if (texIndex != 0xffffffffu) {
				__global TexMap *texMap = &texMapDescBuff[texIndex];

				// Check if it has an alpha channel
				if (texMap->alphaOffset != 0xffffffffu) {
					// Interpolate UV coordinates
					UV uv;
					Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);

					const float alpha = TexMap_GetAlpha(&texMapAlphaBuff[texMap->alphaOffset], texMap->width, texMap->height, uv.u, uv.v);

					if ((alpha == 0.0f) || ((alpha < 1.f) && (RndFloatValue(&seed) > alpha))) {
						// Continue to trace the ray
						const float hitPointT = rayHit->t;
						ray->o.x = ray->o.x + ray->d.x * hitPointT;
						ray->o.y = ray->o.y + ray->d.y * hitPointT;
						ray->o.z = ray->o.z + ray->d.z * hitPointT;
						ray->maxt -= hitPointT;

						// Save the seed
						path->seed.s1 = seed.s1;
						path->seed.s2 = seed.s2;
						path->seed.s3 = seed.s3;

						return;
					}
				}
			}
#endif

#if defined(PARAM_ENABLE_MAT_ARCHGLASS)
            // Check if is a shadow transparent material
            __global Material *hitPointMat = &mats[meshMats[meshIndex]];

            if (hitPointMat->type == MAT_ARCHGLASS) {
                const float hitPointT = rayHit->t;
                ray->o.x = ray->o.x + ray->d.x * hitPointT;
                ray->o.y = ray->o.y + ray->d.y * hitPointT;
                ray->o.z = ray->o.z + ray->d.z * hitPointT;
                ray->maxt -= hitPointT;

                path->lightRadiance.r *= hitPointMat->param.archGlass.refrct_r;
                path->lightRadiance.g *= hitPointMat->param.archGlass.refrct_g;
                path->lightRadiance.b *= hitPointMat->param.archGlass.refrct_b;

                // Continue to trace the ray
                return;
            }
#endif
        }
#endif

		// Restore the path RayHit
		*rayHit = path->pathHit;

		// Restore the path Ray
        *ray = path->pathRay;

        path->state = PATH_STATE_NEXT_VERTEX;
        return;
	}

    // state is PATH_STATE_NEXT_VERTEX or PATH_STATE_CONTINUE_NEXT_VERTEX

	// If we are in PATH_STATE_CONTINUE_NEXT_VERTEX state
	path->state = PATH_STATE_NEXT_VERTEX;

    if (currentTriangleIndex != 0xffffffffu) {
        // Something was hit

		const uint meshIndex = meshIDs[currentTriangleIndex];
		__global Material *hitPointMat = &mats[meshMats[meshIndex]];

        Vector rayDir = ray->d;

        const float hitPointT = rayHit->t;
        const float hitPointB1 = rayHit->b1;
        const float hitPointB2 = rayHit->b2;

        Point hitPoint;
        hitPoint.x = ray->o.x + rayDir.x * hitPointT;
        hitPoint.y = ray->o.y + rayDir.y * hitPointT;
        hitPoint.z = ray->o.z + rayDir.z * hitPointT;

		// Interpolate Color
		Spectrum shadeColor;
		Mesh_InterpolateColor(vertColors, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &shadeColor);

#if defined(PARAM_HAS_TEXTUREMAPS)
        // Interpolate UV coordinates
        UV uv;
        Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);

        // Check it the mesh has a texture map
        unsigned int texIndex = meshTexsBuff[meshIndex];
        if (texIndex != 0xffffffffu) {
            __global TexMap *texMap = &texMapDescBuff[texIndex];

#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
			// Check if it has an alpha channel
			if (texMap->alphaOffset != 0xffffffffu) {
				const float alpha = TexMap_GetAlpha(&texMapAlphaBuff[texMap->alphaOffset], texMap->width, texMap->height, uv.u, uv.v);

				if ((alpha == 0.0f) || ((alpha < 1.f) && (RndFloatValue(&seed) > alpha))) {
					// Continue to trace the ray
					ray->o = hitPoint;
					ray->maxt -= hitPointT;

					path->state = PATH_STATE_CONTINUE_NEXT_VERTEX;
					// Save the seed
					path->seed.s1 = seed.s1;
					path->seed.s2 = seed.s2;
					path->seed.s3 = seed.s3;

					return;
				}
			}
#endif

            Spectrum texColor;
            TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texMap->width, texMap->height, uv.u, uv.v, &texColor);

            shadeColor.r *= texColor.r;
            shadeColor.g *= texColor.g;
            shadeColor.b *= texColor.b;
        }
#endif

		// Interpolate the normal
        Vector N;
		Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &N);

		// Flip the normal if required
        Vector shadeN;
		const float nFlip = (Dot(&rayDir, &N) > 0.f) ? -1.f : 1.f;
		shadeN.x = nFlip * N.x;
		shadeN.y = nFlip * N.y;
		shadeN.z = nFlip * N.z;

        float directLightPdf;
        switch (hitPointMat->type) {
            case MAT_MATTE:
                directLightPdf = 1.f;
                break;
            case MAT_MATTEMIRROR:
                directLightPdf = 1.f / hitPointMat->param.matteMirror.mattePdf;
                break;
            case MAT_MATTEMETAL:
                directLightPdf = 1.f / hitPointMat->param.matteMetal.mattePdf;
                break;
            case MAT_ALLOY: {
                // Schilick's approximation
                const float c = 1.f + Dot(&rayDir, &shadeN);
                const float R0 = hitPointMat->param.alloy.R0;
                const float Re = R0 + (1.f - R0) * c * c * c * c * c;

                const float P = .25f + .5f * Re;

                directLightPdf = (1.f - P) / (1.f - Re);
                break;
            }
            default:
                directLightPdf = 0.f;
                break;
        }

        if (directLightPdf > 0.f) {
            // Select a light source to sample
            const uint lightIndex = min((uint)floor(PARAM_DL_LIGHT_COUNT * RndFloatValue(&seed)), (uint)(PARAM_DL_LIGHT_COUNT - 1));
            __global TriangleLight *l = &triLights[lightIndex];

            // Setup the shadow ray
            Vector wo;
            wo.x = -rayDir.x;
            wo.y = -rayDir.y;
            wo.z = -rayDir.z;
            Spectrum Le;
            float lightPdf;
            Ray shadowRay;
            TriangleLight_Sample_L(l, &wo, &hitPoint, &lightPdf, &Le, &shadowRay,
                RndFloatValue(&seed), RndFloatValue(&seed), RndFloatValue(&seed));

            const float dp = Dot(&shadeN, &shadowRay.d);
            const float matPdf = (dp <= 0.f) ? 0.f : 1.f;

            const float pdf = lightPdf * matPdf * directLightPdf;
            if (pdf > 0.f) {
                Spectrum throughput = path->throughput;
                throughput.r *= shadeColor.r;
                throughput.g *= shadeColor.g;
                throughput.b *= shadeColor.b;

                const float k = dp * PARAM_DL_LIGHT_COUNT / (pdf * M_PI);
                // NOTE: I assume all matte mixed material have a MatteParam as first field
                path->lightRadiance.r = throughput.r * hitPointMat->param.matte.r * k * Le.r;
                path->lightRadiance.g = throughput.g * hitPointMat->param.matte.g * k * Le.g;
                path->lightRadiance.b = throughput.b * hitPointMat->param.matte.b * k * Le.b;

                // Save current ray hit information
                path->pathHit.t = hitPointT;
                path->pathHit.b1 = hitPointB1;
                path->pathHit.b2 = hitPointB2;
                path->pathHit.index = currentTriangleIndex;

                // Save the current Ray
                path->pathRay = *ray;

                *ray = shadowRay;

                path->state = PATH_STATE_SAMPLE_LIGHT;
            }

            // Save the seed
            path->seed.s1 = seed.s1;
            path->seed.s2 = seed.s2;
            path->seed.s3 = seed.s3;
        }
    }
}
#endif

__kernel void AdvancePaths_Step2(
		__global Path *paths,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
		__global Material *mats,
		__global uint *meshMats,
		__global uint *meshIDs,
		__global Spectrum *vertColors,
		__global Vector *vertNormals,
		__global Triangle *triangles
#if defined(PARAM_CAMERA_DYNAMIC)
		, __global float *cameraData
#endif
#if defined(PARAM_HAVE_INFINITELIGHT)
		, __global Spectrum *infiniteLightMap
#endif
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global TriangleLight *triLights
#endif
#if defined(PARAM_HAS_TEXTUREMAPS)
        , __global Spectrum *texMapRGBBuff
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
		, __global float *texMapAlphaBuff
#endif
        , __global TexMap *texMapDescBuff
        , __global unsigned int *meshTexsBuff
        , __global UV *vertUVs
#endif
		) {
	const int gid = get_global_id(0);
	__global Path *path = &paths[gid];
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	int pathState = path->state;
	if ((pathState == PATH_STATE_SAMPLE_LIGHT) || (pathState == PATH_STATE_CONTINUE_NEXT_VERTEX)) {
        // Handled by Step1 kernel
        return;
    }
#endif

	// Read the seed
	Seed seed;
	seed.s1 = path->seed.s1;
	seed.s2 = path->seed.s2;
	seed.s3 = path->seed.s3;

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	uint currentTriangleIndex = rayHit->index;

	const float hitPointT = rayHit->t;
    const float hitPointB1 = rayHit->b1;
    const float hitPointB2 = rayHit->b2;

    Vector rayDir = ray->d;

	Point hitPoint;
    hitPoint.x = ray->o.x + rayDir.x * hitPointT;
    hitPoint.y = ray->o.y + rayDir.y * hitPointT;
    hitPoint.z = ray->o.z + rayDir.z * hitPointT;

	Spectrum throughput = path->throughput;

    if (currentTriangleIndex != 0xffffffffu) {
		// Something was hit

		const uint meshIndex = meshIDs[currentTriangleIndex];
		__global Material *hitPointMat = &mats[meshMats[meshIndex]];
		uint matType = hitPointMat->type;

		// Interpolate Color
        Spectrum shadeColor;
		Mesh_InterpolateColor(vertColors, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &shadeColor);

#if defined(PARAM_HAS_TEXTUREMAPS)
        // Interpolate UV coordinates
        UV uv;
        Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);

        // Check it the mesh has a texture map
        unsigned int texIndex = meshTexsBuff[meshIndex];
        if (texIndex != 0xffffffffu) {
            __global TexMap *texMap = &texMapDescBuff[texIndex];

#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS) && !defined(PARAM_DIRECT_LIGHT_SAMPLING)
			// Check if it has an alpha channel
			if (texMap->alphaOffset != 0xffffffffu) {
				const float alpha = TexMap_GetAlpha(&texMapAlphaBuff[texMap->alphaOffset], texMap->width, texMap->height, uv.u, uv.v);

				if ((alpha == 0.0f) || ((alpha < 1.f) && (RndFloatValue(&seed) > alpha))) {
					// Continue to trace the ray
					matType = MAT_NULL;
				}
			}
#endif

            Spectrum texColor;
            TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texMap->width, texMap->height, uv.u, uv.v, &texColor);

            shadeColor.r *= texColor.r;
            shadeColor.g *= texColor.g;
            shadeColor.b *= texColor.b;
        }
#endif

		throughput.r *= shadeColor.r;
		throughput.g *= shadeColor.g;
		throughput.b *= shadeColor.b;

		// Interpolate the normal
        Vector N;
		Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &N);

		// Flip the normal if required
        Vector shadeN;
		const float nFlip = (Dot(&rayDir, &N) > 0.f) ? -1.f : 1.f;
		shadeN.x = nFlip * N.x;
		shadeN.y = nFlip * N.y;
		shadeN.z = nFlip * N.z;

		const float u0 = RndFloatValue(&seed);
		const float u1 = RndFloatValue(&seed);
		const float u2 = RndFloatValue(&seed);

		Vector wo;
		wo.x = -rayDir.x;
		wo.y = -rayDir.y;
		wo.z = -rayDir.z;

		Vector wi;
		float pdf;
		Spectrum f;

		switch (matType) {

#if defined(PARAM_ENABLE_MAT_MATTE)
			case MAT_MATTE:
				Matte_Sample_f(&hitPointMat->param.matte, &wo, &wi, &pdf, &f, &shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
				break;
#endif

#if defined(PARAM_ENABLE_MAT_AREALIGHT)
			case MAT_AREALIGHT: {
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
				if (path->specularBounce) {
#endif
					Spectrum radiance;
					AreaLight_Le(&hitPointMat->param.areaLight, &wo, &N, &radiance);
                    radiance.r *= throughput.r;
                    radiance.g *= throughput.g;
                    radiance.b *= throughput.b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					radiance.r += path->accumRadiance.r;
					radiance.g += path->accumRadiance.g;
					radiance.b += path->accumRadiance.b;
#endif

					TerminatePath(path, ray, frameBuffer, &seed, &radiance
#if defined(PARAM_CAMERA_DYNAMIC)
						, cameraData
#endif
					);

					// Save the seed
					path->seed.s1 = seed.s1;
					path->seed.s2 = seed.s2;
					path->seed.s3 = seed.s3;

					return;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
				}
				break;
#endif
			}
#endif


#if defined(PARAM_ENABLE_MAT_MIRROR)
			case MAT_MIRROR:
				Mirror_Sample_f(&hitPointMat->param.mirror, &wo, &wi, &pdf, &f, &shadeN
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
				break;
#endif

#if defined(PARAM_ENABLE_MAT_GLASS)
			case MAT_GLASS:
				Glass_Sample_f(&hitPointMat->param.glass, &wo, &wi, &pdf, &f, &N, &shadeN, u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
				break;
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMIRROR)
            case MAT_MATTEMIRROR:
                MatteMirror_Sample_f(&hitPointMat->param.matteMirror, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
                break;
#endif

#if defined(PARAM_ENABLE_MAT_METAL)
            case MAT_METAL:
				Metal_Sample_f(&hitPointMat->param.metal, &wo, &wi, &pdf, &f, &shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
                break;
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMETAL)
            case MAT_MATTEMETAL:
                MatteMetal_Sample_f(&hitPointMat->param.matteMetal, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
                break;
#endif

#if defined(PARAM_ENABLE_MAT_ALLOY)
            case MAT_ALLOY:
				Alloy_Sample_f(&hitPointMat->param.alloy, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
                break;
#endif

#if defined(PARAM_ENABLE_MAT_ARCHGLASS)
            case MAT_ARCHGLASS:
				ArchGlass_Sample_f(&hitPointMat->param.archGlass, &wo, &wi, &pdf, &f, &N, &shadeN, u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
					, &path->specularBounce
#endif
					);
                break;
#endif

			case MAT_NULL:
				wi = rayDir;
				pdf = 1.f;
				f.r = 1.f;
				f.g = 1.f;
				f.b = 1.f;

				// I have also to restore the original throughput
				throughput = path->throughput;
				break;

			default:
				// Huston, we have a problem...
				pdf = 0.f;
				break;
		}

		const uint pathDepth = path->depth + 1;
        const float invPdf = ((pdf <= 0.f) || (pathDepth >= PARAM_MAX_PATH_DEPTH)) ? 0.f : (1.f / pdf);
        throughput.r *= f.r * invPdf;
		throughput.g *= f.g * invPdf;
		throughput.b *= f.b * invPdf;

        // Russian roulette
        const float rrProb = max(max(throughput.r, max(throughput.g, throughput.b)), (float)PARAM_RR_CAP);
        const float rrSample = RndFloatValue(&seed);
        const float invRRProb = (pathDepth > PARAM_RR_DEPTH) ? ((rrProb >= rrSample) ? 0.f : (1.f / rrProb)) : 1.f;
        throughput.r *= invRRProb;
        throughput.g *= invRRProb;
        throughput.b *= invRRProb;

		if ((throughput.r <= 0.f) && (throughput.g <= 0.f) && (throughput.b <= 0.f)) {
			Spectrum radiance;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
			radiance.r = path->accumRadiance.r;
			radiance.g = path->accumRadiance.g;
			radiance.b = path->accumRadiance.b;
#else
			radiance.r = 0.f;
			radiance.g = 0.f;
			radiance.b = 0.f;
#endif

			TerminatePath(path, ray, frameBuffer, &seed, &radiance
#if defined(PARAM_CAMERA_DYNAMIC)
				, cameraData
#endif
				);
		} else {
			path->throughput = throughput;

			// Setup next ray
			ray->o = hitPoint;
			ray->d = wi;
			ray->mint = PARAM_RAY_EPSILON;
			ray->maxt = FLT_MAX;

			path->depth = pathDepth;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
			path->state = PATH_STATE_NEXT_VERTEX;
#endif
		}
	} else {
		Spectrum radiance;

#if defined(PARAM_HAVE_INFINITELIGHT)
		Spectrum Le;
		InfiniteLight_Le(infiniteLightMap, &Le, &rayDir);

		radiance.r = throughput.r * Le.r;
		radiance.g = throughput.g * Le.g;
		radiance.b = throughput.b * Le.b;
#else
		radiance.r = 0.f;
		radiance.g = 0.f;
		radiance.b = 0.f;
#endif

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		radiance.r += path->accumRadiance.r;
		radiance.g += path->accumRadiance.g;
		radiance.b += path->accumRadiance.b;
#endif

		TerminatePath(path, ray, frameBuffer, &seed, &radiance
#if defined(PARAM_CAMERA_DYNAMIC)
				, cameraData
#endif
			);
	}

	// Save the seed
	path->seed.s1 = seed.s1;
	path->seed.s2 = seed.s2;
	path->seed.s3 = seed.s3;
}
