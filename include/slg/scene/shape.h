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
#include <string>
#include <vector>

#include "luxrays/core/namedobject.h"
#include "luxrays/utils/buffer.h"
#include "slg/usings.h"

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
};

}

#endif	/* _SLG_SCENE_SHAPE_H */
