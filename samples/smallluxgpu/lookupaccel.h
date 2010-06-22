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

enum LookUpAccelType {
	HASH_GRID, KD_TREE, HYBRID_HASH_GRID
};

class HitPointsLookUpAccel {
public:
	HitPointsLookUpAccel() { }
	virtual ~HitPointsLookUpAccel() { }

	virtual void RefreshMutex() = 0;
	virtual void RefreshParallel(const unsigned int index, const unsigned int count) { }

	virtual void AddFlux(const luxrays::Point &hitPoint, const luxrays::Vector &wi,
		const luxrays::Spectrum &photonFlux) = 0;
};

//------------------------------------------------------------------------------
// HashGrid accelerator
//------------------------------------------------------------------------------

class HashGrid : public HitPointsLookUpAccel {
public:
	HashGrid(HitPoints *hps);

	~HashGrid();

	void RefreshMutex();

	void AddFlux(const luxrays::Point &hitPoint, const luxrays::Vector &wi,
		const luxrays::Spectrum &photonFlux);

private:
	unsigned int Hash(const int ix, const int iy, const int iz) {
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % gridSize;
	}

	HitPoints *hitPoints;
	unsigned int gridSize;
	float invCellSize;
	std::list<HitPoint *> **grid;
};

//------------------------------------------------------------------------------
// KdTree accelerator
//------------------------------------------------------------------------------

class KdTree : public HitPointsLookUpAccel {
public:
	KdTree(HitPoints *hps);

	~KdTree();

	void RefreshMutex();

	void AddFlux(const luxrays::Point &hitPoint,  const luxrays::Vector &wi,
		const luxrays::Spectrum &photonFlux);

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

	HitPoints *hitPoints;

	KdNode *nodes;
	HitPoint **nodeData;
	unsigned int nNodes, nextFreeNode;
	float maxDistSquared;
};

//------------------------------------------------------------------------------
// HybridHashGrid accelerator
//------------------------------------------------------------------------------

class HybridHashGrid : public HitPointsLookUpAccel {
public:
	HybridHashGrid(HitPoints *hps);

	~HybridHashGrid();

	void RefreshMutex();
	void RefreshParallel(const unsigned int index, const unsigned int count);

	void AddFlux(const luxrays::Point &hitPoint, const luxrays::Vector &wi,
		const luxrays::Spectrum &photonFlux);

private:
	unsigned int Hash(const int ix, const int iy, const int iz) {
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % gridSize;
	}

	class HHGKdTree {
	public:
		HHGKdTree(std::list<HitPoint *> *hps, const unsigned int count);
		~HHGKdTree();

		void AddFlux(const luxrays::Point &hitPoint, const luxrays::Vector &wi,
			const luxrays::Spectrum &photonFlux);

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
			CompareNode(int a) {
				axis = a;
			}

			int axis;

			bool operator()(const HitPoint *d1, const HitPoint * d2) const;
		};

		void RecursiveBuild(const unsigned int nodeNum, const unsigned int start,
				const unsigned int end, std::vector<HitPoint *> &buildNodes);

		KdNode *nodes;
		HitPoint **nodeData;
		unsigned int nNodes, nextFreeNode;
		float maxDistSquared;
	};

	enum HashCellType {
		LIST, KD_TREE
	};

	class HashCell {
	public:
		HashCell(const HashCellType t) {
			type = LIST;
			size = 0;
			list = new std::list<HitPoint *>();
		}
		~HashCell() {
			switch (type) {
				case LIST:
					delete list;
					break;
				case KD_TREE:
					delete kdtree;
					break;
				default:
					assert (false);
			}
		}

		void AddList(HitPoint *hp) {
			assert (type == LIST);

			list->push_front(hp);
			++size;
		}

		void TransformToKdTree() {
			assert (type == LIST);

			std::list<HitPoint *> *hplist = list;
			kdtree = new HHGKdTree(hplist, size);
			delete hplist;
			type = KD_TREE;
		}

		void AddFlux(const luxrays::Point &hitPoint, const luxrays::Vector &wi,
			const luxrays::Spectrum &photonFlux);

		unsigned int GetSize() const { return size; }

	private:
		HashCellType type;
		unsigned int size;
		union {
			std::list<HitPoint *> *list;
			HHGKdTree *kdtree;
		};
	};

	unsigned int kdtreeThreshold;
	HitPoints *hitPoints;
	unsigned int gridSize;
	float invCellSize;
	HashCell **grid;
};

#endif	/* _LOOKUPACCEL_H */
