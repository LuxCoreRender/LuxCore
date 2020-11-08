/***************************************************************************
 * Copyright 1998-2016 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _SLG_ENVIRONMENT_CAMERA_H
#define	_SLG_ENVIRONMENT_CAMERA_H

#include "slg/cameras/camera.h"

namespace slg {

//------------------------------------------------------------------------------
// EnvironmentCamera
//------------------------------------------------------------------------------

class EnvironmentCamera : public Camera {
public:
	EnvironmentCamera(const luxrays::Point &o, const luxrays::Point &t,
		const luxrays::Vector &u, const float *sw = NULL);
	virtual ~EnvironmentCamera() { }

	virtual luxrays::BBox GetBBox() const { return ComputeBBox(orig); }
	virtual const luxrays::Vector GetDir() const { return dir; }
	virtual float GetPixelArea() const { return pixelArea; }
	virtual const luxrays::Transform &GetRasterToCamera(const u_int index = 0) const {
		return camTrans.rasterToCamera;
	}
	virtual const luxrays::Transform &GetCameraToWorld(const u_int index = 0) const {
		return camTrans.cameraToWorld;
	}
	virtual const luxrays::Transform &GetScreenToWorld(const u_int index = 0) const {
		return camTrans.screenToWorld;
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

	virtual void Rotate(const float angle, const luxrays::Vector &axis);

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
	virtual void Update(const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion);
	virtual void UpdateAuto(const Scene *scene) { };

	// Rendering methods
	virtual void GenerateRay(const float time,
		const float filmX, const float filmY,
		luxrays::Ray *ray, PathVolumeInfo *volInfo,
		const float u1, const float u2) const;

	// Used for connecting light paths to the camera
	virtual void ClampRay(luxrays::Ray *ray) const;
	virtual bool GetSamplePosition(luxrays::Ray *eyeRay, float *filmX, float *filmY) const;
	virtual bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;
	virtual void GetPDF(const luxrays::Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	float screenOffsetX, screenOffsetY;
	float degrees;

protected:
	typedef struct {
		// Note: all *ToWorld don't include camera motion blur
		luxrays::Transform cameraToWorld;
		luxrays::Transform screenToCamera, screenToWorld;
		luxrays::Transform rasterToScreen, rasterToWorld;
		luxrays::Transform rasterToCamera;
	} CameraTransforms;

	float screenWindow[4];
	bool autoUpdateScreenWindow;
	
private:
	// Calculated values
	luxrays::Point orig, target, rayOrigin;
	luxrays::Vector up;

	float pixelArea;
	luxrays::Vector dir, x, y;
	CameraTransforms camTrans;

	void InitCameraTransforms(CameraTransforms *trans);
	void InitPixelArea();
	void InitRay(luxrays::Ray *ray, const float filmX, const float filmY) const;
};

}
#endif	/* _SLG_ENVIRONMENT_CAMERA_H */

