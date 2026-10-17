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

#ifndef _SLG_SKY3LIGHT_H
#define	 _SLG_SKY3LIGHT_H

#include "slg/lights/light.h"
#include "slg/lights/visibility/envlightvisibilitycache.h"
#include "slg/lights/pragueskyutils.h"

namespace slg {

//------------------------------------------------------------------------------
// Sky3 implementation
//
// A sky dome light source based on the "Prague Sky Model" (Wilkie et al. 2021,
// Vevoda et al. 2022). It uses the full spectral dataset (sky radiance, up to
// 15 km observer altitude) and provides sky radiance converted to RGB via the
// CIE 1931 2 degree standard observer. The direct sun disc is not part of
// this light: use the dedicated sun3 light source, based on the same model,
// for it.
//
// The model is evaluated on the CPU only. For OpenCL/CUDA rendering the
// radiance of the dome is baked, during Preprocess(), into
// a lat-long ImageMap that the GPU kernels sample.
//------------------------------------------------------------------------------

class SkyLight3 : public EnvLightSource {
public:
	SkyLight3();
	virtual ~SkyLight3();

	virtual void Preprocess();

	// Called by Scene::CreateLightSource() after Preprocess() in order to
	// register the baked radiance ImageMap in the scene image map cache
	// (required by OpenCL/CUDA rendering)
	void RegisterRadianceImageMap(ImageMapCache &imgMapCache);

	std::tuple<luxrays::Distribution2DRef, EnvLightVisibilityCacheRPtr>
	GetPreprocessedData(float *absoluteUpDirData,
		float *scaledGroundColorData, int *hasGroundData) const;

	virtual void UpdateVisibilityMap(SceneConstRef scene, const bool useRTMode);

	virtual LightSourceType GetType() const { return TYPE_IL_SKY3; }
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

	virtual luxrays::Spectrum GetRadiance(SceneConstRef scene,
			const BSDF *bsdf, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;
	virtual luxrays::UV GetEnvUV(const luxrays::Vector &dir) const;

	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const {
		if (radianceMap)
			referencedImgMaps.insert(radianceMap.get());
	}

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	std::string datasetFile;
	luxrays::Vector localSunDir;
	float visibility;		// in km
	float altitude;			// in m
	float groundAlbedo;		// in [0, 1]
	luxrays::Spectrum groundColor;
	bool hasGround, hasGroundAutoScale;

	// Resolution of the baked lat-long radiance map (used by OpenCL/CUDA
	// rendering and for the light Distribution2D)
	u_int mapWidth, mapHeight;

	// Visibility map cache options
	ELVCParams visibilityMapCacheParams;
	bool useVisibilityMapCache;

	// Registered radiance ImageMap (null until RegisterRadianceImageMap() is
	// called)
	ImageMapPtr radianceMap;

private:
	luxrays::Spectrum ComputeSkyRadiance(const luxrays::Vector &w) const;
	luxrays::Spectrum ComputeRadiance(const luxrays::Vector &w) const;

	// Shared dataset (see GetPragueSkyModel())
	std::shared_ptr<PragueSkyModel> model;

	// Sun angles in the light local frame (as expected by the model)
	double localSunElevation, localSunAzimuth;

	// Spectral channels of the dataset
	u_int channels;
	double channelStart, channelWidth;

	luxrays::Vector absoluteSunDir, absoluteUpDir;
	luxrays::Spectrum scaledGroundColor;

	bool isGroundBlack;

	// Baked radiance map, moved to the scene image map cache by
	// RegisterRadianceImageMap()
	ImageMapUPtr radianceImageMap;

	luxrays::Distribution2DUPtr skyDistribution;

	EnvLightVisibilityCacheUPtr visibilityMapCache;
};

}

#endif	/* _SLG_SKY3LIGHT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
