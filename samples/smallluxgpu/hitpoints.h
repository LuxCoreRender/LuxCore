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

class EyePath {
public:
	// Screen information
	unsigned int pixelIndex;

	// Eye path information
	luxrays::Ray ray;
	unsigned int depth;
	luxrays::Spectrum throughput;
};

class PhotonPath {
public:
	// The ray is stored in the RayBuffer and the index is implicitly stored
	// in the array of PhotonPath
	luxrays::Spectrum flux;
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
	luxrays::Spectrum throughput;

	// Used for SURFACE type
	luxrays::Point position;
	luxrays::Vector wo;
	luxrays::Normal normal;
	const luxrays::sdl::SurfaceMaterial *material;

	unsigned long long photonCount;
	luxrays::Spectrum reflectedFlux;

	float accumPhotonRadius2;
	unsigned int accumPhotonCount;
	luxrays::Spectrum accumReflectedFlux;

	luxrays::Spectrum accumRadiance;

	unsigned int constantHitsCount;
	unsigned int surfaceHitsCount;
	luxrays::Spectrum radiance;
};

extern bool GetHitPointInformation(const luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
		luxrays::Ray *ray, const luxrays::RayHit *rayHit, luxrays::Point &hitPoint,
		luxrays::Spectrum &surfaceColor, luxrays::Normal &N, luxrays::Normal &shadeN);

class HitPoints {
public:
	HitPoints(luxrays::sdl::Scene *scn, luxrays::RandomGenerator *rndGen,
			luxrays::IntersectionDevice *dev, luxrays::RayBuffer *rayBuffer,
			const float a, const unsigned int maxEyeDepth,
			const unsigned int w, const unsigned int h,
			const LookUpAccelType accelType);
	~HitPoints();

	HitPoint *GetHitPoint(const unsigned int index) {
		return &(*hitPoints)[index];
	}

	const unsigned int GetSize() const {
		return hitPoints->size();
	}

	const luxrays::BBox GetBBox() const {
		return bbox;
	}

	void AddFlux(const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
		lookUpAccel->AddFlux(hitPoint, shadeN, wi, photonFlux);
	}

	void AccumulateFlux(const unsigned long long photonTraced);

	void Recast(luxrays::RandomGenerator *rndGen, luxrays::RayBuffer *rayBuffer, const unsigned long long photonTraced) {
		AccumulateFlux(photonTraced);
		SetHitPoints(rndGen, rayBuffer);
		lookUpAccel->Refresh();

		++pass;
	}

	unsigned int GetPassCount() const { return pass; }

private:
	void SetHitPoints(luxrays::RandomGenerator *rndGen, luxrays::RayBuffer *rayBuffer);

	luxrays::sdl::Scene *scene;
	luxrays::IntersectionDevice *device;
	// double instead of float because photon counters declared as int 64bit
	double alpha;
	unsigned int maxEyePathDepth;
	unsigned int width;
	unsigned int height;

	luxrays::BBox bbox;
	std::vector<HitPoint> *hitPoints;
	LookUpAccelType lookUpAccelType;
	HitPointsLookUpAccel *lookUpAccel;
	unsigned int pass;
};

#endif	/* _HITPOINTS_H */
