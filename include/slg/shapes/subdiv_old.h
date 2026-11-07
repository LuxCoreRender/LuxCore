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

#ifndef _SLG_SUBDIV_OLD_SHAPE_H
#define	_SLG_SUBDIV_OLD_SHAPE_H

#include <string>

#include "slg/usings.h"
#include "slg/shapes/shape.h"
#include "luxrays/core/exttrianglemesh.h"  // complete type: this header returns ExtTriangleMeshUPtr
#include "luxrays/utils/utils.h"  // u_int (not provided by MSVC system headers)

namespace slg {

// The former "simple" (uniform, non-adaptive) subdivision path, kept as a
// distinct shape for backward compatibility
class SubdivOldShape : public Shape {
public:
	SubdivOldShape(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int maxLevel
	);
	virtual ~SubdivOldShape();

	virtual ShapeType GetType() const override { return SUBDIV_OLD; }

	static luxrays::ExtTriangleMeshUPtr ApplySubdiv(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int maxLevel
	);

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;

};

}

#endif	/* _SLG_SUBDIV_OLD_SHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
