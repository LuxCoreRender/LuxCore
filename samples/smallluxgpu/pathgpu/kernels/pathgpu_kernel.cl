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
#define M_PI M_PI_F
#endif

#ifndef INV_PI
#define INV_PI  0.31830988618379067154f
#endif

#ifndef INV_TWOPI
#define INV_TWOPI  0.15915494309189533577f
#endif

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

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
	unsigned int s1, s2, s3;
} Seed;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex;
	Seed seed;
} Path;

typedef struct {
	Spectrum c;
	unsigned int count;
} Pixel;

#define MAT_MATTE 0

typedef struct {
	unsigned int type;
	union {
		struct {
			float r, g, b;
		} matte;
	} mat;
} Material;

//------------------------------------------------------------------------------
// Random number generator
// maximally equidistributed combined Tausworthe generator
//------------------------------------------------------------------------------

#define FLOATMASK 0x00ffffffu

unsigned int TAUSWORTHE(const unsigned int s, const unsigned int a,
	const unsigned int b, const unsigned int c,
	const unsigned int d) {
	return ((s&c)<<d) ^ (((s << a) ^ s) >> b);
}

unsigned int LCG(const unsigned int x) { return x * 69069; }

unsigned int ValidSeed(const unsigned int x, const unsigned int m) {
	return (x < m) ? (x + m) : x;
}

void InitRandomGenerator(unsigned int seed, Seed *s) {
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

float Dot(Vector *v) {
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

void Normalize(Vector *v) {
	const float il = 1.f / sqrt(Dot(v));

	v->x *= il;
	v->y *= il;
	v->z *= il;
}

//------------------------------------------------------------------------------

void GenerateRay(
		const unsigned int pixelIndex,
		__global Ray *ray, Seed *seed) {
	const float screenX = pixelIndex % PARAM_IMAGE_WIDTH + RndFloatValue(seed) - 0.5f;
	const float screenY = pixelIndex / PARAM_IMAGE_WIDTH + RndFloatValue(seed) - 0.5f;
	Point Pras;
	Pras.x = screenX;
	Pras.y = PARAM_IMAGE_HEIGHT - screenY - 1.f;
	Pras.z = 0;

	Point Pcamera;
	// RasterToCamera(Pras, &Pcamera);
	const float iw = 1.f / (PARAM_RASTER2CAMERA_30 * Pras.x + PARAM_RASTER2CAMERA_31 * Pras.y + PARAM_RASTER2CAMERA_32 * Pras.z + PARAM_RASTER2CAMERA_33);
	Pcamera.x = (PARAM_RASTER2CAMERA_00 * Pras.x + PARAM_RASTER2CAMERA_01 * Pras.y + PARAM_RASTER2CAMERA_02 * Pras.z + PARAM_RASTER2CAMERA_03) * iw;
	Pcamera.y = (PARAM_RASTER2CAMERA_10 * Pras.x + PARAM_RASTER2CAMERA_11 * Pras.y + PARAM_RASTER2CAMERA_12 * Pras.z + PARAM_RASTER2CAMERA_13) * iw;
	Pcamera.z = (PARAM_RASTER2CAMERA_20 * Pras.x + PARAM_RASTER2CAMERA_21 * Pras.y + PARAM_RASTER2CAMERA_22 * Pras.z + PARAM_RASTER2CAMERA_23) * iw;

	Vector dir;
	dir.x = Pcamera.x;
	dir.y = Pcamera.y;
	dir.z = Pcamera.z;
	Normalize(&dir);

	// CameraToWorld(*ray, ray);
	Point torig;
	const float iw2 = 1.f / (PARAM_CAMERA2WORLD_30 * Pcamera.x + PARAM_CAMERA2WORLD_31 * Pcamera.y + PARAM_CAMERA2WORLD_32 * Pcamera.z + PARAM_CAMERA2WORLD_33);
	torig.x = (PARAM_CAMERA2WORLD_00 * Pcamera.x + PARAM_CAMERA2WORLD_01 * Pcamera.y + PARAM_CAMERA2WORLD_02 * Pcamera.z + PARAM_CAMERA2WORLD_03) * iw2;
	torig.y = (PARAM_CAMERA2WORLD_10 * Pcamera.x + PARAM_CAMERA2WORLD_11 * Pcamera.y + PARAM_CAMERA2WORLD_12 * Pcamera.z + PARAM_CAMERA2WORLD_13) * iw2;
	torig.z = (PARAM_CAMERA2WORLD_20 * Pcamera.x + PARAM_CAMERA2WORLD_21 * Pcamera.y + PARAM_CAMERA2WORLD_22 * Pcamera.z + PARAM_CAMERA2WORLD_23) * iw2;

	Vector tdir;
	tdir.x = PARAM_CAMERA2WORLD_00 * dir.x + PARAM_CAMERA2WORLD_01 * dir.y + PARAM_CAMERA2WORLD_02 * dir.z;
	tdir.y = PARAM_CAMERA2WORLD_10 * dir.x + PARAM_CAMERA2WORLD_11 * dir.y + PARAM_CAMERA2WORLD_12 * dir.z;
	tdir.z = PARAM_CAMERA2WORLD_20 * dir.x + PARAM_CAMERA2WORLD_21 * dir.y + PARAM_CAMERA2WORLD_22 * dir.z;

	ray->o = torig;
	ray->d = tdir;
	ray->mint = PARAM_RAY_EPSILON;
	ray->maxt = (PARAM_CLIP_YON - PARAM_CLIP_HITHER) / dir.z;
}

//------------------------------------------------------------------------------

__kernel void Init(
		__global Path *paths,
		__global Ray *rays) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_PATH_COUNT)
		return;

	// Initialize the path
	__global Path *path = &paths[gid];
	path->throughput.r = 1.f;
	path->throughput.g = 1.f;
	path->throughput.b = 1.f;
	path->depth = 0;
	const unsigned int pixelIndex = (PARAM_STARTLINE * PARAM_IMAGE_WIDTH + gid) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
	path->pixelIndex = pixelIndex;

	// Initialize random number generator
	Seed seed;
	InitRandomGenerator(PARAM_SEED + gid, &seed);

	// Generate the eye ray
	GenerateRay(pixelIndex, &rays[gid], &seed);

	// Save the seed
	path->seed.s1 = seed.s1;
	path->seed.s2 = seed.s2;
	path->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------

__kernel void InitFrameBuffer(
		__global Pixel *frameBuffer) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT)
		return;

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0;
}

//------------------------------------------------------------------------------

int Mod(int a, int b) {
	if (b == 0)
		b = 1;

	a %= b;
	if (a < 0)
		a += b;

	return a;
}

void TexMap_GetTexel(__global Spectrum *pixels, const unsigned int width, const unsigned int height,
	const int s, const int t, Spectrum *col) {
	const unsigned int u = Mod(s, width);
	const unsigned int v = Mod(t, height);

	const unsigned index = v * width + u;

	col->r = pixels[index].r;
	col->g = pixels[index].g;
	col->b = pixels[index].b;
}

void TexMap_GetColor(__global Spectrum *pixels, const unsigned int width, const unsigned int height,
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

float SphericalTheta(const Vector *v) {
	return acos(clamp(v->z, -1.f, 1.f));
}

float SphericalPhi(const Vector *v) {
	float p = atan2(v->y, v->x);
	return (p < 0.f) ? p + 2.f * M_PI : p;
}

void InfiniteLight_Le(__global Spectrum *infiniteLightMap, Spectrum *le, Vector *dir) {
	const float u = SphericalPhi(dir) * INV_TWOPI +  PARAM_IL_SHIFT_U;
	const float v = SphericalTheta(dir) * INV_PI + PARAM_IL_SHIFT_V;

	TexMap_GetColor(infiniteLightMap, PARAM_IL_WIDTH, PARAM_IL_HEIGHT, u, v, le);

	le->r *= PARAM_IL_GAIN_R;
	le->g *= PARAM_IL_GAIN_G;
	le->b *= PARAM_IL_GAIN_B;
}

__kernel void AdvancePaths(
		__global Path *paths,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
		__global Material *mats,
		__global unsigned int *meshMats,
		__global unsigned int *meshIDs,
		__global unsigned int *triIDs
#if defined(PARAM_HAVE_INFINITELIGHT)
		, __global Spectrum *infiniteLightMap
#endif
		) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_PATH_COUNT)
		return;

	__global Path *path = &paths[gid];

	// Read the seed
	Seed seed;
	seed.s1 = path->seed.s1;
	seed.s2 = path->seed.s2;
	seed.s3 = path->seed.s3;

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	const unsigned int currentTriangleIndex = rayHit->index;
	Spectrum radiance;
	if (currentTriangleIndex != 0xffffffffu ) {
		// Something was hit

		const unsigned int meshIndex = meshIDs[currentTriangleIndex];
		__global Material *mat = &mats[meshMats[meshIndex]];

		switch (mat->type) {
			case MAT_MATTE:
				radiance.r = mat->mat.matte.r;
				radiance.g = mat->mat.matte.g;
				radiance.b = mat->mat.matte.b;
				break;
			default:
				// Huston, we have a problem...
				radiance.r = 0.f;
				radiance.g = 0.f;
				radiance.b = 0.f;
				break;
		}
	} else {
#if defined(PARAM_HAVE_INFINITELIGHT)
		Vector rayDir;
		rayDir.x = ray->d.x;
		rayDir.y = ray->d.y;
		rayDir.z = ray->d.z;

		Spectrum Le;
		InfiniteLight_Le(infiniteLightMap, &Le, &rayDir);

		radiance.r = Le.r * path->throughput.r;
		radiance.g = Le.g * path->throughput.g;
		radiance.b = Le.b * path->throughput.b;
#else
		radiance.r = 0.f;
		radiance.g = 0.f;
		radiance.b = 0.f;
#endif
	}

	// Add sample to the framebuffer

	const unsigned int pixelIndex = path->pixelIndex;
	__global Pixel *pixel = &frameBuffer[pixelIndex];
	pixel->c.r += radiance.r;
	pixel->c.g += radiance.g;
	pixel->c.b += radiance.b;
	pixel->count += 1;

	const unsigned int newPixelIndex = (pixelIndex + PARAM_PATH_COUNT) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
	GenerateRay(newPixelIndex, ray, &seed);

	path->pixelIndex = newPixelIndex;

	// Save the seed
	path->seed.s1 = seed.s1;
	path->seed.s2 = seed.s2;
	path->seed.s3 = seed.s3;
}
