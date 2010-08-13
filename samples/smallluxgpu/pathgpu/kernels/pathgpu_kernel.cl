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
	Spectrum throughput;
	unsigned int depth, pixelIndex;
} Path;

typedef struct {
	Spectrum c;
	unsigned int count;
} Pixel;

//------------------------------------------------------------------------------

float Dot(Vector *v) {
	return v->x * v->x + v->y * v->y + v->z * v->z;
}

void Normalize(Vector *v) {
	const float il = 1.f / Dot(v);
	v->x = il;
	v->y = il;
	v->z = il;
}

void GenerateRay(
		const unsigned int pixelIndex,
		__global Ray *ray) {
	const float screenX = pixelIndex % PARAM_IMAGE_WIDTH; // + random
	const float screenY = pixelIndex / PARAM_IMAGE_WIDTH; // + random
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

	// Generate the eye ray
	GenerateRay(pixelIndex, &rays[gid]);
}

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

__kernel void AdvancePaths(
		__global Path *paths,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_PATH_COUNT)
		return;

	__global Path *path = &paths[gid];
	const unsigned int pixelIndex = path->pixelIndex;

	__global Pixel *pixel = &frameBuffer[pixelIndex];
	pixel->c.r += gid / (float)PARAM_PATH_COUNT;
	pixel->c.g += 0.f;
	pixel->c.b += 0.f;
	pixel->count += 1;
}
