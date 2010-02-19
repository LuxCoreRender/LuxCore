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

#include <string.h>

#include <cassert>
#include <deque>

#include "luxrays/core/trianglemesh.h"

namespace luxrays {

TriangleMesh *TriangleMesh::Merge(
	const std::deque<TriangleMesh *> &meshes,
	TriangleMeshID **preprocessedMeshIDs) {
	unsigned int totalVertexCount = 0;
	unsigned int totalTriangleCount = 0;

	for (std::deque<TriangleMesh *>::const_iterator m = meshes.begin(); m < meshes.end(); m++) {
		const TriangleMesh *mesh = *m;
		totalVertexCount += mesh->GetTotalVertexCount();
		totalTriangleCount += mesh->GetTotalTriangleCount();
	}

	return Merge(totalVertexCount, totalTriangleCount, meshes, preprocessedMeshIDs);
}

TriangleMesh *TriangleMesh::Merge(
	const unsigned int totalVertexCount,
	const unsigned int totalTriangleCount,
	const std::deque<TriangleMesh *> &meshes,
	TriangleMeshID **preprocessedMeshIDs) {
	assert (totalVertexCount > 0);
	assert (totalTriangleCount > 0);
	assert (meshes.size() > 0);

	Point *v = new Point[totalVertexCount];
	Triangle *i = new Triangle[totalTriangleCount];

	if (preprocessedMeshIDs)
		*preprocessedMeshIDs = new TriangleMeshID[totalTriangleCount];

	unsigned int vIndex = 0;
	unsigned int iIndex = 0;
	TriangleMeshID currentID = 0;
	for (std::deque<TriangleMesh *>::const_iterator m = meshes.begin(); m < meshes.end(); m++) {
		const TriangleMesh *mesh = *m;

		// Copy the mesh vertices
		memcpy(&v[vIndex], mesh->GetVertices(), sizeof(Point) * mesh->GetTotalVertexCount());

		// Translate mesh indices
		const Triangle *tris = mesh->GetTriangles();
		for (unsigned int j = 0; j < mesh->GetTotalTriangleCount(); j++) {
			i[iIndex].v[0] = tris[j].v[0] + vIndex;
			i[iIndex].v[1] = tris[j].v[1] + vIndex;
			i[iIndex++].v[2] = tris[j].v[2] + vIndex;
		}

		vIndex += mesh->GetTotalVertexCount();
		if (preprocessedMeshIDs) {
			(*preprocessedMeshIDs)[currentID] = currentID;
			// To avoid compiler warning
			currentID = currentID + 1;
		}
	}

	return new TriangleMesh(totalVertexCount, totalTriangleCount, v, i);
}

}
