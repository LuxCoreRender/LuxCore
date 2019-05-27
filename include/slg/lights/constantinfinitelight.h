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

#ifndef _SLG_CONSTANTINFINITELIGHT_H
#define	_SLG_CONSTANTINFINITELIGHT_H

#include "slg/lights/light.h"
#include "slg/lights/visibility/envlightvisibilitycache.h"

namespace slg {

//------------------------------------------------------------------------------
// ConstantInfiniteLight implementation
//------------------------------------------------------------------------------

class ConstantInfiniteLight : public EnvLightSource {
public:
	ConstantInfiniteLight();
	virtual ~ConstantInfiniteLight();

	virtual void Preprocess() { }
	void GetPreprocessedData(const luxrays::Distribution2D **visibilityDistribution) const;

	virtual void UpdateVisibilityMap(const Scene *scene);

	virtual LightSourceType GetType() const { return TYPE_IL_CONSTANT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const Scene &scene,
			const luxrays::Point &p, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;
	virtual luxrays::UV GetEnvUV(const luxrays::Vector &dir) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	luxrays::Spectrum color;

	// Visibility map options
	u_int visibilityMapWidth, visibilityMapHeight;
	u_int visibilityMapSamples, visibilityMapMaxDepth;
	bool useVisibilityMap;

	// Visibility map cache options
	ELVCParams visibilityMapCacheParams;
	bool useVisibilityMapCache;

private:
	luxrays::Distribution2D *visibilityDistribution;

	EnvLightVisibilityCache *visibilityMapCache;
};

}

#endif	/* _SLG_CONSTANTINFINITELIGHT_H */
