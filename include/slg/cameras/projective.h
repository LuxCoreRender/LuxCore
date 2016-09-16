/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_PROJECTIVE_CAMERA_H
#define	_SLG_PROJECTIVE_CAMERA_H

#include "slg/cameras/camera.h"

namespace slg {

class ProjectiveCamera : public Camera {
public:
	ProjectiveCamera(const CameraType type, const float *screenWindow,
			const luxrays::Point &orig, const luxrays::Point &target,
			const luxrays::Vector &up);
	virtual ~ProjectiveCamera() {
	}

	virtual const luxrays::Vector GetDir() const { return dir; }
	virtual float GetPixelArea() const { return pixelArea; }
	virtual luxrays::Matrix4x4 GetRasterToCameraMatrix(const u_int index = 0) const {
		return camTrans.rasterToCamera.GetMatrix();
	}
	virtual luxrays::Matrix4x4 GetCameraToWorldMatrix(const u_int index = 0) const {
		return camTrans.cameraToWorld.GetMatrix();
	}

	// Mostly used by GUIs
	
	virtual void Translate(const luxrays::Vector &t) {
		orig += t;
		target += t;
	}

	virtual void TranslateLeft(const float k) {
		luxrays::Vector t = -k * luxrays::Normalize(x);
		Translate(t);
	}

	virtual void TranslateRight(const float k) {
		luxrays::Vector t = k * luxrays::Normalize(x);
		Translate(t);
	}

	virtual void TranslateForward(const float k) {
		luxrays::Vector t = k * dir;
		Translate(t);
	}

	virtual void TranslateBackward(const float k) {
		luxrays::Vector t = -k * dir;
		Translate(t);
	}

	virtual void Rotate(const float angle, const luxrays::Vector &axis) {
		luxrays::Vector p = target - orig;
		luxrays::Transform t = luxrays::Rotate(angle, axis);
		target = orig + t * p;
	}

	virtual void RotateLeft(const float angle) {
		Rotate(angle, y);
	}

	virtual void RotateRight(const float angle) {
		Rotate(-angle, y);
	}

	virtual void RotateUp(const float angle) {
		Rotate(angle, x);
	}

	virtual void RotateDown(const float angle) {
		Rotate(-angle, x);
	}

	// Preprocess/update methods
	virtual void UpdateFocus(const Scene *scene);
	virtual void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion);

	// Rendering methods
	virtual void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2, const float u3) const;

	virtual luxrays::Properties ToProperties() const;

	// User defined values
	luxrays::Point orig, target;
	luxrays::Vector up;

	// World clipping plane
	luxrays::Point clippingPlaneCenter;
	luxrays::Normal clippingPlaneNormal;
	bool enableClippingPlane;

	// User defined values
	float lensRadius, focalDistance;
	bool autoFocus;

protected:
	typedef struct {
		// Note: all *ToWorld don't include camera motion blur
		luxrays::Transform cameraToWorld;
		luxrays::Transform screenToCamera, screenToWorld;
		luxrays::Transform rasterToScreen, rasterToWorld;
		luxrays::Transform rasterToCamera;
	} CameraTransforms;
	
	virtual void InitCameraTransforms(CameraTransforms *trans) = 0;
	virtual void InitPixelArea() = 0;
	virtual void InitRay(luxrays::Ray *ray, const float filmX, const float filmY) const = 0;

	void ApplyArbitraryClippingPlane(luxrays::Ray *ray) const;

	float screenWindow[4];
	bool autoUpdateScreenWindow;

	// A copy of Film values
	u_int filmWidth, filmHeight;
	float filmSubRegion[4];

	// Calculated values
	float pixelArea;
	luxrays::Vector dir, x, y;
	CameraTransforms camTrans;
};

}

#endif	/* _SLG_PROJECTIVE_CAMERA_H */
