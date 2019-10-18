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

#ifndef _SLG_SHARPDISTANTLIGHT_H
#define	_SLG_SHARPDISTANTLIGHT_H

#include "slg/lights/light.h"

namespace slg {

//------------------------------------------------------------------------------
// SharpDistantLight implementation
//------------------------------------------------------------------------------

class SharpDistantLight : public InfiniteLightSource {
public:
	SharpDistantLight();
	virtual ~SharpDistantLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteLightDirData, float *xData, float *yData) const;

	virtual LightSourceType GetType() const { return TYPE_SHARPDISTANT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		luxrays::Point &rayOrig, luxrays::Vector &rayDir, float &emissionPdfW,
		float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector &shadowRayDir, float &shadowRayDistance, float &directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;
	
	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	luxrays::Spectrum color;
	luxrays::Vector localLightDir;

protected:
	luxrays::Vector absoluteLightDir;
	luxrays::Vector x, y;
};

}

#endif	/* _SLG_SHARPDISTANTLIGHT_H */
