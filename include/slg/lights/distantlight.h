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

#ifndef _SLG_DISTANTLIGHT_H
#define	_SLG_DISTANTLIGHT_H

#include "slg/lights/light.h"

namespace slg {

//------------------------------------------------------------------------------
// DistantLight implementation
//------------------------------------------------------------------------------

class DistantLight : public InfiniteLightSource {
public:
	DistantLight();
	virtual ~DistantLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteLightDirData, float *xData, float *yData,
		float *sin2ThetaMaxData, float *cosThetaMaxData) const;

	virtual LightSourceType GetType() const { return TYPE_DISTANT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	luxrays::Vector localLightDir;
	float theta;

protected:
	luxrays::Vector absoluteLightDir;
	luxrays::Vector x, y;
	float sin2ThetaMax, cosThetaMax;
};

}

#endif	/* _SLG_DISTANTLIGHT_H */
