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

#include "slg/cameras/projective.h"

namespace slg {

//------------------------------------------------------------------------------
// OrthographicCamera
//------------------------------------------------------------------------------

class OrthographicCamera : public ProjectiveCamera {
public:
	OrthographicCamera(const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u, const float *region = NULL);

	virtual void Update(const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion = NULL);
	void GenerateRay(
		const float filmX, const float filmY,
		luxrays::Ray *ray, const float u1, const float u2, const float u4) const;
	bool GetSamplePosition(luxrays::Ray *eyeRay, float *filmX, float *filmY) const;

	bool SampleLens(const float time, const float u1, const float u2,
		luxrays::Point *lensPoint) const;

	luxrays::Properties ToProperties() const;

private:
	void InitCameraTransforms(CameraTransforms *trans, const float screen[4],
		const float screenOffsetX, const float screenOffsetY);
};

}

#endif	/* _SLG_ORTHOGRAPHIC_CAMERA_H */
