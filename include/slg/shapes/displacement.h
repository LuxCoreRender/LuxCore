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

#ifndef _SLG_DISPLACEMENTSHAPE_H
#define	_SLG_DISPLACEMENTSHAPE_H

#include <string>

#include "slg/shapes/shape.h"

namespace slg {

class Texture;

class DisplacementShape : public ShapeModifier {
public:
	typedef enum {
		HIGHT_DISPLACEMENT,
		VECTOR_DISPLACEMENT
	} DisplacementType;

	typedef struct {
		DisplacementType mapType;
		u_int mapChannels[3];
		float scale;
		float offset;
		u_int uvIndex;
		bool normalSmooth;
	} Params;

	DisplacementShape(luxrays::ExtTriangleMeshRef srcMesh, const Texture &dispMap,
			const Params &params);
	virtual ~DisplacementShape();

	// Shape-level application: the displacement is computed ONCE on the
	// shape common point buffer and the result submeshes share the displaced
	// buffer (a Buffer copy shares storage), so shared edges stay welded
	// (watertight). Normals/UVs are read from the first submesh.
	static ShapeUPtr ApplyToShape(ShapeConstRef srcShape, const Texture &dispMap,
			const Params &params, const std::string &resultName);

	virtual ShapeType GetType() const override { return DISPLACEMENT; }

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;
};

}

#endif	/* _SLG_SUBDIVSHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
