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

void MaterialDefinitions::DefineMaterial(Material *newMat) {
	const Material *oldMat = static_cast<const Material *>(mats.DefineObj(newMat));

	if (oldMat) {
		// Update all references
		BOOST_FOREACH(NamedObject *obj, mats.GetObjs()) {
			// Update all references in material/volume (note: volume is also a material)
			static_cast<Material *>(obj)->UpdateMaterialReferences(oldMat, newMat);
		}

		// Delete the old material definition
		delete oldMat;
	}
}

void MaterialDefinitions::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	BOOST_FOREACH(NamedObject *mat, mats.GetObjs())
		static_cast<Material *>(mat)->UpdateTextureReferences(oldTex, newTex);
}

void MaterialDefinitions::GetMaterialSortedNames(vector<std::string> &names) const {
	boost::unordered_set<string> doneNames;

	for (u_int i = 0; i < GetSize(); ++i) {
		const Material *mat = GetMaterial(i);
		
		GetMaterialSortedNamesImpl(mat, names, doneNames);
	}
}

void MaterialDefinitions::GetMaterialSortedNamesImpl(const Material *mat,
		vector<std::string> &names, boost::unordered_set<string> &doneNames) const {
	// Check it has not been already added
	const string &matName = mat->GetName();
	if (doneNames.count(matName) != 0)
		return;

	// Get the list of reference materials by this one
	boost::unordered_set<const Material *> referencedTexs;
	mat->AddReferencedMaterials(referencedTexs);

	// Add all referenced texture names
	for (auto refMat : referencedTexs) {
		// AddReferencedMaterials() adds also itself to the list of referenced materials
		if (refMat != mat)
			GetMaterialSortedNamesImpl(refMat, names, doneNames);
	}

	// I can now add the name of this texture name
	names.push_back(matName);
	doneNames.insert(matName);
}
