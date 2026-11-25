/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/shape.h"

using namespace luxrays;

void Shape::ApplyTransform(const Transform &trans) {
	// Copy-on-write: the vertex storage may be shared with other Shapes
	// (copying a Buffer shares the storage), and shared content must never
	// be modified in place
	if (vertices.IsShared()) {
		VertexBuffer copy;
		copy.Allocate(vertices.Count());
		copy.Set(vertices);
		vertices = std::move(copy);
	}

	for (size_t i = 0; i < vertices.Count(); ++i)
		vertices[i] *= trans;

	// Propagate the transform to all the submeshes (vertex normals,
	// triangle normals, area and bbox caches are refreshed by the meshes)
	for (auto &sm : submeshes)
		sm.get().OnShapeTransformed(trans);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
