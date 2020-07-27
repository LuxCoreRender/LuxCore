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
		radiance.Resize(radianceGroupCount);
	else
		radiance.Resize(0);

	firstPathVertexEvent = NONE;
	firstPathVertex = true;
	// lastPathVertex can not be really initialized here without knowing
	// the max. path depth.
	lastPathVertex = false;
}

Spectrum SampleResult::GetSpectrum(const vector<RadianceChannelScale> &radianceChannelScales) const {
	Spectrum s = 0.f;
	for (u_int i = 0; i < radiance.Size(); ++i)
		s += radianceChannelScales[i].Scale(radiance[i]);
	
	return s;
}

float SampleResult::GetY(const vector<RadianceChannelScale> &radianceChannelScales) const {
	return GetSpectrum(radianceChannelScales).Y();
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

bool SampleResult::IsValid() const {
	for (u_int i = 0; i < radiance.Size(); ++i)
		if (radiance[i].IsNaN() || radiance[i].IsInf() || radiance[i].IsNeg())
			return false;

	return true;
}

bool SampleResult::IsAllValid(const vector<SampleResult> &sampleResults) {
	for (u_int i = 0; i < sampleResults.size(); ++i)
		if (!sampleResults[i].IsValid())
			return false;
	
	return true;
}
