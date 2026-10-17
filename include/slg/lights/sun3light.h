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

#ifndef _SLG_SUN3LIGHT_H
#define	 _SLG_SUN3LIGHT_H

#include <memory>

#include "slg/lights/light.h"
#include "slg/lights/pragueskyutils.h"

namespace slg {

//------------------------------------------------------------------------------
// Sun3 implementation
//
// A direct sun light source based on the "Prague Sky Model" (Wilkie et al.
// 2021, Vevoda et al. 2022) sun radiance, i.e. the sun disc radiance as seen
// after the atmospheric transmittance defined by the model. It is the natural
// companion of the sky3 sky light: together they reproduce what the sky3
// light used to include as a folded in sun disc, but with the efficient cone
// sampling of the classic sun light source.
//
// The radiance of the disc is evaluated, during Preprocess(), at the center
// of the sun disc and assumed constant over the disc (as in the model).
//------------------------------------------------------------------------------

class SunLight3 : public EnvLightSource {
public:
	SunLight3();
	virtual ~SunLight3() { }

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteSunDirData,
		float *xData, float *yData,
		float *cosThetaMaxData, float *sin2ThetaMaxData) const;

	virtual LightSourceType GetType() const { return TYPE_SUN3; }
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

	luxrays::Spectrum GetRadiance(SceneConstRef scene, const BSDF *bsdf,
			const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	luxrays::Spectrum color;

	std::string datasetFile;
	luxrays::Vector localSunDir;
	float visibility;		// in km
	float altitude;		// in m
	float groundAlbedo;		// in [0, 1]
	float relSize;

private:
	// Shared dataset (see GetPragueSkyModel())
	std::shared_ptr<PragueSkyModel> model;

	// Spectral channels of the dataset
	u_int channels;
	double channelStart, channelWidth;

	luxrays::Vector absoluteSunDir;
	// XY Vectors for cone sampling
	luxrays::Vector x, y;
	float cosThetaMax, sin2ThetaMax;
};

}

#endif	/* _SLG_SUN3LIGHT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
