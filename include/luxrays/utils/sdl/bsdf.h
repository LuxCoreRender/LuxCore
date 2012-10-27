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

#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/sdl/scene.h"
#include "luxrays/utils/sdl/bsdfevents.h"

namespace luxrays { namespace sdl {

class BSDF {
public:
	// An empty BSDF
	BSDF() : material(NULL) { };

	// A BSDF initialized from a ray hit
	BSDF(const bool l2e, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0) {
		Init(l2e, scene, ray, rayHit, u0);
	}

	void Init(const bool fixedFromLight, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0);

	bool IsEmpty() const { return (material == NULL); }
	bool IsPassThrough() const { return isPassThrough; }
	bool IsLightSource() const { return isLightSource; }
	bool IsDelta() const { return surfMat->IsSpecular(); }

	Spectrum Evaluate(const Vector &lightDir, const Vector &eyeDir,
		BSDFEvent *event, float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, BSDFEvent *event) const;
	Spectrum GetEmittedRadiance(const Scene *scene,
			const Vector &dir,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const ;

	Point hitPoint;
	Normal geometryN;
	Normal shadeN;
	Spectrum surfaceColor;

private:
	const ExtMesh *mesh;
	unsigned int triIndex;

	const Material *material;
	const SurfaceMaterial *surfMat; // != NULL only if it isn't an area light
	const LightSource *lightSource; // != NULL only if it is an area light
	Frame frame;

	bool fromLight, isPassThrough, isLightSource;
};
	
} }

#endif	/* _LUXRAYS_SDL_BSDF_H */
