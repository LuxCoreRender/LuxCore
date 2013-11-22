/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/utils/mcdistribution.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/core/sphericalfunction/sphericalfunctionies.h"
#include "slg/core/sphericalfunction/photometricdataies.h"

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

float SampleableSphericalFunction::Pdf(const Vector& w) const {
	const float theta = SphericalTheta(w);
	const float phi = SphericalPhi(w);

	return uvDistrib->Pdf(phi * INV_TWOPI, theta * INV_PI) /
		(2.f * M_PI * M_PI * sinf(theta));
}

float SampleableSphericalFunction::Average() const {
	return average;
}

//------------------------------------------------------------------------------
// CreateSphericalFunction
//------------------------------------------------------------------------------

SphericalFunction *slg::CreateSphericalFunction(ImageMapCache &imgMapCache,
		const string &prefix, const Properties &props) {
	const bool flipZ = props.Get(Property(prefix + ".flipz")(false)).Get<bool>();
	const string imgMapName = props.Get(Property(prefix + ".mapname")("")).Get<string>();
	const string iesName = props.Get(Property(prefix + ".iesname")("")).Get<string>();

	// Create ImageMap distribution
	SphericalFunction *imgMapFunc = NULL;
	if (imgMapName.length() > 0) {
		const ImageMap *imgMap = imgMapCache.GetImageMap(imgMapName, 1.f);
		imgMapFunc = new ImageMapSphericalFunction(imgMap);
	}

	// Create IES distribution
	SphericalFunction *iesFunc = NULL;
	if (iesName.length() > 0) {
		PhotometricDataIES data(iesName.c_str());

		if (data.IsValid())
			iesFunc = new IESSphericalFunction(data, flipZ);
		else
			throw runtime_error("Invalid IES file: " + iesName);
	}

	if (iesFunc && imgMapFunc) {
		CompositeSphericalFunction *compositeFunc = new CompositeSphericalFunction();
		compositeFunc->Add(imgMapFunc);
		compositeFunc->Add(iesFunc);

		return compositeFunc;
	} else if (imgMapFunc)
		return imgMapFunc;
	else if (iesFunc) {
		return iesFunc;
	} else
		return NULL;
}
