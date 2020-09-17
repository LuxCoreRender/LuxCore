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

#include "slg/film/imagepipeline/radiancechannelscale.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RadianceChannelScale serialization
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::RadianceChannelScale)

template<class Archive> void RadianceChannelScale::save(Archive &ar, const u_int version) const {
	ar & globalScale;
	ar & temperature;
	ar & rgbScale;
	ar & reverse;
	ar & normalize;
	ar & enabled;
}

template<class Archive> void RadianceChannelScale::load(Archive &ar, const u_int version) {
	ar & globalScale;
	ar & temperature;
	ar & rgbScale;
	ar & reverse;
	ar & normalize;
	ar & enabled;

	Init();
}

namespace slg {
// Explicit instantiations for portable archives
template void RadianceChannelScale::save(LuxOutputArchive &ar, const u_int version) const;
template void RadianceChannelScale::load(LuxInputArchive &ar, const u_int version);
}
