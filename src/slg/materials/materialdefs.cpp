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
		BOOST_FOREACH(NamedObject *mat, mats.GetObjs())
			static_cast<Material *>(mat)->UpdateMaterialReferences(oldMat, newMat);

		// Delete the old material definition
		delete oldMat;
	}
}

void MaterialDefinitions::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	BOOST_FOREACH(NamedObject *mat, mats.GetObjs())
		static_cast<Material *>(mat)->UpdateTextureReferences(oldTex, newTex);
}
