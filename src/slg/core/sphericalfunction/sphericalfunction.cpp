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

#include "luxrays/utils/mcdistribution.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapSphericalFunction
//------------------------------------------------------------------------------

ImageMapSphericalFunction::ImageMapSphericalFunction() {
	imgMap = NULL;
}

ImageMapSphericalFunction::ImageMapSphericalFunction(const ImageMap *map) {
	SetImageMap(map);
}

void ImageMapSphericalFunction::SetImageMap(const ImageMap *map) {
	imgMap = map;
}

Spectrum ImageMapSphericalFunction::Evaluate(const float phi, const float theta) const {
	return imgMap->GetSpectrum(UV(phi * INV_TWOPI, theta * INV_PI));
}

//------------------------------------------------------------------------------
// SampleableSphericalFunction
//------------------------------------------------------------------------------

SampleableSphericalFunction::SampleableSphericalFunction(const SphericalFunction *aFunc,
		const u_int xRes, const u_int yRes) : func(aFunc) {
	// Compute scalar-valued image
	float *img = new float[xRes * yRes];
	average = 0.f;
	float normalize = 0.f;
	for (u_int y = 0; y < yRes; ++y) {
		const float yp = M_PI * (y + .5f) / yRes;
		const float weight = sinf(yp);
		normalize += xRes * weight;
		for (u_int x = 0; x < xRes; ++x) {
			const float xp = 2.f * M_PI * (x + .5f) / xRes;
			const float value = func->Evaluate(xp, yp).Filter() * weight;
			average += value;
			img[x + y * xRes] = value;
		}
	}
	average *= 4.f * M_PI / normalize;

	// Initialize sampling PDFs
	uvDistrib = new Distribution2D(img, xRes, yRes);
	delete[] img;
}

SampleableSphericalFunction::~SampleableSphericalFunction() {
	delete uvDistrib;
	delete func;
}

Spectrum SampleableSphericalFunction::Evaluate(const float phi, const float theta) const {
	return func->Evaluate(phi, theta);
}

Spectrum SampleableSphericalFunction::Sample(
	const float u1, const float u2, Vector *w, float *pdf) const {
	// Find floating-point $(u,v)$ sample coordinates
	float uv[2];
	uvDistrib->SampleContinuous(u1, u2, uv, pdf);
	if (*pdf == 0.f)
		return Spectrum();

	// Convert sample point to direction on the unit sphere
	const float theta = uv[1] * M_PI;
	const float phi = uv[0] * 2.f * M_PI;
	const float costheta = cosf(theta), sintheta = sinf(theta);
	*w = SphericalDirection(sintheta, costheta, phi);

	// Compute PDF for sampled direction
	*pdf /= 2.f * M_PI * M_PI * sintheta;

	// Return value for direction
	return Evaluate(phi, theta) / *pdf;
}

float SampleableSphericalFunction::Pdf(const Vector &w) const {
	const float theta = SphericalTheta(w);
	const float phi = SphericalPhi(w);

	return uvDistrib->Pdf(phi * INV_TWOPI, theta * INV_PI) /
		(2.f * M_PI * M_PI * sinf(theta));
}

float SampleableSphericalFunction::Average() const {
	return average;
}

//------------------------------------------------------------------------------
// IESSphericalFunction
//------------------------------------------------------------------------------

IESSphericalFunction::IESSphericalFunction(const PhotometricDataIES &data, const bool flipZ,
		const u_int xRes, const u_int yRes) {
	SetImageMap(IES2ImageMap(data, flipZ));
}

IESSphericalFunction::~IESSphericalFunction() {
	delete imgMap;
}

ImageMap *IESSphericalFunction::IES2ImageMap(const PhotometricDataIES &data, const bool flipZ,
			const u_int xRes, const u_int yRes) {
	// This should be a warning but I have no way to emit that kind of information here
	if (data.m_PhotometricType != PhotometricDataIES::PHOTOMETRIC_TYPE_C)
		throw runtime_error("Unsupported photometric type IES file: " + ToString(data.m_PhotometricType));

	vector<double> vertAngles = data.m_VerticalAngles;
	vector<double> horizAngles = data.m_HorizontalAngles;
	vector<vector<double> > values = data.m_CandelaValues;

	// Add a begin/end vertical angle with 0 emission if necessary
	if (vertAngles[0] < 0.) {
		for (u_int i = 0; i < vertAngles.size(); ++i)
			vertAngles[i] = vertAngles[i] + 90.;
	}
	if (vertAngles[0] > 0.) {
		vertAngles.insert(vertAngles.begin(), max(0., vertAngles[0] - 1e-3));
		for (u_int i = 0; i < horizAngles.size(); ++i)
			values[i].insert(values[i].begin(), 0.);
		if (vertAngles[0] > 0.) {
			vertAngles.insert(vertAngles.begin(), 0.);
			for (u_int i = 0; i < horizAngles.size(); ++i)
				values[i].insert(values[i].begin(), 0.);
		}
	}
	if (vertAngles.back() < 180.) {
		vertAngles.push_back(min(180., vertAngles.back() + 1e-3));
		for (u_int i = 0; i < horizAngles.size(); ++i)
			values[i].push_back(0.);
		if (vertAngles.back() < 180.) {
			vertAngles.push_back(180.);
			for (u_int i = 0; i < horizAngles.size(); ++i)
				values[i].push_back(0.);
		}
	}
	// Generate missing horizontal angles
	if (horizAngles[0] == 90. || horizAngles[0] == -90.) {
		const double offset = horizAngles[0];
		for (u_int i = 0; i < horizAngles.size(); ++i)
			horizAngles[i] -= offset;
	}
	if (horizAngles[0] == 0.) {
		if (horizAngles.size() == 1) {
			horizAngles.push_back(90.);
			vector<double> tmpVals = values[0];
			values.push_back(tmpVals);
		}
		if (horizAngles.back() == 90.) {
			for (int i = horizAngles.size() - 2; i >= 0; --i) {
				horizAngles.push_back(180. - horizAngles[i]);
				vector<double> tmpVals = values[i]; // copy before adding!
				values.push_back(tmpVals);
			}
		}
		if (horizAngles.back() == 180.) {
			for (int i = horizAngles.size() - 2; i >= 0; --i) {
				horizAngles.push_back(360. - horizAngles[i]);
				vector<double> tmpVals = values[i]; // copy before adding!
				values.push_back(tmpVals);
			}
		}
		if (horizAngles.back() != 360.) {
 			if ((360. - horizAngles.back()) !=
				(horizAngles.back() - horizAngles[horizAngles.size() - 2])) {
				throw runtime_error("Unsupported horizontal angles in IES file: " + ToString(horizAngles.back()));
			}
			horizAngles.push_back(360.);
			vector<double> tmpVals = values[0];
			values.push_back(tmpVals);
		}
	} else
		throw runtime_error("Invalid horizontal angles in IES file: " + ToString(horizAngles[0]));

	// Initialize irregular functions
	float valueScale = data.m_CandelaMultiplier * 
		   data.BallastFactor * 
		   data.BallastLampPhotometricFactor;
	u_int nVFuncs = horizAngles.size();
	IrregularFunction1D **vFuncs = new IrregularFunction1D*[nVFuncs];
	u_int vFuncLength = vertAngles.size();
	float *vFuncX = new float[vFuncLength];
	float *vFuncY = new float[vFuncLength];
	float *uFuncX = new float[nVFuncs];
	float *uFuncY = new float[nVFuncs];
	for (u_int i = 0; i < nVFuncs; ++i) {
		for (u_int j = 0; j < vFuncLength; ++j) {
			vFuncX[j] = Clamp(Radians(vertAngles[j]) * INV_PI, 0.f, 1.f);
			vFuncY[j] = values[i][j] * valueScale;
		}

		vFuncs[i] = new IrregularFunction1D(vFuncX, vFuncY, vFuncLength);

		uFuncX[i] = Clamp(Radians(horizAngles[i] ) * INV_TWOPI, 0.f, 1.f);
		uFuncY[i] = i;
	}
	delete[] vFuncX;
	delete[] vFuncY;

	IrregularFunction1D *uFunc = new IrregularFunction1D(uFuncX, uFuncY, nVFuncs);
	delete[] uFuncX;
	delete[] uFuncY;

	// Resample the irregular functions
	ImageMap *imgMap = ImageMap::AllocImageMap(1, xRes, yRes, ImageMapConfig());
	float *img = (float *)imgMap->GetStorage()->GetPixelsData();

	for (u_int y = 0; y < yRes; ++y) {
		const float t = (y + .5f) / yRes;
		const u_int tgtY = flipZ ? (yRes - 1) - y : y;

		for (u_int x = 0; x < xRes; ++x) {
			const float s = (x + .5f) / xRes;
			const float u = uFunc->Eval(s);
			const u_int u1 = Floor2UInt(u);
			const u_int u2 = min(nVFuncs - 1, u1 + 1);
			const float du = u - u1;
			
			const float value = Lerp(du, vFuncs[u1]->Eval(t), vFuncs[u2]->Eval(t));
			assert(!isnan(value) && !isinf(value));
			img[x + tgtY * xRes] = value;			
		}
	}
	delete uFunc;
	for (u_int i = 0; i < nVFuncs; ++i)
		delete vFuncs[i];
	delete[] vFuncs;

	imgMap->Preprocess();
	return imgMap;
}
