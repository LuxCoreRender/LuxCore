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

#ifndef _SLG_CAMERA_H
#define	_SLG_CAMERA_H

#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/utils/mc.h"

#include "slg/imagemap/imagemapcache.h"

namespace slg {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
using namespace luxrays::ocl;
#include "slg/cameras/camera_types.cl"
}

class Scene;
class Volume;
class PathVolumeInfo;

class Camera {
public:
	typedef enum {
		PERSPECTIVE, ORTHOGRAPHIC, STEREO, ENVIRONMENT
	} CameraType;

	Camera(const CameraType t) : clipHither(1e-3f), clipYon(1e30f),
		shutterOpen(0.f), shutterClose(1.f), autoVolume(true), volume(NULL),
		motionSystem(NULL), type(t) { }
	virtual ~Camera() {
		delete motionSystem;
	}

	CameraType GetType() const { return type; }
	
	// Returns the bounding box of all possible ray origins for this camera
	virtual luxrays::BBox GetBBox() const = 0;

	virtual const luxrays::Vector GetDir() const = 0;
	// Used for compiling camera information for OpenCL (and more)
	virtual const luxrays::Transform &GetRasterToCamera(const u_int index = 0) const = 0;
	virtual const luxrays::Transform &GetCameraToWorld(const u_int index = 0) const = 0;
	virtual const luxrays::Transform &GetScreenToWorld(const u_int index = 0) const = 0;

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
		const u_int *filmSubRegion);
	virtual void UpdateAuto(const Scene *scene);

	// Rendering methods
	float GenerateRayTime(const float u) const { return luxrays::Lerp(u, shutterOpen, shutterClose); }
	virtual void GenerateRay(const float time, const float filmX, const float filmY,
		luxrays::Ray *ray, PathVolumeInfo *volInfo,
		const float u0, const float u1) const = 0;
	
	// Used for connecting light paths to the camera
	virtual void ClampRay(luxrays::Ray *ray) const { }
	virtual bool GetSamplePosition(luxrays::Ray *eyeRay,
		float *filmX, float *filmY) const = 0;
	virtual bool GetSamplePosition(const luxrays::Point &p,
		float *filmX, float *filmY) const;
	virtual bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const = 0;
	virtual void GetPDF(const luxrays::Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const = 0;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;
	virtual void UpdateVolumeReferences(const Volume *oldVol, const Volume *newVol);

	static Camera *AllocCamera(const luxrays::Properties &props);

	// User defined values
	float clipHither, clipYon, shutterOpen, shutterClose;

	bool autoVolume;
	const Volume *volume;

	// For motion blur
	const luxrays::MotionSystem *motionSystem;
	
	// A copy of Film values
	u_int filmWidth, filmHeight;
	u_int filmSubRegion[4];

protected:
	// An utility methods for computing the bounding box
	luxrays::BBox ComputeBBox(const luxrays::Point &orig) const;

	const CameraType type;
};

}

#endif	/* _SLG_CAMERA_H */
