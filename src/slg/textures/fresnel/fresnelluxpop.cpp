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

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

#include "luxrays/core/color/spds/irregular.h"
#include "slg/textures/fresnel/fresnelluxpop.h"
#include "slg/textures/fresnel/fresnelconst.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Fresnel LuxPop texture
//------------------------------------------------------------------------------

FresnelTexture *slg::AllocFresnelLuxPopTex(const Properties &props, const string &propName) {
	const string fileName = props.Get(Property(propName + ".file")("luxpop.nk")).Get<string>();

	ifstream fs;
	fs.open(fileName.c_str());
	vector<float> wl;
	vector<float> n;
	vector<float> k;

	string line;

	boost::smatch m;

	// Read nk data
	boost::regex sample_expr("(\\d*\\.?\\d+(?:[eE]-?\\d+)?)\\s+(\\d*\\.?\\d+(?:[eE]-?\\d+)?)\\s+(\\d*\\.?\\d+(?:[eE]-?\\d+)?)");

	// We want lambda to go from low to high
	while (!getline(fs, line).eof()) {
		if (fs.bad())
			break;

		// Skip comments
		if (!line.empty() && line[0] == ';')
			continue;

		if (!boost::regex_search(line, m, sample_expr))
			throw runtime_error("Unparseable luxpop data in: " + fileName);

		// Wavelength in data file is in Angstroms, we want nm
		wl.push_back(boost::lexical_cast<float>(m[1]) * .1f);
		n.push_back(boost::lexical_cast<float>(m[2]));
		k.push_back(boost::lexical_cast<float>(m[3]));
	}

	if (!fs.eof())
		throw runtime_error("Junk in luxpop file: " + fileName);

	IrregularSPD N(&wl[0], &n[0], wl.size());
	IrregularSPD K(&wl[0], &k[0], wl.size());

	ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f,
		1.f / 3.f, 1.f / 3.f, 1.f);
	const RGBColor Nrgb = colorSpace.ToRGBConstrained(N.ToNormalizedXYZ());
	const RGBColor Krgb = colorSpace.ToRGBConstrained(K.ToNormalizedXYZ());

	return new FresnelConstTexture(Nrgb, Krgb);
}
