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

#pragma once

#include "slg/shapes/shape.h"

namespace slg {

class MergeOnDistanceShape : public ShapeModifier {
public:
	MergeOnDistanceShape(
		luxrays::ExtTriangleMeshRef srcMesh,
		u_int tolerance,
		bool importNormals
	);

	virtual ~MergeOnDistanceShape();
	virtual ShapeType GetType() const override { return MERGEONDISTANCE; }

	static luxrays::ExtTriangleMeshUPtr ApplyMergeOnDistance(
		luxrays::ExtTriangleMeshRef srcMesh,
		u_int tolerance,
		bool importNormals
	);

	// Shape-level application: the merge is computed ONCE on the shape
	// common point buffer and the welded buffer is shared by all result
	// submeshes (a Buffer copy shares storage), so shared edges stay welded
	static luxrays::ShapeUPtr ApplyToShape(
		luxrays::ShapeConstRef srcShape,
		u_int tolerance,
		bool importNormals,
		const std::string &resultName
	);

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(slg::SceneConstRef scene);

};

}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
