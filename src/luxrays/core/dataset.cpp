/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <cstdlib>
#include <cassert>
#include <deque>
#include <sstream>
#include <boost/foreach.hpp>

#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/accelerators/mbvhaccel.h"
#include "luxrays/accelerators/embreeaccel.h"
#include "luxrays/accelerators/optixaccel.h"
#include "luxrays/core/geometry/bsphere.h"

using namespace luxrays;
using namespace std;

static u_int DataSetID = 0;
static boost::mutex DataSetIDMutex;

DataSet::DataSet(const Context *luxRaysContext) {
	{
		boost::unique_lock<boost::mutex> lock(DataSetIDMutex);
		dataSetID = DataSetID++;
	}
	context = luxRaysContext;

	totalVertexCount = 0;
	totalTriangleCount = 0;

	preprocessed = false;
	hasInstances = false;
	hasMotionBlur = false;

	// Configure
	const Properties &cfg = luxRaysContext->GetConfig();
	accelType = Accelerator::String2AcceleratorType(cfg.Get(Property("accelerator.type")("AUTO")).Get<string>());
	enableInstanceSupport = cfg.Get(Property("accelerator.instances.enable")(true)).Get<bool>();
	enableMotionBlurSupport = cfg.Get(Property("accelerator.motionblur.enable")(true)).Get<bool>();
}

DataSet::~DataSet() {
	for (boost::unordered_map<AcceleratorType, Accelerator *>::const_iterator it = accels.begin(); it != accels.end(); ++it)
		delete it->second;
}

TriangleMeshID DataSet::Add(const Mesh *mesh) {
	assert (!preprocessed);

	const TriangleMeshID id = meshes.size();
	meshes.push_back(mesh);

	totalVertexCount += mesh->GetTotalVertexCount();
	totalTriangleCount += mesh->GetTotalTriangleCount();

	if ((mesh->GetType() == TYPE_TRIANGLE_INSTANCE) || (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE))
		hasInstances = true;
	else if ((mesh->GetType() == TYPE_TRIANGLE_MOTION) || (mesh->GetType() == TYPE_EXT_TRIANGLE_MOTION))
		hasMotionBlur = true;

	return id;
}

void DataSet::Preprocess() {
	assert (!preprocessed);

	LR_LOG(context, "Preprocessing DataSet");
	LR_LOG(context, "Total vertex count: " << totalVertexCount);
	LR_LOG(context, "Total triangle count: " << totalTriangleCount);

	DataSet::UpdateBBoxes();

	preprocessed = true;
	LR_LOG(context, "Preprocessing DataSet done");
}

void DataSet::UpdateBBoxes() {
	if (totalTriangleCount == 0) {
		// Just initialize with some default value to avoid problems
		bbox = Union(Union(bbox, Point(-1.f, -1.f, -1.f)), Point(1.f, 1.f, 1.f));
	} else {
		BOOST_FOREACH(const Mesh *m, meshes)
			bbox = Union(bbox, m->GetBBox());
	}
	bsphere = bbox.BoundingSphere();
}

bool DataSet::HasAccelerator(const AcceleratorType accelType) const {
	boost::unordered_map<AcceleratorType, Accelerator *>::const_iterator it = accels.find(accelType);
	
	return !(it == accels.end());
}

const Accelerator *DataSet::GetAccelerator(const AcceleratorType accelType) {
	boost::unordered_map<AcceleratorType, Accelerator *>::const_iterator it = accels.find(accelType);
	if (it == accels.end()) {
		LR_LOG(context, "Adding DataSet accelerator: " << Accelerator::AcceleratorType2String(accelType));
		LR_LOG(context, "Total vertex count: " << totalVertexCount);
		LR_LOG(context, "Total triangle count: " << totalTriangleCount);

		// Build the Accelerator
		Accelerator *accel;
		switch (accelType) {
			case ACCEL_BVH:
				accel = new BVHAccel(context);
				break;
			case ACCEL_MBVH:
				accel = new MBVHAccel(context);
				break;
			case ACCEL_EMBREE:
				accel = new EmbreeAccel(context);
				break;
#if !defined(LUXRAYS_DISABLE_CUDA)
			case ACCEL_OPTIX:
				accel = new OptixAccel(context);
				break;
#endif
			default:
				throw runtime_error("Unknown AcceleratorType in DataSet::AddAccelerator()");
		}

		accel->Init(meshes, totalVertexCount, totalTriangleCount);

		accels[accelType] = accel;

		return accel;
	} else
		return it->second;
}

bool DataSet::DoesAllAcceleratorsSupportUpdate() const {
	for (boost::unordered_map<AcceleratorType, Accelerator *>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		if (!it->second->DoesSupportUpdate())
			return false;
	}

	return true;
}

void DataSet::UpdateAccelerators() {
	for (boost::unordered_map<AcceleratorType, Accelerator *>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		assert(it->second->DoesSupportUpdate());
		it->second->Update();
	}
}

bool DataSet::IsEqual(const DataSet *dataSet) const {
	return (dataSet != NULL) && (dataSetID == dataSet->dataSetID);
}
