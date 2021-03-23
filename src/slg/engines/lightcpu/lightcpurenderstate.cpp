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

#include "luxrays/utils/serializationutils.h"
#include "slg/engines/lightcpu/lightcpurenderstate.h"
#include "slg/engines/lightcpu/lightcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCPURenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::LightCPURenderState)

LightCPURenderState::LightCPURenderState(const u_int seed) :
		RenderState(LightCPURenderEngine::GetObjectTag()),
		bootStrapSeed(seed) {
}

LightCPURenderState::~LightCPURenderState() {
}

template<class Archive> void LightCPURenderState::serialize(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
}

namespace slg {
// Explicit instantiations for portable archives
template void LightCPURenderState::serialize(LuxOutputBinArchive &ar, const u_int version);
template void LightCPURenderState::serialize(LuxInputBinArchive &ar, const u_int version);
template void LightCPURenderState::serialize(LuxOutputTextArchive &ar, const u_int version);
template void LightCPURenderState::serialize(LuxInputTextArchive &ar, const u_int version);
}
