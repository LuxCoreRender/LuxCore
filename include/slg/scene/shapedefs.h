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

#ifndef _SLG_SHAPEDEFS_H
#define	_SLG_SHAPEDEFS_H

#include <string>
#include <tuple>
#include <vector>

#include "luxrays/core/namedobjectvector.h"
#include "slg/scene/shape.h"
#include "slg/usings.h"

namespace slg {

//------------------------------------------------------------------------------
// ShapeDefinitions
//------------------------------------------------------------------------------

// Container of the Shape handles of a scene, keyed by shape name (a
// NamedObjectVector, like the mesh and object caches).
class ShapeDefinitions {
public:
	ShapeDefinitions() { }
	~ShapeDefinitions() { }

	bool IsShapeDefined(const std::string &name) const {
		return shapes.IsObjDefined(name);
	}
	std::tuple<Shape&, ShapeUPtr> DefineShape(ShapeUPtr&& s) {
		return shapes.DefineObj<Shape>(std::move(s));
	}

	ShapeConstRef GetShape(const std::string &name) const {
		return dynamic_cast<ShapeConstRef>(shapes.GetObj(name));
	}
	ShapeRef GetShape(const std::string &name) {
		return dynamic_cast<ShapeRef>(shapes.GetObj(name));
	}
	ShapeConstRef GetShape(const u_int index) const {
		return dynamic_cast<ShapeConstRef>(shapes.GetObj(index));
	}
	ShapeRef GetShape(const u_int index) {
		return dynamic_cast<ShapeRef>(shapes.GetObj(index));
	}
	u_int GetShapeIndex(const std::string &name) const {
		return shapes.GetIndex(name);
	}

	u_int GetSize() const {
		return shapes.GetSize();
	}
	auto GetShapeNames() const {
		return shapes.GetNames();
	}

	ShapeUPtr DeleteShape(const std::string &name) {
		return shapes.DeleteObj<Shape>(name);
	}

private:
	luxrays::NamedObjectVector shapes;
};

}

#endif	/* _SLG_SHAPEDEFS_H */
