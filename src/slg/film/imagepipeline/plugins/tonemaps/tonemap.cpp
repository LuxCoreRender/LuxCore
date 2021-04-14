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

#include <boost/lexical_cast.hpp>

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/plugins/tonemaps/tonemap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Tone mapping
//------------------------------------------------------------------------------

string slg::ToneMapType2String(const ToneMapType type) {
	switch (type) {		
		case TONEMAP_LINEAR:
			return "LINEAR";
		case TONEMAP_REINHARD02:
			return "REINHARD02";
		case TONEMAP_AUTOLINEAR:
			return "AUTOLINEAR";
		case TONEMAP_LUXLINEAR:
			return "LUXLINEAR";
		case TONEMAP_OPENCOLORIO:
			return "OPENCOLORIO";
		default:
			throw runtime_error("Unknown tone mapping type: " + ToString(type));
	}
}

ToneMapType slg::String2ToneMapType(const std::string &type) {
	if ((type.compare("0") == 0) || (type.compare("LINEAR") == 0))
		return TONEMAP_LINEAR;
	else if ((type.compare("1") == 0) || (type.compare("REINHARD02") == 0))
		return TONEMAP_REINHARD02;
	else if ((type.compare("2") == 0) || (type.compare("AUTOLINEAR") == 0))
		return TONEMAP_AUTOLINEAR;
	else if ((type.compare("3") == 0) || (type.compare("LUXLINEAR") == 0))
		return TONEMAP_LUXLINEAR;
	else if ((type.compare("4") == 0) || (type.compare("OPENCOLORIO") == 0))
		return TONEMAP_LUXLINEAR;
	else
		throw runtime_error("Unknown tone mapping type: " + type);
}
