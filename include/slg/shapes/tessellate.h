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

#include "slg/usings.h"
#include "slg/shapes/shape.h"
// complete type: ApplyToShape returns luxrays::ShapeUPtr (Shape comes from slg/usings.h -> luxrays/core/shape.h)
#include "luxrays/core/exttrianglemesh.h"  // complete type: this header returns ExtTriangleMeshUPtr
#include "luxrays/utils/utils.h"  // u_int (not provided by MSVC system headers)

namespace slg {

// The tessellate shape modifier: standalone flat tessellation of a mesh,
// independent from the subdivision shape. The core tessellation algorithm
// it builds upon lives in tessellate_core.h.

// A standalone shape applying a flat (barycentric) tessellation of the
// source mesh: each edge is split into N = 2^rate segments (same convention
// as the subdivision levels), i.e. each triangle is split in 4^rate
// triangles, the new points being linearly interpolated inside the source
// triangles. All mesh data (UVs, colors, alphas, AOVs, and normals if
// interpolatedNormals) is tessellated along with the vertices.
class TessellateShape : public ShapeModifier {
public:
	TessellateShape(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int rate,
		const bool interpolatedNormals = true
	);
	virtual ~TessellateShape();

	virtual ShapeType GetType() const override { return TESSELLATE; }

	static luxrays::ExtTriangleMeshUPtr ApplyTessellate(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int rate,
		const bool interpolatedNormals = true
	);

	// Shape-level application: tessellate all the submeshes of the source
	// shape TOGETHER when they share one point buffer (merged envelope
	// tessellation, then resplit per submesh), so the result submeshes share
	// a single tessellated buffer and shared edges stay welded (watertight).
	// Submeshes with their own buffer are tessellated per buffer group.
	// Result submeshes are named resultName ("i"+index appended when the
	// source shape has several submeshes).
	static luxrays::ShapeUPtr ApplyToShape(
		luxrays::ShapeConstRef srcShape,
		const u_int rate,
		const bool interpolatedNormals,
		const std::string &resultName
	);

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;

};

}

#endif	/* _SLG_TESSELLATE_SHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
