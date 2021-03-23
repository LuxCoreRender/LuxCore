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

#include "slg/engines/bakecpu/bakecpurenderstate.h"
#include "slg/engines/bakecpu/bakecpu.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BakeCPURenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BakeCPURenderState)

BakeCPURenderState::BakeCPURenderState() :
		RenderState(BakeCPURenderEngine::GetObjectTag()),
		photonGICache(nullptr), deletePhotonGICachePtr(false) {
}

BakeCPURenderState::BakeCPURenderState(const u_int seed, PhotonGICache *pgic) :
		RenderState(BakeCPURenderEngine::GetObjectTag()),
		bootStrapSeed(seed), photonGICache(pgic), deletePhotonGICachePtr(false) {
}

BakeCPURenderState::~BakeCPURenderState() {
	if (deletePhotonGICachePtr)
		delete photonGICache;
}

template<class Archive> void BakeCPURenderState::load(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & photonGICache;

	deletePhotonGICachePtr = true;
}

template<class Archive> void BakeCPURenderState::save(Archive &ar, const u_int version) const {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & photonGICache;
}

namespace slg {
// Explicit instantiations for portable archives
template void BakeCPURenderState::save(LuxOutputBinArchive &ar, const u_int version) const;
template void BakeCPURenderState::load(LuxInputBinArchive &ar, const u_int version);
template void BakeCPURenderState::save(LuxOutputTextArchive &ar, const u_int version) const;
template void BakeCPURenderState::load(LuxInputTextArchive &ar, const u_int version);
}
