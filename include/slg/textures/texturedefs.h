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

#ifndef _SLG_TEXTUREDEFS_H
#define	_SLG_TEXTUREDEFS_H

#include <string>
#include <vector>

#include "luxrays/core/namedobjectvector.h"
#include "luxrays/usings.h"
#include "slg/textures/texture.h"
#include "slg/usings.h"

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

    std::tuple<TextureRef&, TextureUPtr> DefineTexture(TextureUPtr&& t);

	TextureConstRef GetTexture(const std::string &name) const {
		return dynamic_cast<TextureConstRef>(texs.GetObj(name));
	}
	TextureRef GetTexture(const std::string &name) {
		return dynamic_cast<TextureRef>(texs.GetObj(name));
	}

	TextureConstRef GetTexture(const u_int index) const {
		return dynamic_cast<TextureConstRef>(texs.GetObj(index));
	}
	u_int GetTextureIndex(const std::string &name) const {
		return texs.GetIndex(name);
	}
	u_int GetTextureIndex(TextureConstRef t) const {
		return texs.GetIndex(t);
	}
	u_int GetTextureIndex(TextureConstPtr t) const {
		return texs.GetIndex(*t);  // Will throw if t is nullopt
	}

	u_int GetSize() const {
		return texs.GetSize();
	}
	auto GetTextureNames() const {
		return texs.GetNames();
	}

	TextureUPtr DeleteTexture(const std::string &name) {
		return std::move(texs.DeleteObj<Texture>(name));
	}

	const std::vector<std::string> GetTextureSortedNames() const;

private:
	void GetTextureSortedNamesImpl(
		TextureConstRef tex,
		std::vector<std::string> &names,
		std::unordered_set<std::string> &doneNames
	) const;

	luxrays::NamedObjectVector texs;
};

}

#endif	/* _SLG_TEXTUREDEFS_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
