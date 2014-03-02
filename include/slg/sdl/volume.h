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

#ifndef _SLG_VOLUME_H
#define	_SLG_VOLUME_H

#include "luxrays/luxrays.h"
#include "slg/sdl/material.h"
#include "slg/sdl/texture.h"

namespace slg {

class BSDF;
	
class Volume : public Material {
public:
	Volume() : Material(NULL, NULL), priority(0) { }
	virtual ~Volume() { }

	void SetPriority(const int p) { priority = p; }
	int GetPriority() const { return priority; }

	// Returns the ray t value of the scatter event. If (t <= 0.0) there was
	// no scattering.
	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredPath,
		luxrays::Spectrum *connectionThroughput) const = 0;

	friend class SchlickScatter;

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum SigmaT(const HitPoint &hitPoint) const {
		return SigmaA(hitPoint) + SigmaS(hitPoint);
	}
	virtual luxrays::Spectrum Tau(const luxrays::Ray &ray, const float offset) const = 0;

	int priority;
};

// An utility class
class SchlickScatter {
public:
	SchlickScatter(const Volume *volume, const Texture *g);

	luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	const Volume *volume;
	const Texture *g;	
};

// A class used to store volume related information on the on going path
#define PATHVOLUMEINFO_SIZE 8

class PathVolumeInfo {
public:
	PathVolumeInfo();

	const Volume *GetCurrentVolume() const { return currentVolume; }
	const u_int GetListSize() const { return volumeListSize; }

	void AddVolume(const Volume *v);
	void RemoveVolume(const Volume *v);

	void Scattered() { scatteredPath = true; }
	bool IsScattered() const { return scatteredPath; }
	
	void Update(const BSDFEvent eventType, const BSDF &bsdf);
	bool ContinueToTrace(const BSDF &bsdf) const;

private:
	static bool CompareVolumePriorities(const Volume *vol1, const Volume *vol2);

	const Volume *currentVolume;
	// Using a fixed array here mostly to have the some code of OpenCL implementation
	const Volume *volumeList[PATHVOLUMEINFO_SIZE];
	u_int volumeListSize;
	
	bool scatteredPath;
};

//------------------------------------------------------------------------------
// HomogeneousVolume
//------------------------------------------------------------------------------

class HomogeneousVolume : public Volume {
public:
	HomogeneousVolume(const Texture *a, const Texture *s,
			const Texture *g, const bool multiScattering);

	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredPath,
		luxrays::Spectrum *connectionThroughput) const;

	// Material interface

	virtual MaterialType GetType() const { return HOMOGENEOUS_VOL; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum Tau(const luxrays::Ray &ray, const float maxt) const;

private:
	const Texture *sigmaA, *sigmaS;
	SchlickScatter schlickScatter;
	const bool multiScattering;
};

}

#endif	/* _SLG_VOLUME_H */
