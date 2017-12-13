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

#include <vector>

#include <boost/unordered_map.hpp>

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

class MaterialDefinitions {
public:
	MaterialDefinitions();
	~MaterialDefinitions();

	bool IsMaterialDefined(const std::string &name) const {
		return (matsByName.count(name) > 0);
	}
	void DefineMaterial(const std::string &name, Material *m);

	void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	const Material *GetMaterial(const std::string &name) const;
	const Material *GetMaterial(const u_int index) const {
		return mats[index];
	}
	u_int GetMaterialIndex(const std::string &name);
	u_int GetMaterialIndex(const Material *m) const;

	u_int GetSize() const { return static_cast<u_int>(mats.size()); }
	std::vector<std::string> GetMaterialNames() const;

	void DeleteMaterial(const std::string &name);
  
private:
	std::vector<Material *> mats;
	boost::unordered_map<std::string, Material *> matsByName;
};

}

#endif	/* _SLG_MATERIALDEFS_H */
