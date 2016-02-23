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
#include "slg/textures/fresnel/fresnelsopra.h"
#include "slg/textures/fresnel/fresnelconst.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Fresnel Sopra texture
//------------------------------------------------------------------------------

// Converts photon energy (in eV) to wavelength (in nm)
static float eVtolambda(const float eV) {
	// lambda = hc / E, where 
	//  h is planck's constant in eV*s
	//  c is speed of light in m/s
	return (4.13566733e-15f * 299792458.f * 1e9f) / eV;
}

// Converts wavelength (in micrometer) to wavelength (in nm)
static float umtolambda(const float um) {
	return um * 1000.f;
}

// Converts wavelength (in cm-1) to wavelength (in nm)
static float invcmtolambda(const float invcm) {
	return 1e7f / invcm;
}

// Converts wavelength (in nm) to wavelength (in nm)
static float nmtolambda(const float nm) {
	return nm;
}

FresnelTexture *slg::AllocFresnelSopraTex(const Properties &props, const string &propName) {
	const string fileName = props.Get(Property(propName + ".file")("sopra.nk")).Get<string>();

	ifstream fs;
	fs.open(fileName.c_str());
	string line;
	if (!getline(fs, line).good())
		throw runtime_error("Unable to read sopra file: " + fileName);

	boost::smatch m;

	// Read initial line, containing metadata
	boost::regex header_expr("(\\d+)[^\\.[:digit:]]+(\\d*\\.?\\d+(?:[eE]-?\\d+)?)[^\\.[:digit:]]+(\\d*\\.?\\d+(?:[eE]-?\\d+)?)[^\\.[:digit:]]+(\\d+)");

	if (!boost::regex_search(line, m, header_expr))
		throw runtime_error("Bad sopra header in: " + fileName);

	// Used to convert file units to wavelength in nm
	float (*tolambda)(float) = NULL;
	float lambda_first, lambda_last;

	if (m[1] == "1") {
		// lambda in eV
		// low eV -> high lambda
		lambda_first = boost::lexical_cast<float>(m[3]);
		lambda_last = boost::lexical_cast<float>(m[2]);
		tolambda = &eVtolambda;
	} else if (m[1] == "2") {
		// lambda in um
		lambda_first = boost::lexical_cast<float>(m[2]);
		lambda_last = boost::lexical_cast<float>(m[3]);
		tolambda = &umtolambda;
	} else if (m[1] == "3") {
		// lambda in cm-1
		lambda_first = boost::lexical_cast<float>(m[3]);
		lambda_last = boost::lexical_cast<float>(m[2]);
		tolambda = &invcmtolambda;
	} else if (m[1] == "4") {
		// lambda in nm
		lambda_first = boost::lexical_cast<float>(m[2]);
		lambda_last = boost::lexical_cast<float>(m[3]);
		tolambda = &nmtolambda;
	} else
		throw runtime_error("Unknown sopra unit code '" + m[1] + "' in: " + fileName);

	// Number of lines of nk data
	const int count = boost::lexical_cast<int>(m[4]);  

	// Read nk data
	boost::regex sample_expr("(\\d*\\.?\\d+(?:[eE]-?\\d+)?)[^\\.[:digit:]]+(\\d*\\.?\\d+(?:[eE]-?\\d+)?)");

	vector<float> wl(count + 1);
	vector<float> n(count + 1);
	vector<float> k(count + 1);

	// We want lambda to go from low to high
	// so reverse direction
	const float delta = (lambda_last - lambda_first) / count;
	for (int i = count; i >= 0; --i) {
		if (getline(fs, line).bad())
			throw runtime_error("Not enough sopra data in: " + fileName);

		if (!boost::regex_search(line, m, sample_expr))
			throw runtime_error("Unparseable sopra data at data line " + ToString(count - i) + "' in: " + fileName);

		// Linearly interpolate units in file
		// then convert to wavelength in nm
		wl[i] = tolambda(lambda_first + delta * i);
		n[i] = boost::lexical_cast<float>(m[1]);
		k[i] = boost::lexical_cast<float>(m[2]);
	}

	IrregularSPD N(&wl[0], &n[0], wl.size());
	IrregularSPD K(&wl[0], &k[0], wl.size());

	ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f,
		1.f / 3.f, 1.f / 3.f, 1.f);
	const RGBColor Nrgb = colorSpace.ToRGBConstrained(N.ToNormalizedXYZ());
	const RGBColor Krgb = colorSpace.ToRGBConstrained(K.ToNormalizedXYZ());

	return new FresnelConstTexture(Nrgb, Krgb);
}
