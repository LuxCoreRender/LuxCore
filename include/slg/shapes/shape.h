/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "luxrays/core/exttrianglemesh.h"

namespace slg {

class Scene;
	
class Shape {
public:
	typedef enum {
		MESH,
		POINTINESS,
		STRANDS
	} ShapeType;

	Shape() : refined(false) { }
	virtual ~Shape() { }

	virtual ShapeType GetType() const = 0;

	// Note: this method can be called only once and the object is not usable
	// anymore (this is mostly due to optimize memory management).
	luxrays::ExtMesh *Refine(const Scene *scene);

protected:
	virtual luxrays::ExtMesh *RefineImpl(const Scene *scene) = 0;
	
	bool refined;
};

}

#endif	/* _SLG_SHAPE_H */
