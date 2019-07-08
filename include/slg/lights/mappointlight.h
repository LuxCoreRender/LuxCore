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

#ifndef _SLG_MAPPOINTLIGHT_H
#define	_SLG_MAPPOINTLIGHT_H

#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/lights/light.h"
#include "slg/lights/pointlight.h"

namespace slg {

//------------------------------------------------------------------------------
// MapPointLight implementation
//------------------------------------------------------------------------------

class MapPointLight : public PointLight {
public:
	MapPointLight();
	virtual ~MapPointLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *localPos, float *absolutePos, float *emittedFactor,
		const SampleableSphericalFunction **funcData) const;

	virtual LightSourceType GetType() const { return TYPE_MAPPOINT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const BSDF &bsdf,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imageMap);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	const ImageMap *imageMap;

private:
	SampleableSphericalFunction *func;
};

}

#endif	/* _SLG_MAPPOINTLIGHT_H */
