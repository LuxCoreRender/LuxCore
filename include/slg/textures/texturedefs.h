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

#ifndef _SLG_TEXTUREDEFS_H
#define	_SLG_TEXTUREDEFS_H

#include <string>
#include <vector>

#include <boost/unordered_set.hpp>

#include "luxrays/core/namedobjectvector.h"
#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

class TextureDefinitions {
public:
	TextureDefinitions() { }
	~TextureDefinitions() { }

	bool IsTextureDefined(const std::string &name) const {
		return texs.IsObjDefined(name);
	}

	void DefineTexture(Texture *t);

	const Texture *GetTexture(const std::string &name) const {
		return static_cast<const Texture *>(texs.GetObj(name));
	}
	const Texture *GetTexture(const u_int index) const {
		return static_cast<const Texture *>(texs.GetObj(index));
	}
	u_int GetTextureIndex(const std::string &name) const {
		return texs.GetIndex(name);
	}
	u_int GetTextureIndex(const Texture *t) const {
		return texs.GetIndex(t);
	}

	u_int GetSize() const {
		return texs.GetSize();
	}
	void GetTextureNames(std::vector<std::string> &names) const {
		texs.GetNames(names);
	}

	void DeleteTexture(const std::string &name) {
		texs.DeleteObj(name);
	}

	void GetTextureSortedNames(std::vector<std::string> &names) const;

private:
	void GetTextureSortedNamesImpl(const Texture *tex, std::vector<std::string> &names,
			boost::unordered_set<std::string> &doneNames) const;

	luxrays::NamedObjectVector texs;
};

}

#endif	/* _SLG_TEXTUREDEFS_H */
