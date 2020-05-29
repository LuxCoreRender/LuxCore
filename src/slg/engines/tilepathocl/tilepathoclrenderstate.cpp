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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/tilerepository.h"
#include "slg/engines/tilepathocl/tilepathoclrenderstate.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathOCLRenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::TilePathOCLRenderState)

TilePathOCLRenderState::TilePathOCLRenderState() :
		RenderState(TilePathOCLRenderEngine::GetObjectTag()),
		tileRepository(nullptr), photonGICache(nullptr),
		deleteTileRepositoryPtr(false), deletePhotonGICachePtr(false) {
}

TilePathOCLRenderState::TilePathOCLRenderState(const u_int seed,
			TileRepository *tileRepo,
			PhotonGICache *pgic) :
		RenderState(TilePathOCLRenderEngine::GetObjectTag()),
		bootStrapSeed(seed),
		tileRepository(tileRepo),
		photonGICache(pgic),
		deleteTileRepositoryPtr(false),
		deletePhotonGICachePtr(false) {
}

TilePathOCLRenderState::~TilePathOCLRenderState() {
	if (deleteTileRepositoryPtr)
		delete tileRepository;
	if (deletePhotonGICachePtr)
		delete photonGICache;
}

template<class Archive> void TilePathOCLRenderState::load(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & tileRepository;
	ar & photonGICache;

	deletePhotonGICachePtr = true;
	deletePhotonGICachePtr = true;
}

template<class Archive> void TilePathOCLRenderState::save(Archive &ar, const u_int version) const {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & tileRepository;
	ar & photonGICache;
}

namespace slg {
// Explicit instantiations for portable archives
template void TilePathOCLRenderState::save(LuxOutputArchive &ar, const u_int version) const;
template void TilePathOCLRenderState::load(LuxInputArchive &ar, const u_int version);
}

#endif
