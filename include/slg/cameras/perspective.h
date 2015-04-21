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

#ifndef _SLG_PERSPECTIVE_CAMERA_H
#define	_SLG_PERSPECTIVE_CAMERA_H

#include "slg/cameras/projective.h"

namespace slg {

//------------------------------------------------------------------------------
// PerspectiveCamera
//------------------------------------------------------------------------------

class PerspectiveCamera : public ProjectiveCamera {
public:
	PerspectiveCamera(const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u, const float *region = NULL);

	virtual void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion = NULL);

	virtual void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2, const float u4) const;
	virtual bool GetSamplePosition(luxrays::Ray *eyeRay, float *filmX, float *filmY) const;
	virtual bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;

	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Stereo support
	//--------------------------------------------------------------------------

	void SetHorizontalStereo(const bool v) {
		if (v && !autoUpdateFilmRegion)
			throw std::runtime_error("Can not enable horizontal stereo support without film region auto-update");

		enableHorizStereo = v;
	}
	bool IsHorizontalStereoEnabled() const { return enableHorizStereo; }
	void SetOculusRiftBarrel(const bool v) {
		if (v && !enableHorizStereo)
			throw std::runtime_error("Can not enable Oculus Rift Barrel post-processing without horizontal stereo");

		enableOculusRiftBarrel = v;
	}
	bool IsOculusRiftBarrelEnabled() const { return enableOculusRiftBarrel; }
	void SetHorizontalStereoEyesDistance(const float v) { horizStereoEyesDistance = v; }
	float GetHorizontalStereoEyesDistance() const { return horizStereoEyesDistance; }
	void SetHorizontalStereoLensDistance(const float v) { horizStereoLensDistance = v; }
	float GetHorizontalStereoLensDistance() const { return horizStereoLensDistance; }

	//--------------------------------------------------------------------------
	// Oculus Rift post-processing pixel shader
	//--------------------------------------------------------------------------
	
	static void OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY);

	float fieldOfView;

private:
	void InitCameraTransforms(CameraTransforms *trans, const float screen[4],
		const float eyeOffset,
		const float screenOffsetX, const float screenOffsetY);

	float horizStereoEyesDistance, horizStereoLensDistance;
	bool enableHorizStereo, enableOculusRiftBarrel;
};

}

#endif	/* _SLG_PERSPECTIVE_CAMERA_H */
