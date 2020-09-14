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

#ifndef _LUXRAYS_DATASET_H
#define	_LUXRAYS_DATASET_H

#include <deque>
#include <vector>

#include <boost/unordered_map.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"
#include "luxrays/core/trianglemesh.h"

namespace luxrays {

class DataSet {
public:
	DataSet(const Context *luxRaysContext);
	~DataSet();

	AcceleratorType GetAcceleratorType() const { return accelType; }
	void SetAcceleratorType(AcceleratorType type) { accelType = type; }

	bool GetInstanceSupport() const { return enableInstanceSupport; }
	bool RequiresInstanceSupport() const { return enableInstanceSupport && hasInstances; }
	bool HasInstances() const { return hasInstances; }

	bool GetMotionBlurSupport() const { return hasMotionBlur; }
	bool RequiresMotionBlurSupport() const { return enableMotionBlurSupport && hasMotionBlur; }
	bool HasMotionBlur() const { return hasMotionBlur; }

	TriangleMeshID Add(const Mesh *mesh);
	void Preprocess();
	bool IsPreprocessed() const { return preprocessed; }
	void UpdateBBoxes();

	bool HasAccelerator(const AcceleratorType accelType) const;
	const Accelerator *GetAccelerator(const AcceleratorType accelType);
	bool DoesAllAcceleratorsSupportUpdate() const;
	void UpdateAccelerators();

	const BBox &GetBBox() const { return bbox; }
	const BSphere &GetBSphere() const { return bsphere; }

	u_longlong GetTotalVertexCount() const { return totalVertexCount; }
	u_longlong GetTotalTriangleCount() const { return totalTriangleCount; }

	u_int GetDataSetID() const { return dataSetID; }
	bool IsEqual(const DataSet *dataSet) const;

	friend class Context;
	friend class OpenCLIntersectionDevice;

private:
	u_int dataSetID;

	const Context *context;

	u_longlong totalVertexCount;
	u_longlong totalTriangleCount;
	std::deque<const Mesh *> meshes;

	BBox bbox;
	BSphere bsphere;

	boost::unordered_map<AcceleratorType, Accelerator *> accels;

	AcceleratorType accelType;
	bool preprocessed;
	bool hasInstances, enableInstanceSupport;
	bool hasMotionBlur, enableMotionBlurSupport;
};

}

#endif	/* _LUXRAYS_DATASET_H */
