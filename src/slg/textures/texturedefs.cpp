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

#include "slg/textures/texturedefs.h"
#include "luxrays/usings.h"
#include "slg/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

std::tuple<Texture&, TextureUPtr>
TextureDefinitions::DefineTexture(TextureUPtr&& tex) {

	auto [newTexRef, oldTexPtr] = texs.DefineObj<Texture>(std::move(tex));

	if (oldTexPtr) {  // This is a replacement
		auto& oldTexRef = *oldTexPtr;
		// Update all references
		for(NamedObjectRef o: texs.GetObjs()) {
			auto& t = dynamic_cast<Texture&>(o);
			t.UpdateTextureReferences(oldTexRef, newTexRef);
		}
	}

	return std::make_tuple(std::ref(newTexRef), std::move(oldTexPtr));
}

const std::vector<std::string>
TextureDefinitions::GetTextureSortedNames() const {

	std::vector<std::string> names;
	std::unordered_set<std::string> doneNames;

	for (u_int i = 0; i < GetSize(); ++i) {
		auto& tex = GetTexture(i);

		GetTextureSortedNamesImpl(tex, names, doneNames);
	}
	return names;
}

void TextureDefinitions::GetTextureSortedNamesImpl(
	TextureConstRef tex,
	std::vector<std::string> &names,  // in and out
	std::unordered_set<std::string> &doneNames  // in and out
) const {
	// Check it has not been already added
	const string &texName = tex.GetName();
	if (doneNames.count(texName) != 0)
		return;

	// Get the list of reference textures by this one
	std::unordered_set<const Texture *> referencedTexs;
	tex.AddReferencedTextures(referencedTexs);

	// Add all referenced texture names
	for (const auto * refTex : referencedTexs) {
		// AddReferencedTextures() adds also itself to the list of referenced textures
		if (*refTex != tex)
			GetTextureSortedNamesImpl(*refTex, names, doneNames);
	}

	// I can now add the name of this texture name
	names.push_back(texName);
	doneNames.insert(texName);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
