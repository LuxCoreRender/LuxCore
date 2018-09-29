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

#include <boost/foreach.hpp>

#include "slg/textures/texturedefs.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

void TextureDefinitions::DefineTexture(Texture *newTex) {
	const Texture *oldTex = static_cast<const Texture *>(texs.DefineObj(newTex));

	if (oldTex) {
		// Update all references
		BOOST_FOREACH(NamedObject *tex, texs.GetObjs())
			static_cast<Texture *>(tex)->UpdateTextureReferences(oldTex, newTex);

		// Delete the old texture definition
		delete oldTex;
	}
}

void TextureDefinitions::GetTextureSortedNames(vector<std::string> &names) const {
	boost::unordered_set<string> doneNames;

	for (u_int i = 0; i < GetSize(); ++i) {
		const Texture *tex = GetTexture(i);
		
		GetTextureSortedNamesImpl(tex, names, doneNames);
	}
}

void TextureDefinitions::GetTextureSortedNamesImpl(const Texture *tex,
		vector<std::string> &names, boost::unordered_set<string> &doneNames) const {
	// Check it has not been already added
	const string &texName = tex->GetName();
	if (doneNames.count(texName) != 0)
		return;

	// Get the list of reference textures by this one
	boost::unordered_set<const Texture *> referencedTexs;
	tex->AddReferencedTextures(referencedTexs);

	// Add all referenced texture names
	for (auto refTex : referencedTexs) {
		// AddReferencedTextures() adds also itself to the list of referenced textures
		if (refTex != tex)
			GetTextureSortedNamesImpl(refTex, names, doneNames);
	}

	// I can now add the name of this texture name
	names.push_back(texName);
	doneNames.insert(texName);
}
