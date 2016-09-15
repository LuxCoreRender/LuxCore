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

#ifndef _SLG_SKYLIGHT_H
#define	_SLG_SKYLIGHT_H

#include "slg/lights/light.h"

namespace slg {

//------------------------------------------------------------------------------
// Sky implementation
//------------------------------------------------------------------------------

class SkyLight : public EnvLightSource {
public:
	SkyLight();
	virtual ~SkyLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteSunDirData,
		float *absoluteThetaData, float *absolutePhiData,
		float *zenith_YData, float *zenith_xData, float *zenith_yData,
		float *perez_YData, float *perez_xData, float *perez_yData) const;

	virtual LightSourceType GetType() const { return TYPE_IL_SKY; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Vector localSunDir;
	float turbidity;

private:
	void GetSkySpectralRadiance(const float theta, const float phi, luxrays::Spectrum * const spect) const;

	luxrays::Vector absoluteSunDir;
	float absoluteTheta, absolutePhi;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
};

}

#endif	/* _SLG_SKYLIGHT_H */
