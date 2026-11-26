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

#ifndef _SLG_GROUPSHAPE_H
#define	_SLG_GROUPSHAPE_H

#include <functional>
#include <string>
#include <vector>
#include <span>
#include <optional>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/geometry/transform.h"

#include "luxrays/usings.h"
#include "slg/shapes/shape.h"

namespace slg {

using luxrays::ExtTriangleMesh;
using luxrays::ExtTriangleMeshRef;
using luxrays::ExtTriangleMeshUPtr;
using luxrays::Transform;

class GroupShape : public ShapeModifier {
public:
	using MeshRefWrapper = std::reference_wrapper<const ExtTriangleMesh>;

	GroupShape(
		std::vector<MeshRefWrapper> meshes,
		std::optional<std::vector<Transform>> trans = std::nullopt
	);
	virtual ~GroupShape();

	virtual ShapeType GetType() const override { return GROUP; }

protected:
	virtual ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;

	std::vector<MeshRefWrapper> meshes;
	std::optional<std::vector<Transform>> trans;
};

}

#endif	/* _SLG_GROUPSHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
