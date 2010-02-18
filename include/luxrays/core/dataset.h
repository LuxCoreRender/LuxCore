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

#ifndef _LUXRAYS_DATASET_H
#define	_LUXRAYS_DATASET_H

#include <deque>

#include "luxrays/luxrays.h"
#include "luxrays/core/acceleretor.h"
#include "luxrays/core/trianglemesh.h"

namespace luxrays {

class DataSet {
public:
	DataSet(const Context *luxRaysContext);
	~DataSet();

	// NOTE: the mesh is delete by this class destructor
	TriangleMeshID Add(TriangleMesh *mesh);
	void Preprocess();
	bool IsPreprocessed() const { return preprocessed; }

	bool Intersect(const Ray *ray, RayHit *hit) const;

	const TriangleMesh *GetTriangleMesh() const {
		assert (preprocessed);

		return preprocessedMesh;
	}

	void SetAcceleratorType(AcceleratorType type) {
		accelType = type;
	}

	AcceleratorType GetAcceleratorType() const {
		return accelType;
	}

	const Accelerator *GetAccelerator() const {
		assert (preprocessed);

		return accel;
	}

	unsigned int GetTotalVertexCount() const { return totalVertexCount; }
	unsigned int GetTotalTriangleCount() const { return totalTriangleCount; }

private:
	const Context *context;

	unsigned int totalVertexCount;
	unsigned int totalTriangleCount;
	std::deque<TriangleMesh *> meshes;

	bool preprocessed;
	TriangleMesh *preprocessedMesh;
	TriangleMeshID *preprocessedMeshIDs;

	AcceleratorType accelType;
	Accelerator *accel;
};

}

#endif	/* _LUXRAYS_DATASET_H */
