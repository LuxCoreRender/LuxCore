/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "slg/film/film.h"
#include "slg/film/sampleresult.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

void SampleResult::Init(const u_int channelTypes, const u_int radianceGroupCount) {
	channels = channelTypes;

	if ((channels & Film::RADIANCE_PER_PIXEL_NORMALIZED) && (channels & Film::RADIANCE_PER_SCREEN_NORMALIZED))
		throw runtime_error("RADIANCE_PER_PIXEL_NORMALIZED and RADIANCE_PER_SCREEN_NORMALIZED, both used in SampleResult");
	else if ((channels & Film::RADIANCE_PER_PIXEL_NORMALIZED) || (channels & Film::RADIANCE_PER_SCREEN_NORMALIZED))
		radiance.resize(radianceGroupCount);
	else
		radiance.resize(0);

	firstPathVertexEvent = NONE;
	firstPathVertex = true;
	// lastPathVertex can not be really initialized here without knowing
	// the max. path depth.
	lastPathVertex = false;
	passThroughPath = true;
}

float SampleResult::Y() const {
	float luminance = 0.f;
	for (u_int i = 0; i < radiance.size(); ++i)
		luminance += radiance[i].Y();
	
	return luminance;
}

void SampleResult::AddEmission(const u_int lightID, const Spectrum &pathThroughput,
		const Spectrum &incomingRadiance) {
	const Spectrum r = pathThroughput * incomingRadiance;
	radiance[lightID] += r;

	if (firstPathVertex)
		emission += r;
	else {
		indirectShadowMask = 0.f;

		if (firstPathVertexEvent & DIFFUSE)
			indirectDiffuse += r;
		else if (firstPathVertexEvent & GLOSSY)
			indirectGlossy += r;
		else if (firstPathVertexEvent & SPECULAR)
			indirectSpecular += r;
	}
}

void SampleResult::AddDirectLight(const u_int lightID, const BSDFEvent bsdfEvent,
		const Spectrum &pathThroughput, const Spectrum &incomingRadiance, const float lightScale) {
	const Spectrum r = pathThroughput * incomingRadiance;
	radiance[lightID] += r;

	if (firstPathVertex) {
		// directShadowMask is supposed to be initialized to 1.0
		directShadowMask = Max(0.f, directShadowMask - lightScale);

		if (bsdfEvent & DIFFUSE)
			directDiffuse += r;
		else
			directGlossy += r;
	} else {
		// indirectShadowMask is supposed to be initialized to 1.0
		indirectShadowMask = Max(0.f, indirectShadowMask - lightScale);

		if (firstPathVertexEvent & DIFFUSE)
			indirectDiffuse += r;
		else if (firstPathVertexEvent & GLOSSY)
			indirectGlossy += r;
		else if (firstPathVertexEvent & SPECULAR)
			indirectSpecular += r;

		irradiance += irradiancePathThroughput * incomingRadiance;
	}
}

// Used by render engines not supporting AOVs
void SampleResult::AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const Spectrum &radiancePPN,
	const float alpha) {
	assert(!radiancePPN.IsInf() || !radiancePPN.IsNaN());
	assert(!isinf(alpha) || !isnan(alpha));

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	sampleResults[size].Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA, 1);
	sampleResults[size].filmX = filmX;
	sampleResults[size].filmY = filmY;
	sampleResults[size].radiance[0] = radiancePPN;
	sampleResults[size].alpha = alpha;
}

// Used by render engines not supporting AOVs
void SampleResult::AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const Spectrum &radiancePSN) {
	assert(!radiancePSN.IsInf() || !radiancePSN.IsNaN());

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	sampleResults[size].Init(Film::RADIANCE_PER_SCREEN_NORMALIZED, 1);
	sampleResults[size].filmX = filmX;
	sampleResults[size].filmY = filmY;
	sampleResults[size].radiance[0] = radiancePSN;
}

void SampleResult::ClampRadiance(const float minRadiance, const float maxRadiance) {
	for (u_int i = 0; i < radiance.size(); ++i)
		radiance[i] = radiance[i].ScaledClamp(minRadiance, maxRadiance);
}
