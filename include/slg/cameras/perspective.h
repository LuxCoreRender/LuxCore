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

#ifndef _SLG_PERSPECTIVE_CAMERA_H
#define	_SLG_PERSPECTIVE_CAMERA_H

#include <string>

#include "slg/cameras/projective.h"

namespace slg {

//------------------------------------------------------------------------------
// PerspectiveCamera
//------------------------------------------------------------------------------

class PerspectiveCamera : public ProjectiveCamera {
public:
	typedef enum {
		DIST_UNIFORM,
		DIST_EXPONETIAL,
		DIST_INVERSEEXPONETIAL,
		DIST_GAUSSIAN,
		DIST_INVERSEGAUSSIAN,
		DIST_TRIANGULAR
	} BokehDistributionType;

	PerspectiveCamera(const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u, const float *screenWindow = NULL);
	virtual ~PerspectiveCamera() { }

	virtual void ClampRay(luxrays::Ray *ray) const;
	virtual bool GetSamplePosition(luxrays::Ray *eyeRay, float *filmX, float *filmY) const;
	virtual bool LocalSampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;
	virtual bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;
	virtual void GetPDF(const luxrays::Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const;

	virtual luxrays::Properties ToProperties() const;

	static BokehDistributionType String2BokehDistributionType(std::string type);
	static std::string BokehDistributionType2String(const BokehDistributionType type);
	
	float screenOffsetX, screenOffsetY;
	float fieldOfView;

	u_int bokehBlades, bokehPower;
	BokehDistributionType bokehDistribution;

	bool enableOculusRiftBarrel;

protected:
	// Used by sub-classes
	PerspectiveCamera(const CameraType type, const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u, const float *region = NULL);

private:
	static void OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY);

	virtual void InitCameraTransforms(CameraTransforms *trans);
	virtual void InitCameraData();
	virtual void InitRay(luxrays::Ray *ray, const float filmX, const float filmY) const;

	float pixelArea;
};

}

#endif	/* _SLG_PERSPECTIVE_CAMERA_H */
