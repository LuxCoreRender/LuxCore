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

#ifndef _LOOKUPACCEL_H
#define	_LOOKUPACCEL_H

#include "luxrays/luxrays.h"

//------------------------------------------------------------------------------
// Hit points look up accelerators
//------------------------------------------------------------------------------

class HitPoint;
class HitPoints;

class HitPointsLookUpAccel {
public:
	HitPointsLookUpAccel() { }
	virtual ~HitPointsLookUpAccel() { }

	virtual void Refresh() = 0;

	virtual void AddFlux(const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) = 0;
};

//------------------------------------------------------------------------------
// HashGrid accelerator
//------------------------------------------------------------------------------

class HashGrid : public HitPointsLookUpAccel {
public:
	HashGrid(HitPoints *hps);

	~HashGrid();

	void Refresh();

	void AddFlux(const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
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

//------------------------------------------------------------------------------
// KdTree accelerator
//------------------------------------------------------------------------------

class KdTree : public HitPointsLookUpAccel {
public:
	KdTree(HitPoints *hps);

	~KdTree();

	void Refresh();

	void AddFlux(const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
		AddFluxImpl(0, hitPoint, shadeN, wi, photonFlux);
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
		const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux);

	HitPoints *hitPoints;

	KdNode *nodes;
	HitPoint **nodeData;
	unsigned int nNodes, nextFreeNode;
	float maxDistSquared;
};

#endif	/* _LOOKUPACCEL_H */
