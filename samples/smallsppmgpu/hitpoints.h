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

#define MAX_EYE_PATH_DEPTH 16
#define MAX_PHOTON_PATH_DEPTH 8

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
// Hit points look up accelerators
//------------------------------------------------------------------------------

class HitPoint;
class HitPoints;

class HitPointsLookUpAccel {
public:
	HitPointsLookUpAccel();
	virtual ~HitPointsLookUpAccel();

	virtual void Refresh() = 0;

	virtual void AddFlux(const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) = 0;
};

class HashGrid : public HitPointsLookUpAccel {
public:
	HashGrid(HitPoints *hps);

	~HashGrid();

	void Refresh();

	void AddFlux(const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux);

private:
	unsigned int Hash(const int ix, const int iy, const int iz) {
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % hashGridSize;
	}

	HitPoints *hitPoints;
	unsigned int hashGridSize;
	float invCellSize;
	std::list<HitPoint *> **hashGrid;
};

class KdTree : public HitPointsLookUpAccel {
public:
	KdTree(HitPoints *hps);

	~KdTree();

	void Refresh();

	void AddFlux(const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
		AddFluxImpl(0, alpha, hitPoint, shadeN, wi, photonFlux);
	}

private:
	struct KdNode {
		void init(const float p, const unsigned int a) {
			splitPos = p;
			splitAxis = a;
			// Dade - in order to avoid a gcc warning
			rightChild = 0;
			rightChild = ~rightChild;
			hasLeftChild = 0;
		}

		void initLeaf() {
			splitAxis = 3;
			// Dade - in order to avoid a gcc warning
			rightChild = 0;
			rightChild = ~rightChild;
			hasLeftChild = 0;
		}

		// KdNode Data
		float splitPos;
		unsigned int splitAxis : 2;
		unsigned int hasLeftChild : 1;
		unsigned int rightChild : 29;
	};

	struct CompareNode {
		CompareNode(int a) { axis = a; }

		int axis;

		bool operator()(const HitPoint *d1, const HitPoint *d2) const;
	};

	void RecursiveBuild(const unsigned int nodeNum, const unsigned int start,
		const unsigned int end, std::vector<HitPoint *> &buildNodes);

	void AddFluxImpl(const unsigned int nodeNum,
		const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux);

	HitPoints *hitPoints;

	KdNode *nodes;
	HitPoint **nodeData;
	unsigned int nNodes, nextFreeNode;
	float maxDistSquared;
};

//------------------------------------------------------------------------------
// Eye path hit points
//------------------------------------------------------------------------------

enum HitPointType {
	SURFACE, CONSTANT_COLOR
};

enum LookUpAccelType {
	HASH_GRID, KD_TREE
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
	unsigned long long accumPhotonCount;
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
	HitPoints(luxrays::sdl::Scene *scn, luxrays::RandomGenerator *rndg,
			luxrays::IntersectionDevice *dev, const float a,
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

	void AddFlux(const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
		lookUpAccel->AddFlux(alpha, hitPoint, shadeN, wi, photonFlux);
	}

	void AccumulateFlux(const unsigned int photonTraced);

	void Recast(const unsigned int photonTraced) {
		AccumulateFlux(photonTraced);
		SetHitPoints();
		lookUpAccel->Refresh();

		++pass;
	}

	unsigned int GetPassCount() const { return pass; }

private:
	void SetHitPoints();

	luxrays::sdl::Scene *scene;
	luxrays::RandomGenerator *rndGen;
	luxrays::IntersectionDevice *device;
	luxrays::RayBuffer *rayBuffer;
	// double instead of float because photon counters declared as int 64bit
	double alpha;
	unsigned int width;
	unsigned int height;

	luxrays::BBox bbox;
	std::vector<HitPoint> *hitPoints;
	LookUpAccelType lookUpAccelType;
	HitPointsLookUpAccel *lookUpAccel;
	unsigned int pass;
};

#endif	/* _HITPOINTS_H */
