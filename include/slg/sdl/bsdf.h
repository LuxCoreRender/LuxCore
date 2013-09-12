/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _SLG_BSDF_H
#define	_SLG_BSDF_H

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/frame.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/spectrum.h"
#include "slg/sdl/bsdfevents.h"
#include "slg/sdl/material.h"
#include "slg/sdl/light.h"
#include "slg/sdl/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/sdl/bsdf_types.cl"
}

class Scene;

class BSDF {
public:
	// An empty BSDF
	BSDF() : material(NULL) { };

	// A BSDF initialized from a ray hit
	BSDF(const bool fixedFromLight, const Scene &scene, const luxrays::Ray &ray,
		const luxrays::RayHit &rayHit, const float passThroughEvent) {
		assert (!rayHit.Miss());
		Init(fixedFromLight, scene, ray, rayHit, passThroughEvent);
	}
	void Init(const bool fixedFromLight, const Scene &scene, const luxrays::Ray &ray,
		const luxrays::RayHit &rayHit, const float passThroughEvent);

	bool IsEmpty() const { return (material == NULL); }
	bool IsLightSource() const { return material->IsLightSource(); }
	bool IsDelta() const { return material->IsDelta(); }
	bool IsVisibleIndirectDiffuse() const { return material->IsVisibleIndirectDiffuse(); }
	bool IsVisibleIndirectGlossy() const { return material->IsVisibleIndirectGlossy(); }

	BSDFEvent GetEventTypes() const { return material->GetEventTypes(); }

	luxrays::Spectrum GetPassThroughTransparency() const;

	luxrays::Spectrum Evaluate(const luxrays::Vector &generatedDir,
		BSDFEvent *event, float *directPdfW = NULL, float *reversePdfW = NULL) const;
	luxrays::Spectrum Sample(luxrays::Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent = ALL) const;
	void Pdf(const luxrays::Vector &sampledDir, float *directPdfW, float *reversePdfW) const;

	luxrays::Spectrum GetEmittedRadiance(float *directPdfA = NULL, float *emissionPdfW = NULL) const ;

	const LightSource *GetLightSource() const { return triangleLightSource; }

	HitPoint hitPoint;

private:
	const luxrays::ExtMesh *mesh;
	const Material *material;
	const TriangleLight *triangleLightSource; // != NULL only if it is an area light
	luxrays::Frame frame;
};
	
}

#endif	/* _SLG_BSDF_H */
