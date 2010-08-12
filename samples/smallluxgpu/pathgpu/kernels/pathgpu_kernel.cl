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
	path->pixelIndex = (PARAM_STARTLINE * PARAM_IMAGE_WIDTH + gid) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);

	// Generate the eye ray
}

__kernel void AdvancePaths(
		__global Path *paths,
		__global Ray *rays,
		__global RayHit *rayHits) {
	const int gid = get_global_id(0);
	if (gid >= PARAM_PATH_COUNT)
		return;
}
