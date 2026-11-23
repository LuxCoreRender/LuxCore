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

#ifndef _SLG_SCENE_SHAPE_H
#define	_SLG_SCENE_SHAPE_H

#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "luxrays/core/namedobject.h"
#include "luxrays/utils/buffer.h"
#include "slg/usings.h"
#include "slg/shapes/subdiv_corner_data.h"

namespace luxrays {
class ExtTriangleMesh;
}

namespace slg {

//------------------------------------------------------------------------------
// Shape
//------------------------------------------------------------------------------

// A Shape is a handle grouping the submeshes of one object: it owns the
// shape name and the canonical (shared) vertex buffer, and references its
// submeshes (ExtTriangleMesh). Shape modifiers take a Shape as input, not a
// submesh, so that vertex-level modifiers (displacement, merge on distance)
// run once on the common point buffer and all submeshes stay welded.
class Shape : public luxrays::NamedObject {
public:
	Shape(const std::string &name, const luxrays::VertexBuffer &vertices) :
		NamedObject(name), vertices(vertices) { }
	virtual ~Shape() = default;

	// Canonical shared point buffer of the shape. A Buffer copy shares the
	// storage, so the submeshes of the shape reference the same points.
	const luxrays::VertexBuffer &GetVertices() const { return vertices; }
	luxrays::VertexBuffer &GetVertices() { return vertices; }
	// Replace the canonical buffer (Buffer copy: shares the storage)
	void SetVertices(const luxrays::VertexBuffer &v) { vertices = v; }

	// Register a submesh already defined in the scene mesh cache
	void AddSubmesh(luxrays::ExtTriangleMesh &mesh) {
		submeshes.emplace_back(mesh);
	}
	void RemoveSubmesh(luxrays::ExtTriangleMesh &mesh) {
		submeshes.erase(
			std::remove_if(submeshes.begin(), submeshes.end(),
				[&](const auto &r) { return &r.get() == &mesh; }),
			submeshes.end());
	}
	// Register a submesh not yet defined in the scene mesh cache: the Shape
	// keeps ownership until ReleaseOwnedMeshes() hands the meshes over to
	// Scene::DefineMesh()
	void AddSubmesh(luxrays::ExtTriangleMeshUPtr &&mesh) {
		submeshes.emplace_back(*mesh);
		ownedMeshes.push_back(std::move(mesh));
	}
	const std::vector<std::reference_wrapper<luxrays::ExtTriangleMesh>> &
	GetSubmeshes() const { return submeshes; }
	u_int GetSubmeshCount() const { return (u_int)submeshes.size(); }

	// Cache of subdivided envelopes: ONE subdivision run per subdiv
	// parameter set, shared by all the per-material-slot (matslot) shapes
	// subdividing this Shape, which then resplit the result per slot.
	// The Shape replaces the old scene-level cache keyed by the point-buffer
	// pointer: the buffer identity is the Shape itself. Pure runtime cache
	// (not serialized), cleared when meshes are (re)defined
	// (Scene::ClearSubdivEnvelopeCache).
	struct SubdivEnvelopeCacheEntry {
		luxrays::ExtTriangleMeshSPtr mesh;  // full subdivided envelope
		std::vector<u_int> triBaseFace;
		SubdivCornerData corner;  // per-corner varying data of the subdivided mesh
	};
	std::mutex &GetSubdivEnvelopeMutex() const { return subdivEnvelopeMtx; }
	auto &GetSubdivEnvelopeCache() const { return subdivEnvelopeCache; }
	void ClearSubdivEnvelopeCache() {
		std::lock_guard<std::mutex> lock(subdivEnvelopeMtx);
		subdivEnvelopeCache.clear();
	}

	// Hands over the owned submesh meshes (factory results) to the caller,
	// typically to define them into the scene mesh cache. The submesh
	// references stay valid: moving a unique_ptr does not move the object.
	std::vector<luxrays::ExtTriangleMeshUPtr> ReleaseOwnedMeshes() {
		return std::move(ownedMeshes);
	}

private:
	luxrays::VertexBuffer vertices;
	std::vector<std::reference_wrapper<luxrays::ExtTriangleMesh>> submeshes;
	std::vector<luxrays::ExtTriangleMeshUPtr> ownedMeshes;

	// Subdivided-envelope cache (see above); mutable because it is filled
	// lazily from const shape references during shape refinement
	mutable std::mutex subdivEnvelopeMtx;
	mutable std::unordered_map<std::string, SubdivEnvelopeCacheEntry> subdivEnvelopeCache;
};

// Rebuild a submesh that references the shape's canonical vertex buffer
// (local triangles + vertex map, attribute arrays sized countVertices) as a
// global-indexed mesh sharing that canonical buffer, with attribute layers
// expanded to the canonical point count. Transient working view for shape
// modifiers. Returns nullptr when the mesh owns its points (no vertex map).
luxrays::ExtTriangleMeshUPtr MakeGlobalView(
	const luxrays::ExtTriangleMesh &mesh, const luxrays::VertexBuffer &canonical);

}

#endif	/* _SLG_SCENE_SHAPE_H */
