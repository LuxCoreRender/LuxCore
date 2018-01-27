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

#include "luxrays/utils/serializationutils.h"
#include "slg/engines/pathcpu/pathcpurenderstate.h"
#include "slg/engines/pathcpu/pathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathCPURenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::PathCPURenderState)

PathCPURenderState::PathCPURenderState(const u_int seed) :
		RenderState(PathCPURenderEngine::GetObjectTag()),
		bootStrapSeed(seed) {
}

PathCPURenderState::~PathCPURenderState() {
}

template<class Archive> void PathCPURenderState::serialize(Archive &ar, const u_int version) {
	ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(RenderState);
	ar & bootStrapSeed;
}

namespace slg {
// Explicit instantiations for portable archives
template void PathCPURenderState::serialize(LuxOutputArchive &ar, const u_int version);
template void PathCPURenderState::serialize(LuxInputArchive &ar, const u_int version);
// Explicit instantiations for polymorphic archives
template void PathCPURenderState::serialize(boost::archive::polymorphic_oarchive &ar, const u_int version);
template void PathCPURenderState::serialize(boost::archive::polymorphic_iarchive &ar, const u_int version);
}
