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

#ifndef _SLG_TESSELLATE_SHAPE_H
#define	_SLG_TESSELLATE_SHAPE_H

#include <string>
#include <tuple>
#include <vector>

#include <opensubdiv/far/topologyRefiner.h>

// OpenSubdiv declares Far inside OpenSubdiv::OPENSUBDIV_VERSION
namespace Far = OpenSubdiv::Far;

#include "slg/usings.h"
#include "slg/shapes/shape.h"
#include "luxrays/core/exttrianglemesh.h"  // complete type: this header returns ExtTriangleMeshUPtr
#include "luxrays/utils/utils.h"  // u_int (not provided by MSVC system headers)
#include "luxrays/utils/buffer.h"

namespace slg {

// Storage of local coordinates (coordinates in a given face) during
// tessellation, in the form (face, x, y).
struct LocalCoords {
	int face = -1;
	float x = 0.f;
	float y = 0.f;

	LocalCoords() {}

	LocalCoords(int p_face, float p_x, float p_y):
		face(p_face),
		x(p_x),
		y(p_y)
	{ }

	LocalCoords(int p_face, int p_i, int p_j, int N):
		face(p_face),
		x(float(p_i) / float(N)),
		y(float(p_j) / float(N))
	{ }

	// Avoid unexpected use of LocalCoords(int, float, float)
	// with implicit conversion
	LocalCoords(int, int, int) = delete;

	// Interpolating constructor
	LocalCoords(LocalCoords p0, LocalCoords p1, u_int weight, u_int total) {
		assert(p0.face == p1.face);
		assert(weight <= total);

		face = p0.face;

		float weight_f = float(weight);
		float total_f = float(total);
		x = (weight_f * p1.x + (total_f - weight_f) * p0.x) / total_f;
		y = (weight_f * p1.y + (total_f - weight_f) * p0.y) / total_f;
	}
};

typedef std::vector<LocalCoords> CoordVector;

// Triforce tessellation of a topology: splits each edge in N segments and
// generates N^2 triangles per input triangle. Multithreaded reimplementation
// of the OSD tessellation (the OSD one is monothreaded).
//
// This is the API used by the subdivision shape: it is called directly on
// the subdivision refiner.
//
// Returns (tessCoords, tessTris, numPoints, numTriangles).
std::tuple<CoordVector, luxrays::TriangleBuffer, int, int>
TessellateTopology(const Far::TopologyRefiner &refiner, const size_t N);

// Number of positions / triangles after an N-tessellation of the given
// (base level) topology
int GetTessellationPosCount(const Far::TopologyLevel &topology, int N);
int GetTessellationTriangleCount(const Far::TopologyLevel &topology, int N);

// A standalone shape applying a flat (barycentric) tessellation of the
// source mesh: each triangle is split in rate^2 triangles, the new points
// being linearly interpolated inside the source triangles.
class TessellateShape : public Shape {
public:
	TessellateShape(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int rate
	);
	virtual ~TessellateShape();

	virtual ShapeType GetType() const override { return TESSELLATE; }

	static luxrays::ExtTriangleMeshUPtr ApplyTessellate(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int rate
	);

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;

};

}

#endif	/* _SLG_TESSELLATE_SHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
