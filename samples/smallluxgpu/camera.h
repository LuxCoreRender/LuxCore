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
	PerspectiveCamera(const bool lowLatency, const Point &o, const Point &t,
			Film *flm) : orig(o), target(t), fieldOfView(45.f) {
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
		dir = target - orig;
		dir = Normalize(dir);

		const Vector up(0.f, 0.f, 1.f);

		const float k = Radians(fieldOfView);
		x = Cross(dir, up);
		x = Normalize(x);
		x *= film->GetWidth() * k / film->GetHeight();

		y = Cross(x, dir);
		y = Normalize(y);
		y *= k;
	}

	void GenerateRay(Sample *sample, Ray *ray) const {
		const float cx = sample->screenX / film->GetWidth() - .5f;
		const float cy = sample->screenY / film->GetHeight() - .5f;
		Vector rdir = x * cx + y * cy + dir;
		Point rorig = orig;
		rorig += rdir * 0.1f;
		rdir = Normalize(rdir);

		*ray = Ray(rorig, rdir);
	}

	Film *film;
	/* User defined values */
	Point orig, target;
	float fieldOfView;

private:
	/* Calculated values */
	Vector dir, x, y;
};

#endif	/* _CAMERA_H */
