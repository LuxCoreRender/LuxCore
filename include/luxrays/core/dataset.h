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

#ifndef _LUXRAYS_DATASET_H
#define	_LUXRAYS_DATASET_H

#include <deque>
#include <vector>

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

	bool GetInstanceSupport(const bool v) const { return enableInstanceSupport; }
	void SetInstanceSupport(const bool v) { enableInstanceSupport = v; }
	bool RequiresInstanceSupport() const { return enableInstanceSupport && hasInstances; }
	bool HasInstances() const { return hasInstances; }

	TriangleMeshID Add(const Mesh *mesh);

	const Accelerator *GetAccelerator(const AcceleratorType accelType);
	bool DoesAllAcceleratorsSupportUpdate() const;
	const void Update();

	const BBox &GetBBox() const { return bbox; }
	const BSphere &GetBSphere() const { return bsphere; }

	unsigned int GetTotalVertexCount() const { return totalVertexCount; }
	unsigned int GetTotalTriangleCount() const { return totalTriangleCount; }

	unsigned int GetDataSetID() const { return dataSetID; }
	bool IsEqual(const DataSet *dataSet) const;

	const TriangleMeshID GetMeshID(const u_int index) const {
		return meshIDs[index];
	}
	const TriangleMeshID *GetMeshIDTable() const {
		return &meshIDs[0];
	}
	const TriangleID GetMeshTriangleID(const u_int index) const {
		return meshTriangleIDs[index];
	}
	const TriangleID *GetMeshTriangleIDTable() const {
		return &meshTriangleIDs[0];
	}

	friend class Context;
	friend class OpenCLIntersectionDevice;

private:
	unsigned int dataSetID;

	const Context *context;

	unsigned int totalVertexCount;
	unsigned int totalTriangleCount;
	std::deque<const Mesh *> meshes;
	std::vector<TriangleMeshID> meshIDs;
	std::vector<TriangleID> meshTriangleIDs;

	BBox bbox;
	BSphere bsphere;

	std::map<AcceleratorType, Accelerator *> accels;

	AcceleratorType accelType;
	bool hasInstances, enableInstanceSupport;
};

}

#endif	/* _LUXRAYS_DATASET_H */
