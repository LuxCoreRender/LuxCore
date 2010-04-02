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

#ifndef _LUXRAYS_EXTTRIANGLEMESH_H
#define	_LUXRAYS_EXTTRIANGLEMESH_H

#include <cassert>
#include <cstdlib>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/utils/core/spectrum.h"

namespace luxrays {

class ExtTriangleMesh : public TriangleMesh {
public:
	ExtTriangleMesh(const unsigned int meshVertCount, const unsigned int meshTriCount,
			Point *meshVertices, Triangle *meshTris, Normal *meshNormals = NULL, Spectrum *meshColors = NULL, UV *meshUV = NULL) :
			TriangleMesh(meshVertCount, meshTriCount, meshVertices, meshTris) {
		normals = meshNormals;
		colors = meshColors;
		uvs = meshUV;
	}
	~ExtTriangleMesh() { };
	virtual void Delete() {
		TriangleMesh::Delete();
		delete[] normals;
		delete[] colors;
		delete[] uvs;
	}

	Normal *GetNormal() const { return normals; }
	Spectrum *GetColors() const { return colors; }
	UV *GetUVs() const { return uvs; }

	static ExtTriangleMesh *LoadExtTriangleMesh(Context *ctx, const std::string &fileName, const bool usePlyNormals = false);
	static ExtTriangleMesh *Merge(
		const std::deque<ExtTriangleMesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);
	static ExtTriangleMesh *Merge(
		const unsigned int totalVerticesCount,
		const unsigned int totalIndicesCount,
		const std::deque<ExtTriangleMesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);

private:
	Normal *normals;
	Spectrum *colors;
	UV *uvs;
};

inline Normal InterpolateTriNormal(const Triangle &tri, const Normal *normals, const float b1, const float b2) {
	const float b0 = 1.f - b1 - b2;
	return Normalize(b0 * normals[tri.v[0]] + b1 * normals[tri.v[1]] + b2 * normals[tri.v[2]]);
}

inline Spectrum InterpolateTriColor(const Triangle &tri, const Spectrum *colors, const float b0, const float b1, const float b2) {
	return b0 * colors[tri.v[0]] + b1 * colors[tri.v[1]] + b2 * colors[tri.v[2]];
}

inline Spectrum InterpolateTriColor(const Triangle &tri, const Spectrum *colors, const float b1, const float b2) {
	const float b0 = 1.f - b1 - b2;
	return b0 * colors[tri.v[0]] + b1 * colors[tri.v[1]] + b2 * colors[tri.v[2]];
}

inline UV InterpolateTriUV(const Triangle &tri, const UV *uvs, const float b1, const float b2) {
	const float b0 = 1.f - b1 - b2;
	return b0 * uvs[tri.v[0]] + b1 * uvs[tri.v[1]] + b2 * uvs[tri.v[2]];
}

}

#endif	/* _LUXRAYS_EXTTRIANGLEMESH_H */
