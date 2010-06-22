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

#ifndef _HITPOINTS_H
#define	_HITPOINTS_H

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/sdl/scene.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/core/intersectiondevice.h"

#include "lookupaccel.h"

using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

class EyePath {
public:
	// Screen information
	unsigned int pixelIndex;

	// Eye path information
	Ray ray;
	unsigned int depth;
	Spectrum throughput;
};

class PhotonPath {
public:
	// The ray is stored in the RayBuffer and the index is implicitly stored
	// in the array of PhotonPath
	Spectrum flux;
	unsigned int depth;
};

//------------------------------------------------------------------------------
// Eye path hit points
//------------------------------------------------------------------------------

enum HitPointType {
	SURFACE, CONSTANT_COLOR
};

class HitPoint {
public:
	HitPointType type;

	// Used for CONSTANT_COLOR and SURFACE type
	Spectrum throughput;

	// Used for SURFACE type
	Point position;
	Vector wo;
	Normal normal;
	const SurfaceMaterial *material;

	unsigned long long photonCount;
	Spectrum reflectedFlux;

	float accumPhotonRadius2;
	unsigned int accumPhotonCount;
	Spectrum accumReflectedFlux;

	Spectrum accumRadiance;

	unsigned int constantHitsCount;
	unsigned int surfaceHitsCount;
	Spectrum radiance;
};

extern bool GetHitPointInformation(const Scene *scene, RandomGenerator *rndGen,
		Ray *ray, const RayHit *rayHit, Point &hitPoint,
		Spectrum &surfaceColor, Normal &N, Normal &shadeN);

class SPPMRenderEngine;

class HitPoints {
public:
	HitPoints(SPPMRenderEngine *engine, RandomGenerator *rndGen,
			IntersectionDevice *dev, RayBuffer *rayBuffer);
	~HitPoints();

	HitPoint *GetHitPoint(const unsigned int index) {
		return &(*hitPoints)[index];
	}

	const unsigned int GetSize() const {
		return hitPoints->size();
	}

	const BBox GetBBox() const {
		return bbox;
	}

	void UpdateBBox();
	unsigned int GetPassCount() const { return pass; }
	void IncPass() { ++pass; }

	void AddFlux(const Point &hitPoint, const Vector &wi, const Spectrum &photonFlux) {
		lookUpAccel->AddFlux(hitPoint, wi, photonFlux);
	}

	void AccumulateFlux(const unsigned long long photonTraced,
			const unsigned int index, const unsigned int count);
	void SetHitPoints(RandomGenerator *rndGen,
			IntersectionDevice *device, RayBuffer *rayBuffer,
			const unsigned int index, const unsigned int count);

	void RefreshAccelMutex() {
		lookUpAccel->RefreshMutex();
	}

	void RefreshAccelParallel(const unsigned int index, const unsigned int count) {
		lookUpAccel->RefreshParallel(index, count);
	}

private:
	SPPMRenderEngine *renderEngine;

	BBox bbox;
	std::vector<HitPoint> *hitPoints;
	HitPointsLookUpAccel *lookUpAccel;
	unsigned int pass;
};

#endif	/* _HITPOINTS_H */
