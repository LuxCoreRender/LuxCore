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

#include <cstddef>

#include "luxrays/utils/sdl/camera.h"

namespace luxrays { namespace sdl {

void PerspectiveCamera::Update(const unsigned int w, const unsigned int h) {
	filmWidth = w;
	filmHeight = h;

	// Used to move translate the camera
	dir = target - orig;
	dir = Normalize(dir);

	x = Cross(dir, up);
	x = Normalize(x);

	y = Cross(x, dir);
	y = Normalize(y);

	// Used to generate rays

	Transform WorldToCamera = LookAt(orig, target, up);
	cameraToWorld = Inverse(WorldToCamera);

	Transform cameraToScreen = Perspective(fieldOfView, clipHither, clipYon);

	const float frame =  float(filmWidth) / float(filmHeight);
	float screen[4];
	if (frame < 1.f) {
		screen[0] = -frame;
		screen[1] = frame;
		screen[2] = -1.f;
		screen[3] = 1.f;
	} else {
		screen[0] = -1.f;
		screen[1] = 1.f;
		screen[2] = -1.f / frame;
		screen[3] = 1.f / frame;
	}
	Transform screenToRaster =
			Scale(float(filmWidth), float(filmHeight), 1.f) *
			Scale(1.f / (screen[1] - screen[0]), 1.f / (screen[2] - screen[3]), 1.f) *
			luxrays::Translate(Vector(-screen[0], -screen[3], 0.f));

	rasterToCamera = Inverse(screenToRaster * cameraToScreen);

	Transform screenToWorld = cameraToWorld * Inverse(cameraToScreen);
	rasterToWorld = screenToWorld * Inverse(screenToRaster);

	const float tanAngle = tanf(Radians(fieldOfView) / 2.f) * 2.f;
	const float xPixelWidth = tanAngle * ((screen[1] - screen[0]) / 2.f) / filmWidth;
	const float yPixelHeight = tanAngle * ((screen[3] - screen[2]) / 2.f) / filmHeight;
	pixelArea = xPixelWidth * yPixelHeight;
}

void PerspectiveCamera::GenerateRay(
	const float filmX, const float filmY,
	Ray *ray, const float u1, const float u2, const float u3) const {
	const Point Pras(filmX, filmHeight - filmY - 1.f, 0.f);
	const Point Pcamera(rasterToCamera * Pras);

	ray->o = Pcamera;
	ray->d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);

	// Modify ray for depth of field
	if (lensRadius > 0.f) {
		// Sample point on lens
		float lensU, lensV;
		ConcentricSampleDisk(u1, u2, &lensU, &lensV);
		lensU *= lensRadius;
		lensV *= lensRadius;

		// Compute point on plane of focus
		const float ft = (focalDistance - clipHither) / ray->d.z;
		Point Pfocus = (*ray)(ft);
		// Update ray for effect of lens
		ray->o.x += lensU * (focalDistance - clipHither) / focalDistance;
		ray->o.y += lensV * (focalDistance - clipHither) / focalDistance;
		ray->d = Pfocus - ray->o;
	}

	ray->d = Normalize(ray->d);
	ray->mint = MachineEpsilon::E(ray->o);
	ray->maxt = (clipYon - clipHither) / ray->d.z;

	*ray = cameraToWorld * (*ray);
}

bool PerspectiveCamera::GetSamplePosition(const Point &p, const Vector &wi,
	float distance, float *x, float *y) const {
	const float cosi = Dot(wi, dir);
	if ((cosi <= 0.f) || (!isinf(distance) && (distance * cosi < clipHither ||
		distance * cosi > clipYon)))
		return false;

	const Point pO(Inverse(rasterToWorld) * (p + ((lensRadius > 0.f) ?
		(wi * (focalDistance / cosi)) : wi)));

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Check if we are inside the image plane
	if ((*x < 0.f) || (*x >= filmWidth) ||
			(*y < 0.f) || (*y >= filmHeight))
		return false;
	else
		return true;
}

bool PerspectiveCamera::SampleLens(const float u1, const float u2,
		const float u3, Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);
	if (lensRadius > 0.f) {
		ConcentricSampleDisk(u1, u2, &lensPoint.x, &lensPoint.y);
		lensPoint.x *= lensRadius;
		lensPoint.y *= lensRadius;
	}

	*lensp = cameraToWorld * lensPoint;

	return true;
}

} }
