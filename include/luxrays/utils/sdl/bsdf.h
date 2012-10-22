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

#include "luxrays/utils/sdl/scene.h"

namespace luxrays { namespace sdl {

class BSDF {
public:
	enum Events {
		NONE     = 0,
		DIFFUSE  = 1,
        GLOSSY   = 2,
        REFLECT  = 4,
        REFRACT  = 8,
        SPECULAR = (REFLECT | REFRACT),
        NON_SPECULAR = (DIFFUSE  | GLOSSY),
        ALL          = (SPECULAR | NON_SPECULAR)
    };

	// An empty BSDF
	BSDF() : material(NULL) { };

	// A BSDF initialized from a ray hit
	BSDF(const bool l2e, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0) {
		Init(l2e, scene, ray, rayHit, u0);
	}

	void Init(const bool fromLightToEye, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0);

	bool IsEmpty() const { return (material == NULL); }
	bool IsPassThrough() const { return isPassThrough; }
	bool IsLightSource() const { return isLightSource; }
	
	Spectrum Evaluate(const Vector &wi, const Vector &wo) {
		return surfMat->Evaluate(wi, wo, shadeN);
	}

	Spectrum Sample(const Vector &wi, Vector *wo,
		const float u0, const float u1,  const float u2,
		float *pdf) const {
		return surfMat->Sample(wi, wo, geometryN, shadeN, u0, u1, u2, pdf);
	}

	Point hitPoint;
	Normal geometryN;
	Normal shadeN;
	Spectrum surfaceColor;

public:
	const Material *material;
	const SurfaceMaterial *surfMat; // != NULL only if it isn't an area light

	bool fromLightToEye, isPassThrough, isLightSource;
};
	
} }

#endif	/* _LUXRAYS_SDL_BSDF_H */
