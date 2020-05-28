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

#ifndef _SLG_VOLUME_H
#define	_SLG_VOLUME_H

#include "luxrays/luxrays.h"
#include "slg/materials/material.h"
#include "slg/textures/texture.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/volumes/volume_types.cl"
}

class BSDF;
	
class Volume : public Material {
public:
	Volume(const Texture *ior, const Texture *emission) : Material(NULL, NULL, NULL, NULL),
			iorTex(ior), volumeEmissionTex(emission), volumeLightID(0), priority(0) { }
	virtual ~Volume() { }

	void SetVolumeLightID(const u_int id) { volumeLightID = id; }
	u_int GetVolumeLightID() const { return volumeLightID; }
	void SetPriority(const int p) { priority = p; }
	int GetPriority() const { return priority; }

	const Texture *GetIORTexture() const { return iorTex; }
	const Texture *GetVolumeEmissionTexture() const { return volumeEmissionTex; }

	float GetIOR(const HitPoint &hitPoint) const { return iorTex->GetFloatValue(hitPoint); }

	// Returns the ray t value of the scatter event. If (t <= 0.0) there was
	// no scattering. In any case, it applies transmittance to connectionThroughput
	// too.
	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredStart,
		luxrays::Spectrum *connectionThroughput, luxrays::Spectrum *connectionEmission) const = 0;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	friend class SchlickScatter;

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const = 0;
	luxrays::Spectrum SigmaT(const HitPoint &hitPoint) const {
		return SigmaA(hitPoint) + SigmaS(hitPoint);
	}
	luxrays::Spectrum Emission(const HitPoint &hitPoint) const {
		return (volumeEmissionTex) ? volumeEmissionTex->GetSpectrumValue(hitPoint).Clamp() : luxrays::Spectrum(0.f);
	}

	const Texture *iorTex;
	// This is a different kind of emission texture from the one in
	// Material class (i.e. is not sampled by direct light).
	const Texture *volumeEmissionTex;
	u_int volumeLightID;
	int priority;
};

// An utility class
class SchlickScatter {
public:
	SchlickScatter(const Volume *volume, const Texture *g);

	luxrays::Spectrum Albedo(const HitPoint &hitPoint) const;

	luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const;
	void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	const Volume *volume;
	const Texture *g;

private:
	luxrays::Spectrum GetColor(const HitPoint &hitPoint) const;
};

}

#endif	/* _SLG_VOLUME_H */
