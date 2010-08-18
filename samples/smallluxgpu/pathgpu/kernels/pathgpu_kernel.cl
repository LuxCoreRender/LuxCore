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
	unsigned int v0, v1, v2;
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
	unsigned int s1, s2, s3;
} Seed;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex, subpixelIndex;
	Seed seed;
} Path;

typedef struct {
	Spectrum c;
	unsigned int count;
} Pixel;

#define MAT_MATTE 0
#define MAT_AREALIGHT 1
#define MAT_MIRROR 2

typedef struct {
	unsigned int type;
	union {
		struct {
			float r, g, b;
		} matte;
		struct {
			float gain_r, gain_g, gain_b;
		} areaLight;
		struct {
			float r, g, b;
		} mirror;
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
				theta = 8.0f + sy / r;
		} else {
			// Handle second region of disk
			r = sy;
			theta = 2.0f - sx / r;
		}
	} else {
		if (sx <= sy) {
			// Handle third region of disk
			r = -sx;
			theta = 4.0f - sy / r;
		} else {
			// Handle fourth region of disk
			r = -sy;
			theta = 6.0f + sx / r;
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
		const unsigned int pixelIndex,
		__global Ray *ray, Seed *seed) {

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
	const float iw = 1.f / (PARAM_RASTER2CAMERA_30 * Pras.x + PARAM_RASTER2CAMERA_31 * Pras.y + PARAM_RASTER2CAMERA_32 * Pras.z + PARAM_RASTER2CAMERA_33);
	orig.x = (PARAM_RASTER2CAMERA_00 * Pras.x + PARAM_RASTER2CAMERA_01 * Pras.y + PARAM_RASTER2CAMERA_02 * Pras.z + PARAM_RASTER2CAMERA_03) * iw;
	orig.y = (PARAM_RASTER2CAMERA_10 * Pras.x + PARAM_RASTER2CAMERA_11 * Pras.y + PARAM_RASTER2CAMERA_12 * Pras.z + PARAM_RASTER2CAMERA_13) * iw;
	orig.z = (PARAM_RASTER2CAMERA_20 * Pras.x + PARAM_RASTER2CAMERA_21 * Pras.y + PARAM_RASTER2CAMERA_22 * Pras.z + PARAM_RASTER2CAMERA_23) * iw;

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
	const float iw2 = 1.f / (PARAM_CAMERA2WORLD_30 * orig.x + PARAM_CAMERA2WORLD_31 * orig.y + PARAM_CAMERA2WORLD_32 * orig.z + PARAM_CAMERA2WORLD_33);
	torig.x = (PARAM_CAMERA2WORLD_00 * orig.x + PARAM_CAMERA2WORLD_01 * orig.y + PARAM_CAMERA2WORLD_02 * orig.z + PARAM_CAMERA2WORLD_03) * iw2;
	torig.y = (PARAM_CAMERA2WORLD_10 * orig.x + PARAM_CAMERA2WORLD_11 * orig.y + PARAM_CAMERA2WORLD_12 * orig.z + PARAM_CAMERA2WORLD_13) * iw2;
	torig.z = (PARAM_CAMERA2WORLD_20 * orig.x + PARAM_CAMERA2WORLD_21 * orig.y + PARAM_CAMERA2WORLD_22 * orig.z + PARAM_CAMERA2WORLD_23) * iw2;

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
	path->subpixelIndex = 0;

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

#if defined(PARAM_HAVE_INFINITELIGHT)
void InfiniteLight_Le(__global Spectrum *infiniteLightMap, Spectrum *le, Vector *dir) {
	const float u = SphericalPhi(dir) * INV_TWOPI +  PARAM_IL_SHIFT_U;
	const float v = SphericalTheta(dir) * INV_PI + PARAM_IL_SHIFT_V;

	TexMap_GetColor(infiniteLightMap, PARAM_IL_WIDTH, PARAM_IL_HEIGHT, u, v, le);

	le->r *= PARAM_IL_GAIN_R;
	le->g *= PARAM_IL_GAIN_G;
	le->b *= PARAM_IL_GAIN_B;
}
#endif

void Mesh_InterpolateColor(__global Spectrum *colors, __global Triangle *triangles,
		const unsigned int triIndex, const float b1, const float b2, Spectrum *C) {
	__global Triangle *tri = &triangles[triIndex];

	const float b0 = 1.f - b1 - b2;
	C->r = b0 * colors[tri->v0].r + b1 * colors[tri->v1].r + b2 * colors[tri->v2].r;
	C->g = b0 * colors[tri->v0].g + b1 * colors[tri->v1].g + b2 * colors[tri->v2].g;
	C->b = b0 * colors[tri->v0].b + b1 * colors[tri->v1].b + b2 * colors[tri->v2].b;
}

void Mesh_InterpolateNormal(__global Vector *normals, __global Triangle *triangles,
		const unsigned int triIndex, const float b1, const float b2, Vector *N) {
	__global Triangle *tri = &triangles[triIndex];

	const float b0 = 1.f - b1 - b2;
	N->x = b0 * normals[tri->v0].x + b1 * normals[tri->v1].x + b2 * normals[tri->v2].x;
	N->y = b0 * normals[tri->v0].y + b1 * normals[tri->v1].y + b2 * normals[tri->v2].y;
	N->z = b0 * normals[tri->v0].z + b1 * normals[tri->v1].z + b2 * normals[tri->v2].z;

	Normalize(N);
}

//------------------------------------------------------------------------------
// Materials
//------------------------------------------------------------------------------

void Matte_Sample_f(__global Material *mat, const Vector *wo, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN,
		const float u0, const float u1,  const float u2) {
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

		f->r = mat->mat.matte.r * INV_PI;
		f->g = mat->mat.matte.g * INV_PI;
		f->b = mat->mat.matte.b * INV_PI;
	}
}

void AreaLight_Le(__global Material *mat, const Vector *N, const Vector *wo, Spectrum *Le) {
	if (Dot(N, wo) <= 0.f) {
		Le->r = 0.f;
		Le->g = 0.f;
		Le->b = 0.f;
	} else {
		Le->r = mat->mat.areaLight.gain_r;
		Le->g = mat->mat.areaLight.gain_g;
		Le->b = mat->mat.areaLight.gain_b;
	}
}

void Mirror_Sample_f(__global Material *mat, const Vector *rayDir, Vector *wi,
		float *pdf, Spectrum *f, const Vector *shadeN, const float RdotShadeN) {
	wi->x = rayDir->x + (2.f * RdotShadeN) * shadeN->x;
	wi->y = rayDir->y + (2.f * RdotShadeN) * shadeN->y;
	wi->z = rayDir->z + (2.f * RdotShadeN) * shadeN->z;

	*pdf = 1.f;

	f->r = mat->mat.mirror.r;
	f->g = mat->mat.mirror.g;
	f->b = mat->mat.mirror.b;
}

//------------------------------------------------------------------------------

void TerminatePath(__global Path *path, __global Ray *ray, __global Pixel *frameBuffer, Seed *seed, Spectrum *radiance) {
	// Add sample to the framebuffer

	const unsigned int pixelIndex = path->pixelIndex;
	__global Pixel *pixel = &frameBuffer[pixelIndex];

	pixel->c.r += isnan(radiance->r) ? 0.f : radiance->r;
	pixel->c.g += isnan(radiance->g) ? 0.f : radiance->g;
	pixel->c.b += isnan(radiance->b) ? 0.f : radiance->b;
	pixel->count += 1;

	const unsigned int subpixelIndex = path->subpixelIndex;
	unsigned int newPixelIndex;
	if (subpixelIndex >= PARAM_SAMPLE_PER_PIXEL) {
		newPixelIndex = (pixelIndex + PARAM_PATH_COUNT) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
		path->pixelIndex = newPixelIndex;
		path->subpixelIndex = 0;
	} else {
		newPixelIndex = pixelIndex;
		path->subpixelIndex = subpixelIndex + 1;
	}

	GenerateRay(newPixelIndex, ray, seed);

	// Re-initialize the path
	path->throughput.r = 1.f;
	path->throughput.g = 1.f;
	path->throughput.b = 1.f;
	path->depth = 0;
}

__kernel void AdvancePaths(
		__global Path *paths,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
		__global Material *mats,
		__global unsigned int *meshMats,
		__global unsigned int *meshIDs, // Not used
		__global unsigned int *triIDs,
		__global Vector *vertColors,
		__global Vector *vertNormals,
		__global Triangle *triangles
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
	Vector rayDir;
	rayDir.x = ray->d.x;
	rayDir.y = ray->d.y;
	rayDir.z = ray->d.z;

	Spectrum throughput;
	throughput.r = path->throughput.r;
	throughput.g = path->throughput.g;
	throughput.b = path->throughput.b;

	__global RayHit *rayHit = &rayHits[gid];
	const unsigned int currentTriangleIndex = rayHit->index;
	if (currentTriangleIndex != 0xffffffffu ) {
		// Something was hit

		// Interpolate Color
		Spectrum shadeColor;
		Mesh_InterpolateColor(vertColors, triangles, currentTriangleIndex, rayHit->b1, rayHit->b2, &shadeColor);
		throughput.r *= shadeColor.r;
		throughput.g *= shadeColor.g;
		throughput.b *= shadeColor.b;

		// Interpolate the normal
		Vector N;
		Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, rayHit->b1, rayHit->b2, &N);

		// Flip the normal if required
		Vector shadeN;
		float RdotShadeN = Dot(&rayDir, &N);

		const float nFlip = (RdotShadeN > 0.f) ? -1.f : 1.f;
		shadeN.x = nFlip * N.x;
		shadeN.y = nFlip * N.y;
		shadeN.z = nFlip * N.z;
		RdotShadeN = -nFlip * RdotShadeN;

		const unsigned int meshIndex = meshIDs[currentTriangleIndex];
		__global Material *mat = &mats[meshMats[meshIndex]];

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

		Spectrum materialLe;
		materialLe.r = 0.f;
		materialLe.g = 0.f;
		materialLe.b = 0.f;
		bool areaLightHit = false;
		switch (mat->type) {
			case MAT_MATTE:
				Matte_Sample_f(mat, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2);
				break;
			case MAT_AREALIGHT:
				areaLightHit = true;
				AreaLight_Le(mat, &N, &wo, &materialLe);
				break;
			case MAT_MIRROR:
				Mirror_Sample_f(mat, &rayDir, &wi, &pdf, &f, &shadeN, RdotShadeN);
				break;
			default:
				// Huston, we have a problem...
				pdf = 0.f;
				break;
		}

		// Russian roulette
		const float rrProb = max(max(throughput.r, max(throughput.g, throughput.b)), (float)PARAM_RR_CAP);

		const unsigned int pathDepth = path->depth + 1;
		const float rrSample = RndFloatValue(&seed);
		bool terminatePath = areaLightHit || (pdf <= 0.f) || (pathDepth >= PARAM_MAX_PATH_DEPTH) ||
			((pathDepth > PARAM_RR_DEPTH) && (rrProb < rrSample));

		const float invRRProb = 1.f / rrProb;
		throughput.r *= invRRProb;
		throughput.g *= invRRProb;
		throughput.b *= invRRProb;

		if (terminatePath) {
			Spectrum radiance;
			radiance.r = materialLe.r * path->throughput.r;
			radiance.g = materialLe.g * path->throughput.g;
			radiance.b = materialLe.b * path->throughput.b;

			TerminatePath(path, ray, frameBuffer, &seed, &radiance);
		} else {
			const float invPdf = 1.f / pdf;
			path->throughput.r = throughput.r * f.r * invPdf;
			path->throughput.g = throughput.g * f.g * invPdf;
			path->throughput.b = throughput.b * f.b * invPdf;

			// Setup next ray
			const float t = rayHit->t;
			ray->o.x = ray->o.x + rayDir.x * t;
			ray->o.y = ray->o.y + rayDir.y * t;
			ray->o.z = ray->o.z + rayDir.z * t;

			ray->d.x = wi.x;
			ray->d.y = wi.y;
			ray->d.z = wi.z;

			path->depth = pathDepth;
		}
	} else {
		Spectrum radiance;

#if defined(PARAM_HAVE_INFINITELIGHT)
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

		TerminatePath(path, ray, frameBuffer, &seed, &radiance);
	}

	// Save the seed
	path->seed.s1 = seed.s1;
	path->seed.s2 = seed.s2;
	path->seed.s3 = seed.s3;
}
