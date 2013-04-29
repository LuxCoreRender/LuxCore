/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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
#include <boost/lexical_cast.hpp>

#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/accelerators/qbvhaccel.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/mbvhaccel.h"
#include "luxrays/core/geometry/bsphere.h"

using namespace luxrays;

static unsigned int DataSetID = 0;
static boost::mutex DataSetIDMutex;

std::string Accelerator::AcceleratorType2String(const AcceleratorType type) {
	switch(type) {
		case ACCEL_AUTO:
			return "AUTO";
		case ACCEL_BVH:
			return "BVH";
		case ACCEL_QBVH:
			return "QBVH";
		case ACCEL_MQBVH:
			return "MQBVH";
		case ACCEL_MBVH:
			return "MBVH";
		default:
			throw std::runtime_error("Unknown AcceleratorType in AcceleratorType2String()");
	}
}

DataSet::DataSet(const Context *luxRaysContext) {
	{
		boost::unique_lock<boost::mutex> lock(DataSetIDMutex);
		dataSetID = DataSetID++;
	}
	context = luxRaysContext;

	totalVertexCount = 0;
	totalTriangleCount = 0;

	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;
	hasInstances = false;
}

DataSet::~DataSet() {
	for (std::map<AcceleratorType, Accelerator *>::const_iterator it = accels.begin(); it != accels.end(); ++it)
		delete it->second;
}

TriangleMeshID DataSet::Add(const Mesh *mesh) {
	const TriangleMeshID id = meshes.size();
	meshes.push_back(mesh);

	totalVertexCount += mesh->GetTotalVertexCount();
	totalTriangleCount += mesh->GetTotalTriangleCount();

	bbox = Union(bbox, mesh->GetBBox());
	bsphere = bbox.BoundingSphere();

	for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
		meshIDs.push_back(id);
		meshTriangleIDs.push_back(i);
	}

	if ((mesh->GetType() == TYPE_TRIANGLE_INSTANCE) || (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE))
		hasInstances = true;

	return id;
}

const Accelerator *DataSet::GetAccelerator(const AcceleratorType accelType) {
	std::map<AcceleratorType, Accelerator *>::const_iterator it = accels.find(accelType);
	if (it == accels.end()) {
		LR_LOG(context, "Adding DataSet accelerator: " << Accelerator::AcceleratorType2String(accelType));
		LR_LOG(context, "Total vertex count: " << totalVertexCount);
		LR_LOG(context, "Total triangle count: " << totalTriangleCount);

		if (totalTriangleCount == 0)
			throw std::runtime_error("An empty DataSet can not be preprocessed");

		// Build the Accelerator
		Accelerator *accel;
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
			case ACCEL_MBVH: {
				const int treeType = 4; // Tree type to generate (2 = binary, 4 = quad, 8 = octree)
				const int costSamples = 0; // Samples to get for cost minimization
				const int isectCost = 80;
				const int travCost = 10;
				const float emptyBonus = 0.5f;

				accel = new MBVHAccel(context, treeType, costSamples, isectCost, travCost, emptyBonus);
				break;
			}
			default:
				throw std::runtime_error("Unknown AcceleratorType in DataSet::AddAccelerator()");
		}

		accel->Init(meshes, totalVertexCount, totalTriangleCount);

		accels[accelType] = accel;

		return accel;
	} else
		return it->second;
}

bool DataSet::DoesAllAcceleratorsSupportUpdate() const {
	for (std::map<AcceleratorType, Accelerator *>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		if (!it->second->DoesSupportUpdate())
			return false;
	}

	return true;
}

const void DataSet::Update() {
	for (std::map<AcceleratorType, Accelerator *>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		assert(it->second->DoesSupportUpdate());
		it->second->Update();
	}
}

bool DataSet::IsEqual(const DataSet *dataSet) const {
	return (dataSet != NULL) && (dataSetID == dataSet->dataSetID);
}
