/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_INFINITELIGHT_H
#define	_SLG_INFINITELIGHT_H

#include "slg/lights/light.h"
#include "slg/lights/visibility/envlightvisibilitycache.h"

namespace slg {

//------------------------------------------------------------------------------
// InfiniteLight implementation
//------------------------------------------------------------------------------

class InfiniteLight : public EnvLightSource {
public:
	InfiniteLight();
	virtual ~InfiniteLight();

	virtual void Preprocess();

	std::tuple<luxrays::Distribution2DRef, EnvLightVisibilityCacheRPtr>
	GetPreprocessedData() const;

	virtual void UpdateVisibilityMap(SceneConstRef scene, const bool useRTMode);

	virtual LightSourceType GetType() const { return TYPE_IL; }
	virtual float GetPower(SceneConstRef scene) const;

	virtual luxrays::Spectrum Emit(SceneConstRef scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		luxrays::Ray &ray, float &emissionPdfW,
		float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(SceneConstRef scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        luxrays::Ray &shadowRay, float &directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(SceneConstRef scene, const BSDF *bsdf,
		const luxrays::Vector &dir,
		float *directPdfA = NULL, float *emissionPdfW = NULL) const;
	virtual luxrays::UV GetEnvUV(const luxrays::Vector &dir) const;

	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imageMap.get());
	}

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	ImageMapConstPtr imageMap;
	bool sampleUpperHemisphereOnly;

	// Visibility map cache options
	ELVCParams visibilityMapCacheParams;
	bool useVisibilityMapCache;

private:
	std::unique_ptr<luxrays::Distribution2D> imageMapDistribution;

	std::unique_ptr<EnvLightVisibilityCache> visibilityMapCache;
};

}

#endif	/* _SLG_INFINITELIGHT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
