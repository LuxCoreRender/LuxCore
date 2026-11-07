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

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/shape.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

// Defined here (and not inline in the header) so that the construction
// and destruction of the 'mesh' member (std::unique_ptr<ExtTriangleMesh>)
// happens with the complete type, visible via exttrianglemesh.h
Shape::Shape() : refined(false) { }
Shape::~Shape() { }

ExtTriangleMeshUPtr Shape::Refine(SceneConstRef scene) {
	if (refined)
		throw runtime_error("Called Shape::Refine() on an already refined shape");

	auto mesh = RefineImpl(scene);
	refined = true;

	return mesh;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
