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

#include "smalllux.h"
#include "hitpoints.h"

HybridHashGrid::HybridHashGrid(HitPoints *hps) {
	hitPoints = hps;
	grid = NULL;
	kdtreeTreshold = 10;

	Refresh();
}

HybridHashGrid::~HybridHashGrid() {
	for (unsigned int i = 0; i < gridSize; ++i)
		delete grid[i];
	delete[] grid;
}

void HybridHashGrid::Refresh() {
	const unsigned int hitPointsCount = hitPoints->GetSize();
	const luxrays::BBox &hpBBox = hitPoints->GetBBox();

	// Calculate the size of the grid cell
	float maxPhotonRadius2 = 0.f;
	for (unsigned int i = 0; i < hitPointsCount; ++i) {
		HitPoint *hp = hitPoints->GetHitPoint(i);

		if (hp->type == SURFACE)
			maxPhotonRadius2 = luxrays::Max(maxPhotonRadius2, hp->accumPhotonRadius2);
	}

	const float cellSize = sqrtf(maxPhotonRadius2) * 2.f;
	std::cerr << "Hybrid hash grid cell size: " << cellSize << std::endl;
	invCellSize = 1.f / cellSize;

	// TODO: add a tunable parameter for HybridHashGrid size
	gridSize = hitPointsCount;
	if (!grid) {
		grid = new HashCell*[gridSize];

		for (unsigned int i = 0; i < gridSize; ++i)
			grid[i] = NULL;
	} else {
		for (unsigned int i = 0; i < gridSize; ++i) {
			delete grid[i];
			grid[i] = NULL;
		}
	}

	std::cerr << "Building hit points hybrid hash grid:" << std::endl;
	std::cerr << "  0k/" << hitPointsCount / 1000 << "k" <<std::endl;
	unsigned int maxPathCount = 0;
	double lastPrintTime = luxrays::WallClockTime();
	unsigned long long entryCount = 0;
	for (unsigned int i = 0; i < hitPointsCount; ++i) {
		if (luxrays::WallClockTime() - lastPrintTime > 2.0) {
			std::cerr << "  " << i / 1000 << "k/" << hitPointsCount / 1000 << "k" <<std::endl;
			lastPrintTime = luxrays::WallClockTime();
		}

		HitPoint *hp = hitPoints->GetHitPoint(i);

		if (hp->type == SURFACE) {
			const float photonRadius = sqrtf(hp->accumPhotonRadius2);
			const luxrays::Vector rad(photonRadius, photonRadius, photonRadius);
			const luxrays::Vector bMin = ((hp->position - rad) - hpBBox.pMin) * invCellSize;
			const luxrays::Vector bMax = ((hp->position + rad) - hpBBox.pMin) * invCellSize;

			for (int iz = abs(int(bMin.z)); iz <= abs(int(bMax.z)); iz++) {
				for (int iy = abs(int(bMin.y)); iy <= abs(int(bMax.y)); iy++) {
					for (int ix = abs(int(bMin.x)); ix <= abs(int(bMax.x)); ix++) {
						int hv = Hash(ix, iy, iz);

						if (grid[hv] == NULL) {
							grid[hv] = new HashCell(LIST);
						}

						grid[hv]->AddList(hp);
						++entryCount;

						if (grid[hv]->GetSize() > maxPathCount)
							maxPathCount = grid[hv]->GetSize();
					}
				}
			}
		}
	}
	std::cerr << "Max. hit points in a single hybrid hash grid entry: " << maxPathCount << std::endl;
	std::cerr << "Total hash grid entry: " << entryCount << std::endl;
	std::cerr << "Avg. hit points in a single hybrid hash grid entry: " << entryCount / gridSize << std::endl;

	unsigned int HHGKdTreeEntries = 0;
	unsigned int HHGlistEntries = 0;
	for (unsigned int i = 0; i < gridSize; ++i) {
		HashCell *hc = grid[i];

		if (hc && hc->GetSize() > kdtreeTreshold) {
			hc->TransformToKdTree();
			++HHGKdTreeEntries;
		} else
			++HHGlistEntries;
	}
	std::cerr << "Hybrid hash cells storing a HHGKdTree: " << HHGKdTreeEntries << "/" << HHGlistEntries << std::endl;

	// HybridHashGrid debug code
	/*for (unsigned int i = 0; i < HybridHashGridSize; ++i) {
		if (HybridHashGrid[i]) {
			if (HybridHashGrid[i]->size() > 10) {
				std::cerr << "HybridHashGrid[" << i << "].size() = " <<HybridHashGrid[i]->size() << std::endl;
			}
		}
	}*/
}

void HybridHashGrid::AddFlux(const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
	const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
	// Look for eye path hit points near the current hit point
	luxrays::Vector hh = (hitPoint - hitPoints->GetBBox().pMin) * invCellSize;
	const int ix = abs(int(hh.x));
	const int iy = abs(int(hh.y));
	const int iz = abs(int(hh.z));

	HashCell *hc = grid[Hash(ix, iy, iz)];
	if (hc)
		hc->AddFlux(hitPoint, shadeN, wi, photonFlux);
}

void HybridHashGrid::HashCell::AddFlux(const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
	switch (type) {
		case LIST: {
			std::list<HitPoint *>::iterator iter = list->begin();
			while (iter != list->end()) {
				HitPoint *hp = *iter++;

				const float dist2 = luxrays::DistanceSquared(hp->position, hitPoint);
				if ((dist2 >  hp->accumPhotonRadius2))
					continue;

				const float dot = luxrays::Dot(hp->normal, wi);
				if (dot <= luxrays::RAY_EPSILON)
					continue;

				AtomicInc(&hp->accumPhotonCount);
				luxrays::Spectrum flux = photonFlux * hp->material->f(hp->wo, wi, hp->normal) *
						dot * hp->throughput;
				AtomicAdd(&hp->accumReflectedFlux.r, flux.r);
				AtomicAdd(&hp->accumReflectedFlux.g, flux.g);
				AtomicAdd(&hp->accumReflectedFlux.b, flux.b);
			}
			break;
		}
		case KD_TREE: {
			kdtree->AddFlux(hitPoint, shadeN, wi, photonFlux);
			break;
		}
		default:
			assert (false);
	}
}

HybridHashGrid::HHGKdTree::HHGKdTree(std::list<HitPoint *> *hps, const unsigned int count) {
	nNodes = count;
	nextFreeNode = 1;

	//std::cerr << "Building kD-Tree with " << nNodes << " nodes" << std::endl;

	nodes = new KdNode[nNodes];
	nodeData = new HitPoint*[nNodes];
	nextFreeNode = 1;

	// Begin the HHGKdTree building process
	std::vector<HitPoint *> buildNodes;
	buildNodes.reserve(nNodes);
	maxDistSquared = 0.f;
	std::list<HitPoint *>::iterator iter = hps->begin();
	for (unsigned int i = 0; i < nNodes; ++i)  {
		buildNodes.push_back(*iter++);
		maxDistSquared = luxrays::Max(maxDistSquared, buildNodes[i]->accumPhotonRadius2);
	}
	//std::cerr << "kD-Tree search radius: " << sqrtf(maxDistSquared) << std::endl;

	RecursiveBuild(0, 0, nNodes, buildNodes);
}

HybridHashGrid::HHGKdTree::~HHGKdTree() {
	delete[] nodes;
	delete[] nodeData;
}

bool HybridHashGrid::HHGKdTree::CompareNode::operator ()(const HitPoint *d1, const HitPoint *d2) const {
	return (d1->position[axis] == d2->position[axis]) ? (d1 < d2) :
			(d1->position[axis] < d2->position[axis]);
}

void HybridHashGrid::HHGKdTree::RecursiveBuild(const unsigned int nodeNum, const unsigned int start,
		const unsigned int end, std::vector<HitPoint *> &buildNodes) {
	assert (nodeNum >= 0);
	assert (start >= 0);
	assert (end >= 0);
	assert (nodeNum < nNodes);
	assert (start < nNodes);
	assert (end <= nNodes);

	// Create leaf node of kd-tree if we've reached the bottom
	if (start + 1 == end) {
		nodes[nodeNum].initLeaf();
		nodeData[nodeNum] = buildNodes[start];
		return;
	}

	// Choose split direction and partition data
	// Compute bounds of data from start to end
	luxrays::BBox bound;
	for (unsigned int i = start; i < end; ++i)
		bound = luxrays::Union(bound, buildNodes[i]->position);
	unsigned int splitAxis = bound.MaximumExtent();
	unsigned int splitPos = (start + end) / 2;

	std::nth_element(&buildNodes[start], &buildNodes[splitPos],
		&buildNodes[end - 1], CompareNode(splitAxis));

	// Allocate kd-tree node and continue recursively
	nodes[nodeNum].init(buildNodes[splitPos]->position[splitAxis], splitAxis);
	nodeData[nodeNum] = buildNodes[splitPos];

	if (start < splitPos) {
		nodes[nodeNum].hasLeftChild = 1;
		unsigned int childNum = nextFreeNode++;
		RecursiveBuild(childNum, start, splitPos, buildNodes);
	}

	if (splitPos + 1 < end) {
		nodes[nodeNum].rightChild = nextFreeNode++;
		RecursiveBuild(nodes[nodeNum].rightChild, splitPos+1, end, buildNodes);
	}
}

void HybridHashGrid::HHGKdTree::AddFluxImpl(const unsigned int nodeNum,
		const luxrays::Point &p, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
	KdNode *node = &nodes[nodeNum];

	// Process kd-tree node's children
	int axis = node->splitAxis;
	if (axis != 3) {
		float dist = p[axis] - node->splitPos;
		float dist2 = dist * dist;
		if (p[axis] <= node->splitPos) {
			if (node->hasLeftChild)
				AddFluxImpl(nodeNum + 1, p, shadeN, wi, photonFlux);
			if (dist2 < maxDistSquared && node->rightChild < nNodes)
				AddFluxImpl(node->rightChild, p, shadeN, wi, photonFlux);
		} else {
			if (node->rightChild < nNodes)
				AddFluxImpl(node->rightChild, p, shadeN, wi, photonFlux);
			if (dist2 < maxDistSquared && node->hasLeftChild)
				AddFluxImpl(nodeNum + 1, p, shadeN, wi, photonFlux);
		}
	}

	// Hand kd-tree node to processing function
	HitPoint *hp = nodeData[nodeNum];
	const float dist2 = luxrays::DistanceSquared(hp->position, p);
	if (dist2 > hp->accumPhotonRadius2)
		return;

	const float dot = luxrays::Dot(hp->normal, wi);
	if (dot <= luxrays::RAY_EPSILON)
		return;

	AtomicInc(&hp->accumPhotonCount);
	luxrays::Spectrum flux = photonFlux * hp->material->f(hp->wo, wi, hp->normal) *
			dot * hp->throughput;
	AtomicAdd(&hp->accumReflectedFlux.r, flux.r);
	AtomicAdd(&hp->accumReflectedFlux.g, flux.g);
	AtomicAdd(&hp->accumReflectedFlux.b, flux.b);
}
