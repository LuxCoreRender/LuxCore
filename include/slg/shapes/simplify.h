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

#ifndef _SLG_SIMPLIFYSHAPE2_H
#define	_SLG_SIMPLIFYSHAPE2_H


#include "luxrays/usings.h"
#include "slg/usings.h"
#include "slg/shapes/shape.h"

namespace slg {

class SimplifyShape2 : public Shape {
public:
	SimplifyShape2(CameraConstPtr camera, luxrays::ExtTriangleMeshRef srcMesh,
			const float target, const float edgeScreenSize, const bool preserveBorder);
	virtual ~SimplifyShape2();

	virtual ShapeType GetType() const override { return SIMPLIFY; }

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;

};

} // namespace slg


#endif	/* _SLG_SIMPLIFYSHAPE2_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
