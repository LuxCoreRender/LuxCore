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

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <stdexcept>
#include "luxrays/usings.h"
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
		luxrays::ExtMeshUPtr m;
		ar & m;
		SDL_LOG("Loading serialized mesh: " << m->GetName());
		meshes.DefineObj(std::move(m));
	}

	ar & deleteMeshData;
}

template<class Archive> void ExtMeshCache::save(Archive &ar, const u_int version) const {
	// Save the size
	const u_int size = meshes.GetSize();
	ar & size;

	for (u_int i = 0; i < size; ++i) {
		// The cache owns this mesh. Never let a temporary unique_ptr delete it.
		ExtMesh * const meshPtr = dynamic_cast<ExtMesh *>(meshes.objs[i].get());
		if (!meshPtr)
			throw std::runtime_error("Invalid mesh type in ExtMeshCache::save");

		// Keep serializing as unique_ptr to preserve the existing archive format.
		// This is only a temporary, non-owning alias; release it in all paths.
		ExtMeshUPtr m(meshPtr);
		try {
			SDL_LOG("Saving serialized mesh: " << m->GetName());

			// Save the mesh
			ar & m;
		} catch (...) {
			m.release();
			throw;
		}
		m.release();
	}

	ar & deleteMeshData;
}

namespace slg {
// Explicit instantiations for portable archives
template void ExtMeshCache::save(LuxOutputArchive &ar, const u_int version) const;
template void ExtMeshCache::load(LuxInputArchive &ar, const u_int version);
template void ExtMeshCache::save(LuxOutputArchiveText &ar, const u_int version) const;
template void ExtMeshCache::load(LuxInputArchiveText &ar, const u_int version);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
