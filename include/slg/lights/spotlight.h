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

#ifndef _SLG_SPOTLIGHT_H
#define	_SLG_SPOTLIGHT_H

#include "slg/lights/light.h"

namespace slg {

//------------------------------------------------------------------------------
// SpotLight implementation
//------------------------------------------------------------------------------

class SpotLight : public NotIntersectableLightSource {
public:
	SpotLight();
	virtual ~SpotLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *emittedFactor, float *absolutePos,
		float *cosTotalWidth, float *cosFalloffStart,
		const luxrays::Transform **alignedLight2World) const;

	virtual LightSourceType GetType() const { return TYPE_SPOT; }
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

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	luxrays::Spectrum color;
	float power, efficiency;
	bool emittedPowerNormalize;

	luxrays::Point localPos, localTarget;
	float coneAngle, coneDeltaAngle;

protected:
	luxrays::Spectrum emittedFactor;
	luxrays::Point absolutePos;
	float cosTotalWidth, cosFalloffStart;
	luxrays::Transform alignedLight2World;
};

}

#endif	/* _SLG_SPOTLIGHT_H */
