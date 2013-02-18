/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#ifndef _LUXRAYS_SDL_BSDF_H
#define	_LUXRAYS_SDL_BSDF_H

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/core/spectrum.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/sdl/bsdfevents.h"
#include "luxrays/utils/sdl/material.h"
#include "luxrays/utils/sdl/light.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/utils/sdl/bsdf_types.cl"
}

namespace sdl {

class Scene;

struct HitPointStruct {
	// The incoming direction. It is the eyeDir when fromLight = false and
	// lightDir when fromLight = true
	Vector fixedDir;
	Point p;
	UV uv;
	Normal geometryN;
	Normal shadeN;
	float passThroughEvent;
	bool fromLight;
};
typedef HitPointStruct HitPoint;

class BSDF {
public:
	// An empty BSDF
	BSDF() : material(NULL) { };

	// A BSDF initialized from a ray hit
	BSDF(const bool fixedFromLight, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float passThroughEvent) {
		assert (!rayHit.Miss());
		Init(fixedFromLight, scene, ray, rayHit, passThroughEvent);
	}
	void Init(const bool fixedFromLight, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float passThroughEvent);

	bool IsEmpty() const { return (material == NULL); }
	bool IsLightSource() const { return material->IsLightSource(); }
	bool IsDelta() const { return material->IsDelta(); }

	Spectrum GetPassThroughTransparency() const;

	Spectrum Evaluate(const Vector &generatedDir,
		BSDFEvent *event, float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event) const;
	void Pdf(const Vector &sampledDir, float *directPdfW, float *reversePdfW) const;
	Spectrum GetEmittedRadiance(float *directPdfA = NULL, float *emissionPdfW = NULL) const ;

	HitPoint hitPoint;

private:
	const ExtMesh *mesh;
	u_int triIndex;

	const Material *material;
	const TriangleLight *triangleLightSource; // != NULL only if it is an area light
	Frame frame;

};
	
} }

#endif	/* _LUXRAYS_SDL_BSDF_H */
