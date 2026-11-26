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

#include <boost/lexical_cast.hpp>

#include "slg/materials/materialdefs.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/usings.h"
#include "slg/materials/material.h"
#include "slg/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

std::tuple<Material&, MaterialUPtr>
MaterialDefinitions::DefineMaterial(MaterialUPtr&& mat) {

	// Add new material to material container
	auto [newMatRef, oldMatPtr] = mats.DefineObj<Material>(std::move(mat));

	if (oldMatPtr) {  // An object was replaced
		auto& oldMatRef = *oldMatPtr;
		// Update all references
		for(NamedObjectRef o: mats.ViewObjs()) {
			// Update all references in material/volume (note: volume is also a material)
			auto& m = dynamic_cast<MaterialRef>(o);
			m.UpdateMaterialReferences(oldMatRef, newMatRef);
		}
	}

	return std::make_tuple(std::ref(newMatRef), std::move(oldMatPtr));
}

void MaterialDefinitions::UpdateTextureReferences(
	TextureConstRef oldTex, TextureRef newTex) {
	for(NamedObjectRef mat: mats.GetObjs())
		dynamic_cast<MaterialRef>(mat).UpdateTextureReferences(oldTex, newTex);
}

void MaterialDefinitions::GetMaterialSortedNames(vector<std::string> &names) const {
	std::unordered_set<string> doneNames;

	for (u_int i = 0; i < GetSize(); ++i) {
		auto& mat = GetMaterial(i);

		GetMaterialSortedNamesImpl(mat, names, doneNames);
	}
}

void MaterialDefinitions::GetMaterialSortedNamesImpl(
	MaterialConstRef mat,
	vector<std::string> &names,
	std::unordered_set<string> &doneNames
) const {
	// Check it has not been already added
	const string &matName = mat.GetName();
	if (doneNames.count(matName) != 0)
		return;

	// Get the list of reference materials by this one
	std::unordered_set<const Material *> referencedTexs;
	mat.AddReferencedMaterials(referencedTexs);

	// Add all referenced texture names
	for (auto& refMat : referencedTexs) {
		// AddReferencedMaterials() adds also itself to the list of referenced materials
		if (not (refMat == &mat))
			GetMaterialSortedNamesImpl(*refMat, names, doneNames);
	}

	// I can now add the name of this texture name
	names.push_back(matName);
	doneNames.insert(matName);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
