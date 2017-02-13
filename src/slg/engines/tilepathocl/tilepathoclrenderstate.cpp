/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "slg/engines/tilerepository.h"
#include "slg/engines/tilepathocl/tilepathoclrenderstate.h"
#include "slg/engines/tilepathocl/tilepathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathOCLRenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::TilePathOCLRenderState)

TilePathOCLRenderState::TilePathOCLRenderState(const u_int seed, TileRepository *tileRepo) :
		RenderState(TilePathOCLRenderEngine::GetObjectTag()),
		bootStrapSeed(seed),
		tileRepository(tileRepo) {
}

TilePathOCLRenderState::~TilePathOCLRenderState() {
}

template<class Archive> void TilePathOCLRenderState::serialize(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
	ar & tileRepository;
}
