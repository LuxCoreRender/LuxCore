/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include <mutex>

#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"
#include "luxrays/usings.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/accelerators/mbvhaccel.h"
#include "luxrays/accelerators/embreeaccel.h"
#include "luxrays/accelerators/optixaccel.h"
#include "luxrays/core/geometry/bsphere.h"

using namespace luxrays;
using namespace std;

static u_int DataSetID = 0;
static std::mutex DataSetIDMutex;

DataSet::DataSet(const Context& luxRaysContext): context(luxRaysContext) {
	{
		std::unique_lock<std::mutex> lock(DataSetIDMutex);
		dataSetID = DataSetID++;
	}

	totalVertexCount = 0;
	totalTriangleCount = 0;

	preprocessed = false;
	hasInstances = false;
	hasMotionBlur = false;

	// Configure
	const Properties &cfg = luxRaysContext.GetConfig();
	accelType = Accelerator::String2AcceleratorType(cfg.Get(Property("accelerator.type")("AUTO")).Get<string>());
	enableInstanceSupport = cfg.Get(Property("accelerator.instances.enable")(true)).Get<bool>();
	enableMotionBlurSupport = cfg.Get(Property("accelerator.motionblur.enable")(true)).Get<bool>();
}

DataSet::~DataSet() {
	for (auto it = accels.begin(); it != accels.end(); ++it)
		it->second.reset();
}

TriangleMeshID DataSet::Add(MeshConstRef mesh) {
	assert (!preprocessed);

	const TriangleMeshID id = meshes.size();
	meshes.push_back(&mesh);

	totalVertexCount += mesh.GetTotalVertexCount();
	totalTriangleCount += mesh.GetTotalTriangleCount();

	if ((mesh.GetType() == TYPE_TRIANGLE_INSTANCE) || (mesh.GetType() == TYPE_EXT_TRIANGLE_INSTANCE))
		hasInstances = true;
	else if ((mesh.GetType() == TYPE_TRIANGLE_MOTION) || (mesh.GetType() == TYPE_EXT_TRIANGLE_MOTION))
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
		for(const Mesh * m: meshes)
			bbox = Union(bbox, m->GetBBox());
	}
	bsphere = bbox.BoundingSphere();
}

bool DataSet::HasAccelerator(const AcceleratorType accelType) const {
	auto it = accels.find(accelType);

	return !(it == accels.end());
}

AcceleratorConstSPtr DataSet::GetAccelerator(const AcceleratorType accelType) const {
	auto it = accels.find(accelType);
	if (it == accels.end()) {
		std::unique_lock<std::mutex> lock(accelsMutex);

		// Try again under mutex
		it = accels.find(accelType);

		if (it == accels.end()) {
			LR_LOG(context, "Adding DataSet accelerator: " << Accelerator::AcceleratorType2String(accelType));
			LR_LOG(context, "Total vertex count: " << totalVertexCount);
			LR_LOG(context, "Total triangle count: " << totalTriangleCount);

			// Build the Accelerator
			AcceleratorSPtr accel;
			switch (accelType) {
				case ACCEL_BVH:
					accel = std::make_shared<BVHAccel>(context);
					break;
				case ACCEL_MBVH:
					accel = std::make_shared<MBVHAccel>(context);
					break;
				case ACCEL_EMBREE:
					accel = std::make_shared<EmbreeAccel>(context);
					break;
#if !defined(LUXRAYS_DISABLE_CUDA)
				case ACCEL_OPTIX:
					accel = std::make_shared<OptixAccel>(context);
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
	} else
		return it->second;
}

bool DataSet::DoesAllAcceleratorsSupportUpdate() const {
	for (auto it = accels.begin(); it != accels.end(); ++it) {
		if (!it->second->DoesSupportUpdate())
			return false;
	}

	return true;
}

void DataSet::UpdateAccelerators() {
	for (auto it = accels.begin(); it != accels.end(); ++it) {
		assert(it->second->DoesSupportUpdate());
		it->second->Update();
	}
}

bool DataSet::IsEqual(DataSetConstRPtr dataSet) const {
	return (dataSet != NULL) && (dataSetID == dataSet->dataSetID);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
