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

#ifndef _SLG_SPHERELIGHT_H
#define	_SLG_SPHERELIGHT_H

#include "luxrays/core/geometry/ray.h"

#include "slg/lights/pointlight.h"

namespace slg {

//------------------------------------------------------------------------------
// SphereLight implementation
//------------------------------------------------------------------------------

class SphereLight : public PointLight {
public:
	SphereLight();
	virtual ~SphereLight();

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_SPHERE; }

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		luxrays::Ray &ray, float &emissionPdfW,
		float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        luxrays::Ray &shadowRay, float &directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	float radius;

protected:
	bool SphereIntersect(const luxrays::Ray &ray, float &hitT) const;

	float radiusSquared, invArea;
};

}

#endif	/* _SLG_SPHERELIGHT_H */
