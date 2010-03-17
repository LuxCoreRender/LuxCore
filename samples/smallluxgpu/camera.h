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

#ifndef _CAMERA_H
#define	_CAMERA_H

#include "smalllux.h"
#include "sampler.h"
#include "film.h"
#include "luxrays/core/geometry/transform.h"

class PerspectiveCamera {
public:
	PerspectiveCamera(const bool lowLatency, const Point &o, const Point &t, Film *flm) :
		orig(o), target(t), up(0.f, 0.f, 1.f), fieldOfView(45.f), clipHither(1e-3f), clipYon(1e30f) {
		film = flm;

		Update();
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
		Vector p = target - orig;
		Transform t = ::Rotate(angle, axis);
		target = orig + t(p);
	}

	void Update() {
		// Used to move trnslate the camera
		dir = target - orig;
		dir = Normalize(dir);

		x = Cross(dir, up);
		x = Normalize(x);

		y = Cross(x, dir);
		y = Normalize(y);

		// Used to generate rays
		Transform WorldToCamera = LookAt(orig, target, up);
		CameraToWorld = WorldToCamera.GetInverse();

		Transform CameraToScreen = Perspective(fieldOfView, clipHither, clipYon);

		const float frame =  float(film->GetWidth()) / float(film->GetHeight());
		float screen[4];
		if (frame > 1.f) {
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
				Scale(float(film->GetWidth()), float(film->GetHeight()), 1.f) *
				Scale(1.f / (screen[1] - screen[0]), 1.f / (screen[2] - screen[3]), 1.f) *
				::Translate(Vector(-screen[0], -screen[3], 0.f));

		RasterToCamera = CameraToScreen.GetInverse() * ScreenToRaster.GetInverse();
	}

	void GenerateRay(Sample *sample, Ray *ray) const {
        Point Pras(sample->screenX, film->GetHeight() - sample->screenY - 1.f, 0);
        Point Pcamera;
        RasterToCamera(Pras, &Pcamera);

        ray->o = Pcamera;
        ray->d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);
        ray->d = Normalize(ray->d);
        ray->mint = RAY_EPSILON;
        ray->maxt = (clipYon - clipHither) / ray->d.z;

        CameraToWorld(*ray, ray);
	}

	Film *film;
	// User defined values
	Point orig, target;
	Vector up;
	float fieldOfView, clipHither, clipYon;

private:
	// Calculated values
	Vector dir, x, y;
	Transform RasterToCamera, CameraToWorld;
};

#endif	/* _CAMERA_H */
