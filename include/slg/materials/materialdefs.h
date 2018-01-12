/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_MATERIALDEFS_H
#define	_SLG_MATERIALDEFS_H

#include <string>
#include <vector>

#include "luxrays/core/namedobjectvector.h"
#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

class MaterialDefinitions {
public:
	MaterialDefinitions() { }
	~MaterialDefinitions() { }

	bool IsMaterialDefined(const std::string &name) const {
		return mats.IsObjDefined(name);
	}
	void DefineMaterial(Material *m);

	const Material *GetMaterial(const std::string &name) const {
		return static_cast<const Material *>(mats.GetObj(name));
	}
	const Material *GetMaterial(const u_int index) const {
		return static_cast<const Material *>(mats.GetObj(index));
	}
	u_int GetMaterialIndex(const std::string &name) const {
		return mats.GetIndex(name);
	}
	u_int GetMaterialIndex(const Material *m) const {
		return mats.GetIndex(m);
	}

	u_int GetSize() const {
		return mats.GetSize();
	}

	void GetMaterialNames(std::vector<std::string> &names) const {
		mats.GetNames(names);
	}

	void DeleteMaterial(const std::string &name) {
		mats.DeleteObj(name);
	}

	void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);
  
private:
	luxrays::NamedObjectVector mats;
};

}

#endif	/* _SLG_MATERIALDEFS_H */
