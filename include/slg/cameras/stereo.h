/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_STEREO_CAMERA_H
#define	_SLG_STEREO_CAMERA_H

#include "slg/cameras/camera.h"
#include "slg/cameras/perspective.h"

namespace slg {

class StereoCamera : public PerspectiveCamera {
public:
	StereoCamera(const luxrays::Point &orig, const luxrays::Point &target,
			const luxrays::Vector &up);
	virtual ~StereoCamera();

	const luxrays::Transform &GetRasterToCamera(const u_int index = 0) const;
	const luxrays::Transform &GetCameraToWorld(const u_int index = 0) const;
	const luxrays::Transform &GetScreenToWorld(const u_int index = 0) const;

	// Preprocess/update methods
	virtual void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion);

	// Rendering methods
	virtual void GenerateRay(const float time,
		const float filmX, const float filmY,
		luxrays::Ray *ray, PathVolumeInfo *volInfo,
		const float u1, const float u2) const;
	virtual bool GetSamplePosition(luxrays::Ray *eyeRay, float *filmX, float *filmY) const;
	virtual bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;
	virtual void GetPDF(const luxrays::Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	float horizStereoEyesDistance, horizStereoLensDistance;

private:
	PerspectiveCamera *leftEye;
	PerspectiveCamera *rightEye;
};

}

#endif	/* _SLG_CAMERA_H */
