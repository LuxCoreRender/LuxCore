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

#ifndef _SLG_SHAPE_H
#define	_SLG_SHAPE_H

#include <vector>

#include "slg/usings.h"

namespace luxrays {
	class ExtTriangleMesh;
}

namespace slg {

class Scene;

class Shape {
public:
	typedef enum {
		MESH,
		POINTINESS,
		STRANDS,
		GROUP,
		SUBDIV,
		DISPLACEMENT,
		HARLEQUIN,
		SIMPLIFY,
		ISLANDAOV,
		RANDOMTRIANGLEAOV,
		EDGEDETECTORAOV,
		BEVEL,
		CAMERAPROJUV,
		MERGEONDISTANCE
	} ShapeType;

	Shape() : refined(false) { }
	virtual ~Shape() { }

	virtual ShapeType GetType() const = 0;

	// Shape::Refine is the main method of the API. It is intended to be called
	// during shape parsing (see Scene::ParseShapes)
	// Note: it can be called only once and the object is not usable anymore
	// (it is moved to caller)
	luxrays::ExtTriangleMeshUPtr Refine(SceneConstRef scene);

protected:
	// RefineImpl implements refining for derived classes. It is called by
	// Shape::Refine under the hood.
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) = 0;

	bool refined;

	// Derived classes store the shape which they construct into a
	// std::unique_ptr usually named 'mesh', that they define on their own.
	luxrays::ExtTriangleMeshUPtr mesh;
};

}

#endif	/* _SLG_SHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
