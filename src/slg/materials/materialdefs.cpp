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

#include <boost/lexical_cast.hpp>

#include "slg/materials/materialdefs.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

MaterialDefinitions::MaterialDefinitions() { }

MaterialDefinitions::~MaterialDefinitions() {
	BOOST_FOREACH(const Material *m, mats)
		delete m;
}

void MaterialDefinitions::DefineMaterial(Material *newMat) {
	const std::string &name = newMat->GetName();

	if (IsMaterialDefined(name)) {
		const Material *oldMat = GetMaterial(name);

		// Update name/material definition
		const u_int index = GetMaterialIndex(name);
		mats[index] = newMat;
		matsByName.erase(name);
		matsByName.insert(make_pair(name, newMat));

		// Update all possible references to old material with the new one
		BOOST_FOREACH(Material *mat, mats)
			mat->UpdateMaterialReferences(oldMat, newMat);

		// Delete old material
		delete oldMat;
	} else {
		// Add the new material
		mats.push_back(newMat);
		matsByName.insert(make_pair(name, newMat));
	}
}

void MaterialDefinitions::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	BOOST_FOREACH(Material *mat, mats)
		mat->UpdateTextureReferences(oldTex, newTex);
}

const Material *MaterialDefinitions::GetMaterial(const string &name) const {
	// Check if the material has been already defined
	boost::unordered_map<string, Material *>::const_iterator it = matsByName.find(name);

	if (it == matsByName.end())
		throw runtime_error("Reference to an undefined material: " + name);
	else
		return it->second;
}

u_int MaterialDefinitions::GetMaterialIndex(const string &name) {
	return GetMaterialIndex(GetMaterial(name));
}

u_int MaterialDefinitions::GetMaterialIndex(const Material *m) const {
	for (u_int i = 0; i < mats.size(); ++i) {
		if (m == mats[i])
			return i;
	}

	throw runtime_error("Reference to an undefined material: " + boost::lexical_cast<string>(m));
}

vector<string> MaterialDefinitions::GetMaterialNames() const {
	vector<string> names;
	names.reserve(mats.size());
	for (boost::unordered_map<string, Material *>::const_iterator it = matsByName.begin(); it != matsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void MaterialDefinitions::DeleteMaterial(const string &name) {
	const u_int index = GetMaterialIndex(name);
	mats.erase(mats.begin() + index);
	matsByName.erase(name);
}
