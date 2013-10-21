/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_CAMERA_H
#define	_SLG_CAMERA_H

#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/transform.h"
#include "slg/core/mc.h"
#include "slg/film/film.h"

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
	void SetHorizontalStereoEyesDistance(const float v) { horizStereoEyesDistance = v; }
	const float GetHorizontalStereoEyesDistance() const { return horizStereoEyesDistance; }
	void SetHorizontalStereoLensDistance(const float v) { horizStereoLensDistance = v; }
	const float GetHorizontalStereoLensDistance() const { return horizStereoLensDistance; }

	const luxrays::Vector &GetDir() const { return dir; }
	const luxrays::Vector &GetX() const { return x; }
	const luxrays::Vector &GetY() const { return y; }
	const float GetPixelArea() const { return pixelArea; }

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

	void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion = NULL);

	void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2) const;
	bool GetSamplePosition(const luxrays::Point &p, const luxrays::Vector &wi,
		float distance, float *filmX, float *filmY) const;

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

	// A copy of Film values
	u_int filmWidth, filmHeight;

	// Calculated values
	float pixelArea;
	luxrays::Vector dir, x, y;

	// ProjectiveCamera Protected Data
	std::vector<CameraTransforms> camTrans;

	float filmRegion[4], horizStereoEyesDistance, horizStereoLensDistance;
	bool autoUpdateFilmRegion, enableHorizStereo, enableOculusRiftBarrel;

};

}

#endif	/* _SLG_CAMERA_H */
