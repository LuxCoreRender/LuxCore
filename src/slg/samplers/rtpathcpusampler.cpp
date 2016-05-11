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

#include "luxrays/core/color/color.h"
#include "slg/samplers/rtpathcpusampler.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathCPU specific sampler shared data
//------------------------------------------------------------------------------

RTPathCPUSamplerSharedData::RTPathCPUSamplerSharedData() :
		SamplerSharedData() {
	Reset();
}

void RTPathCPUSamplerSharedData::Reset() {
	step = 0;
}

SamplerSharedData *RTPathCPUSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen) {
	return new RTPathCPUSamplerSharedData();
}

//------------------------------------------------------------------------------
// RTPathCPU specific sampler
//------------------------------------------------------------------------------

RTPathCPUSampler::RTPathCPUSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter,
			RTPathCPUSamplerSharedData *samplerSharedData) :
			Sampler(rnd, flm, flmSplatter), sharedData(samplerSharedData) {
	Reset(flm);
}

RTPathCPUSampler::~RTPathCPUSampler() {
}

void RTPathCPUSampler::Reset(Film *flm) {
	film = flm;

	myStep = 0;
	currentX = film->GetWidth() - 1;
	currentY = 0;

	NextPixel();
}

void RTPathCPUSampler::NextPixel() {
	++currentX;
	
	if (currentX == film->GetWidth()) {
		currentX = 0;

		myStep = sharedData->step.fetch_add(1);
		currentY = myStep % film->GetHeight();
	}
}

float RTPathCPUSampler::GetSample(const u_int index) {
	float u;
	switch (index) {
		case 0:
			u = (currentX + rndGen->floatValue()) / (float)film->GetWidth();
			break;
		case 1:
			u = (currentY + rndGen->floatValue()) / (float)film->GetHeight();
			break;
		default:
			u = rndGen->floatValue();
			break;
	}
	
	return u;
}

void RTPathCPUSampler::NextSample(const vector<SampleResult> &sampleResults) {
	// This should be done as atomic operation but it is only for statistics
	film->AddSampleCount(1.0);
	AddSamplesToFilm(sampleResults);

	NextPixel();
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties RTPathCPUSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type"));
}

Sampler *RTPathCPUSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	return new RTPathCPUSampler(rndGen, film, flmSplatter, (RTPathCPUSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *RTPathCPUSampler::FromPropertiesOCL(const Properties &cfg) {
	// This can not happen
	throw runtime_error("Internal error in RTPathCPUSampler::FromPropertiesOCL()");
}

const Properties &RTPathCPUSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag());

	return props;
}
