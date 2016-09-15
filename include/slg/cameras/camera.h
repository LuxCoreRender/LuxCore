/***************************************************************************
 * Copyright 1998-2016 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/utils/mc.h"

#include "slg/film/film.h"

namespace slg {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
using namespace luxrays::ocl;
#include "slg/cameras/camera_types.cl"
}

class Scene;

class Camera {
public:
	typedef enum {
		PERSPECTIVE, ORTHOGRAPHIC, STEREO, ENVIRONMENT
	} CameraType;

	Camera(const CameraType t) : clipHither(1e-3f), clipYon(1e30f),
		shutterOpen(0.f), shutterClose(1.f), motionSystem(NULL), type(t) { }
	virtual ~Camera() {
		delete motionSystem;
	}

	CameraType GetType() const { return type; }
	virtual const luxrays::Vector GetDir() const = 0;
	virtual float GetPixelArea() const = 0;
	// Used for compiling camera information for OpenCL
	virtual luxrays::Matrix4x4 GetRasterToCameraMatrix(const u_int index = 0) const = 0;
	virtual luxrays::Matrix4x4 GetCameraToWorldMatrix(const u_int index = 0) const = 0;

	// Mostly used by GUIs
	virtual void Translate(const luxrays::Vector &t) = 0;
	virtual void TranslateLeft(const float t) = 0;
	virtual void TranslateRight(const float t) = 0;
	virtual void TranslateForward(const float t) = 0;
	virtual void TranslateBackward(const float t) = 0;

	// Mostly used by GUIs
	virtual void Rotate(const float angle, const luxrays::Vector &axis) = 0;
	virtual void RotateLeft(const float angle) = 0;
	virtual void RotateRight(const float angle) = 0;
	virtual void RotateUp(const float angle) = 0;
	virtual void RotateDown(const float angle) = 0;

	// Preprocess/update methods
	virtual void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion) = 0;
	virtual void UpdateFocus(const Scene *scene) = 0;

	// Rendering methods
	virtual void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2, const float u3) const = 0;
	virtual bool GetSamplePosition(luxrays::Ray *eyeRay,
		float *filmX, float *filmY) const = 0;
	virtual bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const = 0;
	virtual float GetPDF(const luxrays::Vector &eyeDir, const float filmX, const float filmY) const = 0;

	virtual luxrays::Properties ToProperties() const;

	static Camera *AllocCamera(const luxrays::Properties &props);

	// User defined values
	float clipHither, clipYon, shutterOpen, shutterClose;

	// For motion blur
	const luxrays::MotionSystem *motionSystem;

protected:
	const CameraType type;
};

}

#endif	/* _SLG_CAMERA_H */
