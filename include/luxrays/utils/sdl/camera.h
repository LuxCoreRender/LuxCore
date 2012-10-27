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

#ifndef _LUXRAYS_SDL_CAMERA_H
#define	_LUXRAYS_SDL_CAMERA_H

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/core/mc.h"

namespace luxrays { namespace sdl {

class PerspectiveCamera {
public:
	PerspectiveCamera(const Point &o, const Point &t, const Vector &u) :
		orig(o), target(t), up(Normalize(u)), fieldOfView(45.f), clipHither(1e-3f), clipYon(1e30f),
		lensRadius(0.f), focalDistance(10.f) {
		motionBlur = false;
	}

	~PerspectiveCamera() {
	}

	const Vector &GetDir() const { return dir; }
	const float GetPixelArea() const { return pixelArea; }

	void TranslateLeft(const float k) {
		Vector t = -k * Normalize(x);
		Translate(t);
	}

	void TranslateRight(const float k) {
		Vector t = k * Normalize(x);
		Translate(t);
	}

	void TranslateForward(const float k) {
		Vector t = k * dir;
		Translate(t);
	}

	void TranslateBackward(const float k) {
		Vector t = -k * dir;
		Translate(t);
	}

	void Translate(const Vector &t) {
		if (motionBlur) {
			mbOrig = orig;
			mbTarget = target;
		}

		orig += t;
		target += t;
	}

	void RotateLeft(const float k) {
		Rotate(k, y);
	}

	void RotateRight(const float k) {
		Rotate(-k, y);
	}

	void RotateUp(const float k) {
		Rotate(k, x);
	}

	void RotateDown(const float k) {
		Rotate(-k, x);
	}

	void Rotate(const float angle, const Vector &axis) {
		if (motionBlur) {
			mbOrig = orig;
			mbTarget = target;
		}

		Vector p = target - orig;
		Transform t = luxrays::Rotate(angle, axis);
		target = orig + t * p;
	}

	void Update(const unsigned int w, const unsigned int h);

	void GenerateRay(
		const float filmX, const float filmY,
		Ray *ray, const float u1, const float u2, const float u3) const;
	bool GetSamplePosition(const Point &p, const Vector &wi,
		float distance, float *x, float *y) const;

	bool SampleW(const float u1, const float u2, const float u3,
		Point *lensPoint) const;
	void ClampRay(Ray *ray) const {
		const float cosi = Dot(ray->d, dir);
		ray->mint = Max(ray->mint, clipHither / cosi);
		ray->maxt = Min(ray->maxt, clipYon / cosi);
	}

	const Matrix4x4 GetRasterToCameraMatrix() const {
		return rasterToCamera.GetMatrix();
	}

	const Matrix4x4 GetCameraToWorldMatrix() const {
		return cameraToWorld.GetMatrix();
	}

	float GetClipYon() const { return clipYon; }
	float GetClipHither() const { return clipHither; }

	// User defined values
	Point orig, target;
	Vector up;
	float fieldOfView, clipHither, clipYon, lensRadius, focalDistance;

	// For camera motion blur
	bool motionBlur;
	Point mbOrig, mbTarget;
	Vector mbUp;

private:
	u_int filmWidth, filmHeight;

	// Calculated values
	float pixelArea;
	Vector dir, x, y;
	Transform rasterToCamera, rasterToWorld, cameraToWorld;

	Vector mbDeltaOrig, mbDeltaTarget, mbDeltaUp;
};

} }

#endif	/* _LUXRAYS_SDL_CAMERA_H */
