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

#include "smallsppmgpu.h"

KdTree::KdTree(HitPoints *hps) {
	hitPoints = hps;
	nNodes = hitPoints->GetSize();
	nextFreeNode = 1;
	nodes = NULL;
	nodeData = NULL;

	Refresh();
}

KdTree::~KdTree() {
	delete[] nodes;
	delete[] nodeData;
}

bool KdTree::CompareNode::operator ()(const HitPoint *d1, const HitPoint *d2) const {
	return (d1->position[axis] == d2->position[axis]) ? (d1 < d2) :
			(d1->position[axis] < d2->position[axis]);
}

void KdTree::RecursiveBuild(const unsigned int nodeNum, const unsigned int start,
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

void KdTree::Refresh() {
	delete[] nodes;
	delete[] nodeData;

	std::cerr << "Building kD-Tree with " << nNodes << " nodes" << std::endl;

	nodes = new KdNode[nNodes];
	nodeData = new HitPoint*[nNodes];
	nextFreeNode = 1;

	// Begin the KdTree building process
	std::vector<HitPoint *> buildNodes;
	buildNodes.reserve(nNodes);
	maxDistSquared = 0.f;
	for (unsigned int i = 0; i < nNodes; ++i)  {
		buildNodes.push_back(hitPoints->GetHitPoint(i));
		maxDistSquared = luxrays::Max(maxDistSquared, buildNodes[i]->accumPhotonRadius2);
	}
	std::cerr << "kD-Tree search radius: " << sqrtf(maxDistSquared) << std::endl;

	RecursiveBuild(0, 0, nNodes, buildNodes);
}

void KdTree::AddFluxImpl(const unsigned int nodeNum,
		const float alpha, const luxrays::Point &p, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
	KdNode *node = &nodes[nodeNum];

	// Process kd-tree node's children
	int axis = node->splitAxis;
	if (axis != 3) {
		float dist = p[axis] - node->splitPos;
		float dist2 = dist * dist;
		if (p[axis] <= node->splitPos) {
			if (node->hasLeftChild)
				AddFluxImpl(nodeNum + 1, alpha, p, shadeN, wi, photonFlux);
			if (dist2 < maxDistSquared && node->rightChild < nNodes)
				AddFluxImpl(node->rightChild, alpha, p, shadeN, wi, photonFlux);
		} else {
			if (node->rightChild < nNodes)
				AddFluxImpl(node->rightChild, alpha, p, shadeN, wi, photonFlux);
			if (dist2 < maxDistSquared && node->hasLeftChild)
				AddFluxImpl(nodeNum + 1, alpha, p, shadeN, wi, photonFlux);
		}
	}

	// Hand kd-tree node to processing function
	HitPoint *hp = nodeData[nodeNum];
	const float dist2 = luxrays::DistanceSquared(hp->position, p);
	// TODO: use configurable parameter for normal treshold
	if ((luxrays::Dot(hp->normal, shadeN) > luxrays::RAY_EPSILON) &&
			(dist2 <=  hp->accumPhotonRadius2)) {
		hp->accumPhotonCount++;

		hp->accumReflectedFlux += photonFlux * hp->material->f(hp->wo, wi, hp->normal) *
				luxrays::AbsDot(hp->normal, wi) * hp->throughput;
	}
}
