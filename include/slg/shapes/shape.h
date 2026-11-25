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
	class Shape;
}

namespace slg {

class Scene;

class ShapeModifier {
public:
	typedef enum {
		MESH,
		POINTINESS,
		STRANDS,
		GROUP,
		SUBDIV,
		SUBDIV_OLD,
		TESSELLATE,
		DISPLACEMENT,
		HARLEQUIN,
		SIMPLIFY_OLD,
		ISLANDAOV,
		RANDOMTRIANGLEAOV,
		EDGEDETECTORAOV,
		BEVEL,
		CAMERAPROJUV,
		MERGEONDISTANCE,
		SIMPLIFY
	} ShapeType;

	// Defined in shape.cpp: inline ctor/dtor would instantiate
	// std::unique_ptr<ExtTriangleMesh>'s destructor in every TU including
	// this header, requiring the complete ExtTriangleMesh type there
	ShapeModifier();
	virtual ~ShapeModifier();

	virtual ShapeType GetType() const = 0;

	// ShapeModifier::Refine is the main method of the API. It is intended to be called
	// during shape parsing (see Scene::ParseShapes)
	// Note: it can be called only once and the object is not usable anymore
	// (it is moved to caller)
	// Returns the Shape owning the produced mesh: the mesh references the
	// Shape through Mesh::GetShape(), so the Shape must outlive the mesh
	luxrays::ShapeUPtr Refine(SceneConstRef scene);

protected:
	// RefineImpl implements refining for derived classes. It is called by
	// ShapeModifier::Refine under the hood. It must create 'shape' (the
	// Shape the produced mesh is built on) before constructing 'mesh'.
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) = 0;

	bool refined;

	// Derived classes store the shape which they construct into a
	// std::unique_ptr usually named 'mesh', that they define on their own.
	luxrays::ExtTriangleMeshUPtr mesh;
	// The Shape the produced mesh is built on (owns the canonical vertex
	// buffer of the mesh)
	luxrays::ShapeUPtr shape;
};

}

#endif	/* _SLG_SHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
