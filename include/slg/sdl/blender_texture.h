/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_BLENDER_TEXTURE_H
#define	_SLG_BLENDER_TEXTURE_H

#include "slg/sdl/texture.h"

namespace slg {

typedef enum {
    BLENDER_ORIGINAL, ORIGINAL_PERLIN, IMPROVED_PERLIN,
    VORONOI_F1, VORONOI_F2, VORONOI_F3, VORONOI_F4, VORONOI_F2_F1,
    VORONOI_CRACKLE, CELL_NOISE
} BlenderNoiseBasis;

//------------------------------------------------------------------------------
// Blender wood texture
//------------------------------------------------------------------------------

typedef enum {
	BANDS, RINGS, BANDNOISE, RINGNOISE
} BlenderWoodType;

typedef enum {
	TEX_SIN, TEX_SAW, TEX_TRI
} BlenderWoodNoiseBase;

class BlenderWoodTexture : public Texture {
public:
	BlenderWoodTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoise, const float noisesize, float turb, bool hard, float bright, float contrast);
	virtual ~BlenderWoodTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_WOOD; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	BlenderWoodType GetWoodType() const { return type; }
	BlenderWoodNoiseBase GetNoiseBasis2() const { return noisebasis2; }
	float GetNoiseSize() const { return noisesize; }
	float GetTurbulence() const { return turbulence; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	bool GetNoiseType() const { return hard; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	BlenderWoodType type;
	BlenderWoodNoiseBase noisebasis2;	
	float noisesize, turbulence;
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

class BlenderCloudsTexture : public Texture {
public:
	BlenderCloudsTexture(const TextureMapping3D *mp, const float noisesize, const int noisedepth, bool hard, float bright, float contrast);
	virtual ~BlenderCloudsTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_CLOUDS; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	float GetNoiseSize() const { return noisesize; }
	int GetNoiseDepth() const { return noisedepth; }
	bool GetNoiseType() const { return hard; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	int noisedepth;	
	float noisesize;	
	bool hard;
	float bright, contrast;
};

//------------------------------------------------------------------------------
// Blender voronoi texture
//------------------------------------------------------------------------------

class BlenderVoronoiTexture : public Texture {
public:
	BlenderVoronoiTexture(const TextureMapping3D *mp, const float intensity, const float exponent, const float fw1, const float fw2, const float fw3, const float fw4, 
		const DistanceMetric distmetric, const float noisesize, float bright, float contrast);
	virtual ~BlenderVoronoiTexture() { delete mapping; }

	virtual TextureType GetType() const { return BLENDER_VORONOI; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	float GetBright() const { return bright; }
	float GetContrast() const { return contrast; }
	string GetDistMetric() const {
		std::string result = "";
		switch(distancemetric) {
			case DISTANCE_SQUARED:
				result = "distance_squared";
			case MANHATTAN:
				result = "manhattan";
			case CHEBYCHEV:
				result = "chebychev";
			case MINKOWSKI_HALF:
				result = "minkowski_half";
			case MINKOWSKI_FOUR:
				result = "minkowski_four";
			case MINKOWSKI:
				result = "minkowski";
			case ACTUAL_DISTANCE: 
			default:
				result = "actual_distance";

			return result;
		}
	}
	float GetFeatureWeight1() const { return feature_weight1; }
	float GetFeatureWeight2() const { return feature_weight2; }
	float GetFeatureWeight3() const { return feature_weight3; }
	float GetFeatureWeight4() const { return feature_weight4; }
	float GetNoiseSize() const { return noisesize; }
	float GetIntensity() const { return intensity; }
	float GetExponent() const { return exponent; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;

	float intensity;
	float feature_weight1;
	float feature_weight2;
	float feature_weight3;
	float feature_weight4;
	DistanceMetric distancemetric;
	float exponent;
	float noisesize;
	float bright, contrast;
};

}

#endif	/* _SLG_BLENDER_TEXTURE_H */
