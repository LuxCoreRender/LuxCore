/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include "slg/scene/extmeshcache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ExtMeshCache serialization code
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ExtMeshCache)

template<class Archive> void ExtMeshCache::load(Archive &ar, const u_int version) {
	// Load the size
	u_int size;
	ar & size;

	for (u_int i = 0; i < size; ++i) {
		// Load the mesh
		luxrays::ExtMesh *m;
		ar & m;
		SDL_LOG("Loading serialized mesh: " << m->GetName());

		meshes.DefineObj(m);
	}

	ar & deleteMeshData;
}

template<class Archive> void ExtMeshCache::save(Archive &ar, const u_int version) const {
	// Save the size
	const u_int size = meshes.GetSize();
	ar & size;

	for (u_int i = 0; i < size; ++i) {
		const luxrays::ExtMesh *m = static_cast<const luxrays::ExtMesh *>(meshes.GetObj(i));
		SDL_LOG("Saving serialized mesh: " << m->GetName());

		// Save the mesh
		ar & m;
	}

	ar & deleteMeshData;
}

namespace slg {
// Explicit instantiations for portable archives
template void ExtMeshCache::save(LuxOutputBinArchive &ar, const u_int version) const;
template void ExtMeshCache::load(LuxInputBinArchive &ar, const u_int version);
template void ExtMeshCache::save(LuxOutputTextArchive &ar, const u_int version) const;
template void ExtMeshCache::load(LuxInputTextArchive &ar, const u_int version);
}
