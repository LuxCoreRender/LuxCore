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

#ifndef _SLG_OPENSUBDIVSHAPE_H
#define	_SLG_OPENSUBDIVSHAPE_H

#include <string>

#include <opensubdiv/sdc/options.h>
#include <opensubdiv/sdc/types.h>

#include "slg/shapes/shape.h"

namespace slg {

class OpenSubdivShape : public Shape {
public:
	typedef enum {
		REFINE_UNIFORM,
		REFINE_ADAPTIVE
	} RefineType;

	OpenSubdivShape(luxrays::ExtMesh *mesh);
	virtual ~OpenSubdivShape();

	virtual ShapeType GetType() const { return OPENSUBDIV; }

	OpenSubdiv::Sdc::SchemeType schemeType;
	OpenSubdiv::Sdc::Options options;

	RefineType refineType;
	u_int refineMaxLevel;

protected:
	virtual luxrays::ExtMesh *RefineImpl(const Scene *scene);

	luxrays::ExtMesh *mesh;
};

}

#endif	/* _SLG_OPENSUBDIVSHAPE_H */
