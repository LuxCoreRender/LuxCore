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

#ifndef _SLG_ORTHOGRAPHIC_CAMERA_H
#define	_SLG_ORTHOGRAPHIC_CAMERA_H

#include "slg/cameras/camera.h"

namespace slg {

//------------------------------------------------------------------------------
// OrthographicCamera
//------------------------------------------------------------------------------

class OrthographicCamera : public Camera {
public:
	OrthographicCamera(const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u, const float *region = NULL);

	void SetClippingPlane(const bool v) {
		enableClippingPlane = v;
	}
	bool IsClippingPlaneEnabled() const { return enableClippingPlane; }
	

	const luxrays::Vector GetDir() const { return dir; }
	float GetPixelArea() const { return pixelArea; }

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

	void RotateLeft(const float angle) {
		Rotate(angle, y);
	}

	void RotateRight(const float angle) {
		Rotate(-angle, y);
	}

	void RotateUp(const float angle) {
		Rotate(angle, x);
	}

	void RotateDown(const float angle) {
		Rotate(-angle, x);
	}

	virtual void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion = NULL);
	virtual void UpdateFocus(const Scene *scene);
	void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2, const float u4) const;
	bool GetSamplePosition(luxrays::Ray *eyeRay, float *filmX, float *filmY) const;

	bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;

	const luxrays::Matrix4x4 GetRasterToCameraMatrix(const u_int index) const {
		return camTrans.rasterToCamera.GetMatrix();
	}

	const luxrays::Matrix4x4 GetCameraToWorldMatrix(const u_int index) const {
		return camTrans.cameraToWorld.GetMatrix();
	}

	luxrays::Properties ToProperties() const;

	// User defined values
	luxrays::Point orig, target;
	luxrays::Vector up;

	// World clipping plane
	luxrays::Point clippingPlaneCenter;
	luxrays::Normal clippingPlaneNormal;

private:
	typedef struct {
		// Note: all *ToWorld don't include camera motion blur
		luxrays::Transform cameraToWorld;
		luxrays::Transform screenToCamera, screenToWorld;
		luxrays::Transform rasterToScreen, rasterToWorld;
		luxrays::Transform rasterToCamera;
	} CameraTransforms;

	void InitCameraTransforms(CameraTransforms *trans, const float screen[4],		
		const float screenOffsetX, const float screenOffsetY);
	void ApplyArbitraryClippingPlane(luxrays::Ray *ray) const;

	// A copy of Film values
	u_int filmWidth, filmHeight;

	// Calculated values
	float pixelArea;
	luxrays::Vector dir, x, y;

	CameraTransforms camTrans;
	
	float filmRegion[4];
	bool autoUpdateFilmRegion, enableClippingPlane;
};

}

#endif	/* _SLG_ORTHOGRAPHIC_CAMERA_H */
