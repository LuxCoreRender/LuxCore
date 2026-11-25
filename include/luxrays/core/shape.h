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

#ifndef _LUXRAYS_CORE_SHAPE_H
#define	_LUXRAYS_CORE_SHAPE_H

#include <algorithm>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "luxrays/core/namedobject.h"
#include "luxrays/utils/buffer.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/usings.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Shape
//------------------------------------------------------------------------------

// A Shape is a handle grouping the submeshes of one object: it owns the
// shape name and the canonical (shared) vertex buffer, and references its
// submeshes (ExtTriangleMesh). Shape modifiers take a Shape as input, not a
// submesh, so that vertex-level modifiers (displacement, merge on distance)
// run once on the common point buffer and all submeshes stay welded.
//
// NOTE: Shape used to live in the slg layer (slg/scene/shape.h) and
// owned a runtime subdivided-envelope cache. That cache has been moved to a
// static cache owned by the slg SubdivShape (see slg/shapes/subdiv.h); it is
// no longer a member of Shape, so Shape here depends only on luxrays types.
class Shape : public NamedObject {
public:
	Shape(const std::string &name, const luxrays::VertexBuffer &vertices) :
		NamedObject(name), vertices(vertices) { }
	// Used by serialization
	Shape() { }
	// A Shape owns a canonical vertex buffer and shared submesh references,
	// so it must not be copied (a copy would alias the same submeshes and
	// share unique_ptr ownership). Copying is intentionally disabled.
	Shape(const Shape &) = delete;
	Shape &operator=(const Shape &) = delete;
	virtual ~Shape() = default;

	// Canonical shared point buffer of the shape. A Buffer copy shares the
	// storage, so the submeshes of the shape reference the same points.
	const luxrays::VertexBuffer &GetVertices() const { return vertices; }
	luxrays::VertexBuffer &GetVertices() { return vertices; }
	// Replace the canonical buffer (Buffer copy: shares the storage)
	void SetVertices(const luxrays::VertexBuffer &v) { vertices = v; }

	// Register a submesh already defined in the scene mesh cache
	void AddSubmesh(luxrays::ExtTriangleMesh &mesh) {
		for (auto &sm : submeshes)
			if (&sm.get() == &mesh)
				return;
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
		AddSubmesh(*mesh);
		ownedMeshes.push_back(std::move(mesh));
	}
	// Adopt a whole sub-shape: its owned meshes become submeshes of this
	// shape, and the sub-shape itself is kept alive (its meshes reference
	// it through Mesh::GetShape())
	void AddSubmesh(luxrays::ShapeUPtr &&sub) {
		for (auto &mesh : sub->ReleaseOwnedMeshes())
			AddSubmesh(std::move(mesh));
		ownedSubShapes.push_back(std::move(sub));
	}
	const std::vector<std::reference_wrapper<luxrays::ExtTriangleMesh>> &
	GetSubmeshes() const { return submeshes; }
	u_int GetSubmeshCount() const { return (u_int)submeshes.size(); }

	// Hands over the owned submesh meshes (factory results) to the caller,
	// typically to define them into the scene mesh cache. The submesh
	// references stay valid: moving a unique_ptr does not move the object.
	std::vector<luxrays::ExtTriangleMeshUPtr> ReleaseOwnedMeshes() {
		return std::move(ownedMeshes);
	}

	// Transform the canonical vertex buffer (copy-on-write when the buffer
	// storage is shared) and propagate the transform to all the submeshes
	// (vertex normals, triangle normals, area, caches)
	void ApplyTransform(const luxrays::Transform &trans);

	friend class boost::serialization::access;

private:
	luxrays::VertexBuffer vertices;
	std::vector<std::reference_wrapper<luxrays::ExtTriangleMesh>> submeshes;
	std::vector<luxrays::ExtTriangleMeshUPtr> ownedMeshes;
	// Sub-shapes adopted through AddSubmesh(ShapeUPtr&&), kept alive because
	// their meshes hold a const reference to them
	std::vector<luxrays::ShapeUPtr> ownedSubShapes;

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(NamedObject);

		auto vertCount = vertices.Count();
		ar & vertCount;
		if (Archive::is_loading::value)
			vertices.Allocate(vertCount);
		for (size_t i = 0; i < vertCount; ++i)
			ar & vertices[i];
		// submeshes/ownedMeshes/ownedSubShapes are runtime-only: meshes are
		// serialized through the scene mesh cache, and the submesh links are
		// rebuilt on load
	}
};

// Rebuild a submesh into a global-indexed mesh on the Shape (sharing its
// canonical vertex buffer): local triangles + vertex map, attribute arrays sized
// countVertices, attribute layers expanded to the canonical point count.
// Transient working view for shape modifiers. Returns nullptr when the mesh owns
// its points (no vertex map).
luxrays::ExtTriangleMeshUPtr MakeGlobalView(
	const luxrays::ExtTriangleMesh &mesh, const luxrays::Shape &shape);

}

#endif	/* _LUXRAYS_CORE_SHAPE_H */
