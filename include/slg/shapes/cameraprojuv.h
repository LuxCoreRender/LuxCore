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

#ifndef _SLG_CAMERAPROJUV_H
#define	_SLG_CAMERAPROJUV_H

#include <string>

#include "slg/shapes/shape.h"

namespace slg {

class CameraProjUVShape : public Shape {
public:
	CameraProjUVShape(luxrays::ExtTriangleMesh *mesh, const u_int uvIndex);
	virtual ~CameraProjUVShape();

	virtual ShapeType GetType() const { return CAMERAPROJUV; }
	
protected:
	virtual luxrays::ExtTriangleMesh *RefineImpl(const Scene *scene);

	const u_int uvIndex;
	luxrays::ExtTriangleMesh *mesh;
};

}

#endif	/* _SLG_MESHSHAPE_H */
