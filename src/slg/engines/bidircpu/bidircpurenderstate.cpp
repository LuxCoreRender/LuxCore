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

#include <boost/serialization/base_object.hpp>

#include "luxrays/utils/serializationutils.h"
#include "slg/engines/bidircpu/bidircpurenderstate.h"
#include "slg/engines/bidircpu/bidircpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirCPURenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BiDirCPURenderState)

BiDirCPURenderState::BiDirCPURenderState(const u_int seed) :
		RenderState(BiDirCPURenderEngine::GetObjectTag()),
		bootStrapSeed(seed) {
}

BiDirCPURenderState::~BiDirCPURenderState() {
}

template<class Archive> void BiDirCPURenderState::serialize(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
}

namespace slg {
// Explicit instantiations for portable archives
template void BiDirCPURenderState::serialize(LuxOutputArchive &ar, const u_int version);
template void BiDirCPURenderState::serialize(LuxInputArchive &ar, const u_int version);
}
