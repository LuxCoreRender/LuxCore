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

#ifndef _SLG_CLOUD_H
#define	_SLG_CLOUD_H

#include "slg/textures/texture.h"
#include "luxrays/core/randomgen.h"

namespace slg {

//------------------------------------------------------------------------------
// Cloud texture
//------------------------------------------------------------------------------

// Sphere for cumulus shape
typedef struct {
	luxrays::Point position;
	float radius;
} CumulusSphere;

class CloudTexture : public Texture {
public:
	CloudTexture(const TextureMapping3D *mp,
		const float r, const float noiseScale, const float t,
		const float sharp, const float v, const float baseflatness,
		const u_int octaves, const float o, const float offset,
		const u_int numspheres, const float spheresize);
	virtual ~CloudTexture() { }

	virtual TextureType GetType() const { return CLOUD_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	const float GetRadius() const { return radius; }
	const u_int GetNumSpheres() const { return numSpheres; }
	const u_int GetSphereSize() const { return sphereSize; }
	const float GetSharpness() const { return sharpness; }
	const float GetBaseFadeDistance() const { return baseFadeDistance; }
	const float GetBaseFlatness() const { return baseFlatness; }
	const float GetVariability() const { return variability; }
	const float GetOmega() const { return omega; }
	const float GetNoiseScale() const { return firstNoiseScale; }
	const float GetNoiseOffset() const { return noiseOffset; }
	const float GetTurbulenceAmount() const { return turbulenceAmount; }
	const u_int GetNumOctaves() const { return numOctaves; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	inline float CloudRand(luxrays::RandomGenerator &rng, const int resolution) const {
		return static_cast<float>(rng.uintValue() % resolution) / resolution;
	}

	float NoiseMask(const luxrays::Point &p) const { return CloudNoise(p / radius * 1.4f, omega, 1); }
	
	luxrays::Vector Turbulence(const luxrays::Point &p, const float noiseScale, const u_int octaves) const;
	luxrays::Vector Turbulence(const luxrays::Vector &v, const float noiseScale, const u_int octaves) const;
	
	float CloudNoise(const luxrays::Point &p, const float omegaValue, const u_int octaves) const;
	float CloudShape(const luxrays::Point &p) const;
	bool SphereFunction(const luxrays::Point &p) const;

	const luxrays::Vector scale;
	luxrays::Point sphereCentre;
	const float radius;

	bool cumulus;
	const u_int numSpheres;
	const float sphereSize;
	CumulusSphere *spheres;

	float baseFadeDistance;
	const float sharpness, baseFlatness, variability;
	const float omega, firstNoiseScale, noiseOffset, turbulenceAmount;
	const u_int numOctaves;

	const TextureMapping3D *mapping;
};

}

#endif	/* _SLG_CLOUD_H */
