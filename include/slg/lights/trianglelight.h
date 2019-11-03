/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_TRIANGLELIGHT_H
#define	_SLG_TRIANGLELIGHT_H

#include "slg/lights/light.h"

namespace slg {

//------------------------------------------------------------------------------
// TriangleLight implementation
//------------------------------------------------------------------------------

class TriangleLight : public IntersectableLightSource {
public:
	TriangleLight();
	virtual ~TriangleLight();

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_TRIANGLE; }

	virtual bool IsDirectLightSamplingEnabled() const;

	float GetTriangleArea() const { return triangleArea; }
	float GetMeshArea() const { return meshArea; }
	
	virtual float GetArea() const { return triangleArea; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		luxrays::Ray &ray, float &emissionPdfW,
		float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        luxrays::Ray &shadowRay, float &directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual bool IsAlwaysInShadow(const Scene &scene,
			const luxrays::Point &p, const luxrays::Normal &n) const;

	virtual luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;

	const luxrays::ExtMesh *mesh;
	u_int meshIndex, triangleIndex;
	
private:
	float triangleArea, invTriangleArea;
	float meshArea, invMeshArea;
};

}

#endif	/* _SLG_TRIANGLELIGHT_H */
