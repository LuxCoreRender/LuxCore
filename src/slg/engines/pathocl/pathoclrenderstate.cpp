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

#include "slg/engines/pathocl/pathoclrenderstate.h"
#include "slg/engines/pathocl/pathocl.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLRenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::PathOCLRenderState)

PathOCLRenderState::PathOCLRenderState() :
		RenderState(PathOCLRenderEngine::GetObjectTag()),
		photonGICache(nullptr), deletePhotonGICachePtr(false) {
}

PathOCLRenderState::PathOCLRenderState(const u_int seed, PhotonGICache *pgic) :
		RenderState(PathOCLRenderEngine::GetObjectTag()),
		bootStrapSeed(seed), photonGICache(pgic), deletePhotonGICachePtr(false) {
}

PathOCLRenderState::~PathOCLRenderState() {
	if (deletePhotonGICachePtr)
		delete photonGICache;
}

template<class Archive> void PathOCLRenderState::load(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & photonGICache;

	deletePhotonGICachePtr = true;
}

template<class Archive> void PathOCLRenderState::save(Archive &ar, const u_int version) const {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & photonGICache;
}

namespace slg {
// Explicit instantiations for portable archives
template void PathOCLRenderState::save(LuxOutputBinArchive &ar, const u_int version) const;
template void PathOCLRenderState::load(LuxInputBinArchive &ar, const u_int version);
template void PathOCLRenderState::save(LuxOutputTextArchive &ar, const u_int version) const;
template void PathOCLRenderState::load(LuxInputTextArchive &ar, const u_int version);
}

#endif
