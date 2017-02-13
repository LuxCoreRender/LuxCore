/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

//#include <algorithm>
//#include <numeric>
//#include <memory>
//#include <boost/format.hpp>
#include <boost/foreach.hpp>
//#include <boost/filesystem.hpp>

#include "slg/textures/texturedefs.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

TextureDefinitions::TextureDefinitions() { }

TextureDefinitions::~TextureDefinitions() {
	BOOST_FOREACH(Texture *t, texs)
		delete t;
}

void TextureDefinitions::DefineTexture(const string &name, Texture *newTex) {
	if (IsTextureDefined(name)) {
		const Texture *oldTex = GetTexture(name);

		// Update name/texture definition
		const u_int index = GetTextureIndex(name);
		texs[index] = newTex;
		texsByName.erase(name);
		texsByName.insert(std::make_pair(name, newTex));

		// Update all references
		BOOST_FOREACH(Texture *tex, texs)
			tex->UpdateTextureReferences(oldTex, newTex);

		// Delete the old texture definition
		delete oldTex;
	} else {
		// Add the new texture
		texs.push_back(newTex);
		texsByName.insert(make_pair(name, newTex));
	}
}

Texture *TextureDefinitions::GetTexture(const string &name) {
	// Check if the texture has been already defined
	boost::unordered_map<string, Texture *>::const_iterator it = texsByName.find(name);

	if (it == texsByName.end())
		throw runtime_error("Reference to an undefined texture: " + name);
	else
		return it->second;
}

vector<string> TextureDefinitions::GetTextureNames() const {
	vector<string> names;
	names.reserve(texs.size());

	for (boost::unordered_map<string, Texture *>::const_iterator it = texsByName.begin(); it != texsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void TextureDefinitions::DeleteTexture(const string &name) {
	const u_int index = GetTextureIndex(name);
	texs.erase(texs.begin() + index);
	texsByName.erase(name);
}

u_int TextureDefinitions::GetTextureIndex(const Texture *t) const {
	for (u_int i = 0; i < texs.size(); ++i) {
		if (t == texs[i])
			return i;
	}

	throw runtime_error("Reference to an undefined texture: " + boost::lexical_cast<string>(t));
}

u_int TextureDefinitions::GetTextureIndex(const string &name) {
	return GetTextureIndex(GetTexture(name));
}
