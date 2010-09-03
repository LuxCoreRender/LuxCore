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

#include <cstdlib>
#include <cassert>
#include <deque>
#include <sstream>

#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/accelerators/qbvhaccel.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/core/geometry/bsphere.h"

using namespace luxrays;

DataSet::DataSet(const Context *luxRaysContext) {
	context = luxRaysContext;

	totalVertexCount = 0;
	totalTriangleCount = 0;
	preprocessed = false;

	accelType = ACCEL_QBVH;
	accel = NULL;
}

DataSet::~DataSet() {
	delete accel;
}

TriangleMeshID DataSet::Add(Mesh *mesh) {
	assert (!preprocessed);

	const TriangleMeshID id = meshes.size();
	meshes.push_back(mesh);

	totalVertexCount += mesh->GetTotalVertexCount();
	totalTriangleCount += mesh->GetTotalTriangleCount();

	bbox = Union(bbox, mesh->GetBBox());
	bsphere = bbox.BoundingSphere();

	return id;
}

void DataSet::Preprocess() {
	assert (!preprocessed);

	LR_LOG(context, "Preprocessing DataSet");
	LR_LOG(context, "Total vertex count: " << totalVertexCount);
	LR_LOG(context, "Total triangle count: " << totalTriangleCount);

	// Build the Acceleretor
	switch (accelType) {
		case ACCEL_BVH: {
			const int treeType = 4; // Tree type to generate (2 = binary, 4 = quad, 8 = octree)
			const int costSamples = 0; // Samples to get for cost minimization
			const int isectCost = 80;
			const int travCost = 10;
			const float emptyBonus = 0.5f;

			accel = new BVHAccel(context, treeType, costSamples, isectCost, travCost, emptyBonus);
			break;
		}
		case ACCEL_QBVH: {
			const int maxPrimsPerLeaf = 4;
			const int fullSweepThreshold = 4 * maxPrimsPerLeaf;
			const int skipFactor = 1;

			accel = new QBVHAccel(context,
					maxPrimsPerLeaf, fullSweepThreshold, skipFactor);
			break;
		}
		case ACCEL_MQBVH: {
			const int fullSweepThreshold = 4;
			const int skipFactor = 1;

			accel = new MQBVHAccel(context, fullSweepThreshold, skipFactor);
			break;
		}
		default:
			assert (false);
	}

	accel->Init(meshes, totalVertexCount, totalTriangleCount);

	// Free the list of mesh
	meshes.clear();

	preprocessed = true;
}

void DataSet::UpdateMeshes() {
	assert (preprocessed);
	assert (accelType == ACCEL_MQBVH);

	MQBVHAccel *mqbvh = (MQBVHAccel *)accel;
	mqbvh->Update();
}

bool DataSet::Intersect(const Ray *ray, RayHit *hit) const {
	return accel->Intersect(ray, hit);
}
