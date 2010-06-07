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
#include "luxrays/utils/sdl/mc.h"

namespace luxrays { namespace sdl {

class PerspectiveCamera {
public:
	PerspectiveCamera(const Point &o, const Point &t, const Vector &u) :
		orig(o), target(t), up(u), fieldOfView(45.f), clipHither(1e-3f), clipYon(1e30f),
		lensRadius(0.f), focalDistance(10.f) {
		motionBlur = false;
	}

	~PerspectiveCamera() {
	}

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
		target = orig + t(p);
	}

	void Update(const unsigned int filmWidth, const unsigned int filmHeight) {
		// Used to move trnslate the camera
		dir = target - orig;
		dir = Normalize(dir);

		x = Cross(dir, up);
		x = Normalize(x);

		y = Cross(x, dir);
		y = Normalize(y);

		// Used to generate rays

		if (motionBlur) {
			mbDeltaOrig = mbOrig - orig;
			mbDeltaTarget = mbTarget - target;
			mbDeltaUp = mbUp - up;
		} else {
			Transform WorldToCamera = LookAt(orig, target, up);
			CameraToWorld = WorldToCamera.GetInverse();
		}

		Transform CameraToScreen = Perspective(fieldOfView, clipHither, clipYon);

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
		Transform ScreenToRaster =
				Scale(float(filmWidth), float(filmHeight), 1.f) *
				Scale(1.f / (screen[1] - screen[0]), 1.f / (screen[2] - screen[3]), 1.f) *
				luxrays::Translate(Vector(-screen[0], -screen[3], 0.f));

		RasterToCamera = CameraToScreen.GetInverse() * ScreenToRaster.GetInverse();
	}

	void GenerateRay(
		const float screenX, const float screenY,
		const unsigned int filmWidth, const unsigned int filmHeight,
		Ray *ray, const float u1, const float u2, const float u3) const {
		Transform c2w;
		if (motionBlur) {
			const Point sampledOrig = orig + mbDeltaOrig * u3;
			const Point sampledTarget = target + mbDeltaTarget * u3;
			const Vector sampledUp = Normalize(up + mbDeltaUp * u3);

			// Build the CameraToWorld transformation
			Transform WorldToCamera = LookAt(sampledOrig, sampledTarget, sampledUp);
			c2w = WorldToCamera.GetInverse();
		} else
			c2w = CameraToWorld;

        Point Pras(screenX, filmHeight - screenY - 1.f, 0);
        Point Pcamera;
        RasterToCamera(Pras, &Pcamera);

        ray->o = Pcamera;
        ray->d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);

		// Modify ray for depth of field
		if (lensRadius > 0.f) {
			// Sample point on lens
			Point lenP, lenPCamera;
			ConcentricSampleDisk(u1, u2, &(lenPCamera.x), &(lenPCamera.y));

			lenPCamera.x *= lensRadius;
			lenPCamera.y *= lensRadius;
			lenPCamera.z = 0;
			c2w(lenPCamera, &lenP);
			float lensU = lenPCamera.x;
			float lensV = lenPCamera.y;

			// Compute point on plane of focus
			float ft = (focalDistance - clipHither) / ray->d.z;
			Point Pfocus = (*ray)(ft);
			// Update ray for effect of lens
			ray->o.x += lensU * (focalDistance - clipHither) / focalDistance;
			ray->o.y += lensV * (focalDistance - clipHither) / focalDistance;
			ray->d = Pfocus - ray->o;
		}

        ray->d = Normalize(ray->d);
        ray->mint = RAY_EPSILON;
        ray->maxt = (clipYon - clipHither) / ray->d.z;

        c2w(*ray, ray);
	}

	// User defined values
	Point orig, target;
	Vector up;
	float fieldOfView, clipHither, clipYon, lensRadius, focalDistance;

	// For camera motion blur
	bool motionBlur;
	Point mbOrig, mbTarget;
	Vector mbUp;

private:
	// Calculated values
	Vector dir, x, y;
	Transform RasterToCamera, CameraToWorld;

	Vector mbDeltaOrig, mbDeltaTarget, mbDeltaUp;
};

} }

#endif	/* _LUXRAYS_SDL_CAMERA_H */
