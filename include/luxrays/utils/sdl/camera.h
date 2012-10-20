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

	void Update(const unsigned int w, const unsigned int h) {
		filmWidth = w;
		filmHeight = h;

		// Used to move translate the camera
		dir = target - orig;
		dir = Normalize(dir);

		x = Cross(dir, up);
		x = Normalize(x);

		y = Cross(x, dir);
		y = Normalize(y);

		const float tanHalfAngle = tanf(fieldOfView * M_PI / 360.f);
		const float pixelWidth = tanHalfAngle / filmWidth;
		const float pixelHeight = tanHalfAngle / filmHeight;
        pixelArea = pixelWidth * pixelHeight;

		// Used to generate rays

		if (motionBlur) {
			mbDeltaOrig = mbOrig - orig;
			mbDeltaTarget = mbTarget - target;
			mbDeltaUp = mbUp - up;
		} else {
			Transform WorldToCamera = LookAt(orig, target, up);
			cameraToWorld = WorldToCamera.GetInverse();
		}

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

		rasterToCamera = cameraToScreen.GetInverse() * screenToRaster.GetInverse();

		Transform screenToWorld = cameraToWorld * cameraToScreen.GetInverse();
		rasterToWorld = screenToWorld * screenToRaster.GetInverse();
	}

	void GenerateRay(
		const float filmX, const float filmY,
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
			c2w = cameraToWorld;

        Point Pras(filmX, filmHeight - filmY - 1.f, 0);
        Point Pcamera(rasterToCamera * Pras);

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

        *ray = c2w * *ray;
	}

	bool GetSamplePosition(const Point &p, const Vector &wi,
		float distance, float *x, float *y) const {
		const float cosi = Dot(wi, dir);
		if (cosi <= 0.f || (!isinf(distance) && (distance * cosi < clipHither ||
			distance * cosi > clipYon)))
			return false;

		const Point pO(rasterToWorld / (p + (lensRadius > 0.f ?
			wi * (focalDistance / cosi) : wi)));

		*x = pO.x;
		*y = filmHeight - 1 - pO.y;

		return true;
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
