/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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
	PerspectiveCamera(const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u, const float *region = NULL);
	~PerspectiveCamera() {
	}

	const void SetHorizontalStereo(const bool v) {
		if (v && !autoUpdateFilmRegion)
			throw std::runtime_error("Can not enable horizontal stereo support without film region auto-update");

		enableHorizStereo = v;
	}
	const bool IsHorizontalStereoEnabled() const { return enableHorizStereo; }
	const void SetOculusRiftBarrel(const bool v) {
		if (v && !enableHorizStereo)
			throw std::runtime_error("Can not enable Oculus Rift Barrel post-processing without horizontal stereo");

		enableOculusRiftBarrel = v;
	}
	const bool IsOculusRiftBarrelEnabled() const { return enableOculusRiftBarrel; }
	void SetHorizontalStereoEyeDistance(const float v) { horizStereoEyeDistance = v; }
	const float GetHorizontalStereoEyeDistance() const { return horizStereoEyeDistance; }
	void SetHorizontalStereoLensDistance(const float v) { horizStereoLensDistance = v; }
	const float GetHorizontalStereoLensDistance() const { return horizStereoLensDistance; }

	const luxrays::Vector &GetDir() const { return dir; }
	const luxrays::Vector &GetX() const { return x; }
	const luxrays::Vector &GetY() const { return y; }
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

	const luxrays::Matrix4x4 GetRasterToCameraMatrix(const u_int index) const {
		return camTrans[index].rasterToCamera.GetMatrix();
	}

	const luxrays::Matrix4x4 GetCameraToWorldMatrix(const u_int index) const {
		return camTrans[index].cameraToWorld.GetMatrix();
	}

	float GetClipYon() const { return clipYon; }
	float GetClipHither() const { return clipHither; }

	luxrays::Properties ToProperties() const;

	// User defined values
	luxrays::Point orig, target;
	luxrays::Vector up;
	float fieldOfView, clipHither, clipYon, lensRadius, focalDistance;

	//--------------------------------------------------------------------------
	// Oculus Rift post-processing pixel shader
	//--------------------------------------------------------------------------
	
	static void OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY);

private:
	typedef struct {
		luxrays::Transform cameraToWorld;
		luxrays::Transform screenToCamera, screenToWorld;
		luxrays::Transform rasterToScreen, rasterToWorld;
		luxrays::Transform rasterToCamera;
	} CameraTransforms;

	void InitCameraTransforms(CameraTransforms *trans, const float screen[4],
		const float eyeOffset,
		const float screenOffsetX, const float screenOffsetY);

	u_int filmWidth, filmHeight;

	// Calculated values
	float pixelArea;
	luxrays::Vector dir, x, y;

	// ProjectiveCamera Protected Data
	std::vector<CameraTransforms> camTrans;

	float filmRegion[4], horizStereoEyeDistance, horizStereoLensDistance;
	bool autoUpdateFilmRegion, enableHorizStereo, enableOculusRiftBarrel;

};

}

#endif	/* _SLG_CAMERA_H */
