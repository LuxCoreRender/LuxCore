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

#ifndef _SLG_CAMERA_H
#define	_SLG_CAMERA_H

#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/transform.h"
#include "slg/core/mc.h"

namespace slg {
	
//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
using namespace luxrays::ocl;
#include "slg/camera/camera_types.cl"
} 

//------------------------------------------------------------------------------
// PerspectiveCamera
//------------------------------------------------------------------------------

class PerspectiveCamera {
public:
	PerspectiveCamera(const luxrays::Point &o, const luxrays::Point &t, const luxrays::Vector &u, const float *region = NULL) :
		orig(o), target(t), up(Normalize(u)), fieldOfView(45.f), clipHither(1e-3f), clipYon(1e30f),
		lensRadius(0.f), focalDistance(10.f) {
		if (region) {
			autoUpdateFilmRegion = false;
			filmRegion[0] = region[0];
			filmRegion[1] = region[1];
			filmRegion[2] = region[2];
			filmRegion[3] = region[3];
		} else
			autoUpdateFilmRegion = true;
			
	}

	~PerspectiveCamera() {
	}

	const luxrays::Vector &GetDir() const { return dir; }
	const float GetPixelArea() const { return pixelArea; }
	const u_int GetFilmWeight() const { return filmWidth; }
	const u_int GetFilmHeight() const { return filmHeight; }

	void Translate(const luxrays::Vector &t) {
		orig += t;
		target += t;
	}

	void TranslateLeft(const float k) {
		luxrays::Vector t = -k * luxrays::Normalize(x);
		Translate(t);
	}

	void TranslateRight(const float k) {
		luxrays::Vector t = k * luxrays::Normalize(x);
		Translate(t);
	}

	void TranslateForward(const float k) {
		luxrays::Vector t = k * dir;
		Translate(t);
	}

	void TranslateBackward(const float k) {
		luxrays::Vector t = -k * dir;
		Translate(t);
	}

	void Rotate(const float angle, const luxrays::Vector &axis) {
		luxrays::Vector p = target - orig;
		luxrays::Transform t = luxrays::Rotate(angle, axis);
		target = orig + t * p;
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

	void Update(const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion = NULL);

	void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2) const;
	bool GetSamplePosition(const luxrays::Point &p, const luxrays::Vector &wi,
		float distance, float *x, float *y) const;

	bool SampleLens(const float u1, const float u2,
		luxrays::Point *lensPoint) const;
	void ClampRay(luxrays::Ray *ray) const {
		const float cosi = Dot(ray->d, dir);
		ray->mint = luxrays::Max(ray->mint, clipHither / cosi);
		ray->maxt = luxrays::Min(ray->maxt, clipYon / cosi);
	}

	const luxrays::Matrix4x4 GetRasterToCameraMatrix() const {
		return rasterToCamera.GetMatrix();
	}

	const luxrays::Matrix4x4 GetCameraToWorldMatrix() const {
		return cameraToWorld.GetMatrix();
	}

	float GetClipYon() const { return clipYon; }
	float GetClipHither() const { return clipHither; }

	luxrays::Properties ToProperties() const;

	// User defined values
	luxrays::Point orig, target;
	luxrays::Vector up;
	float fieldOfView, clipHither, clipYon, lensRadius, focalDistance;

	float filmRegion[4];
	bool autoUpdateFilmRegion;

private:
	u_int filmWidth, filmHeight, filmSubRegion[4];

	// Calculated values
	float pixelArea;
	luxrays::Vector dir, x, y;

	// ProjectiveCamera Protected Data
	luxrays::Transform cameraToWorld;
	luxrays::Transform screenToCamera, screenToWorld;
	luxrays::Transform rasterToScreen, rasterToWorld;
	luxrays::Transform rasterToCamera;
};

}

#endif	/* _SLG_CAMERA_H */
