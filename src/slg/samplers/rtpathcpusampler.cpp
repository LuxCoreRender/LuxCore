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
#include "slg/engines/rtpathcpu/rtpathcpu.h"

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

void RTPathCPUSampler::SetRenderEngine(RTPathCPURenderEngine *re) {
	engine = re;
}

void RTPathCPUSampler::Reset(Film *flm) {
	film = flm;

	myStep = sharedData->step.fetch_add(1);
	frameHeight = RoundUp<u_int>(film->GetHeight(), 2);
	currentX = 0;
	currentY = (myStep * 2) % frameHeight;
	linesDone = 0;
	firstFrameDone = false;
}

void RTPathCPUSampler::NextPixel() {
	if (!firstFrameDone) {
		// Render one pixel every 2x2 on the first frame
		currentX += 2;

		if (currentX >= film->GetWidth()) {
			// This should be done as atomic operation but it is only for statistics
			// (adding the effective number of samples rendered, not the pixels count)
			film->AddSampleCount(film->GetWidth() / 2);
			currentX = 0;
			myStep = sharedData->step.fetch_add(1);
			currentY = (myStep * 2) % frameHeight;
			linesDone = 0;

			const bool stillOnFirstFrame = (myStep * 2 < frameHeight);
			if (!stillOnFirstFrame) {
				// Signal the main thread after have finished the rendering
				// of the first frame
				boost::unique_lock<boost::mutex> lock(engine->firstFrameMutex);

				++(engine->firstFrameThreadDoneCount);

				engine->firstFrameCondition.notify_one();
				
				firstFrameDone = true;
			}
		}
	} else {
		// Normal rendering
		++currentX;

		if (currentX == film->GetWidth()) {
			currentX = 0;
			++linesDone;
			++currentY;

			if ((currentY >= film->GetHeight()) || (linesDone == 2)) {
				// This should be done as atomic operation but it is only for statistics
				film->AddSampleCount(film->GetWidth() * linesDone);

				myStep = sharedData->step.fetch_add(1);
				currentY = (myStep * 2) % frameHeight;
				linesDone = 0;
			}
		}
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
	// film->AddSampleCount(1.0) is done in NextPixel()

	const SampleResult *sr = &sampleResults[0];
	
	// AddSamplesToFilm(sampleResults) is replaced by this special section of code to
	// to render 1 sample every 2x2 pixels on the first frame
	if (firstFrameDone)
		film->AddSample(sr->pixelX, sr->pixelY, *sr, 1.f);
	else {
		// A fake weight so the first frame is replaced in a short amount of time
		const float w = .01f;

		const u_int pX = sr->pixelX;
		const u_int pY = sr->pixelY;
		const u_int p1X = pX + 1;
		const u_int p1Y = pY + 1;
		const bool p1XIsInside = (p1X < film->GetWidth());
		const bool p1YIsInside = (p1Y < film->GetHeight());

		film->AddSample(pX, pY, *sr, w);
		if (p1XIsInside) {
			film->AddSample(p1X, pY, *sr, w);
			
			if (p1YIsInside) {
				film->AddSample(pX, p1Y, *sr, w);
				film->AddSample(p1X, p1Y, *sr, w);
			}
		} else {
			if (p1YIsInside)
				film->AddSample(pX, p1Y, *sr, w);
		}
	}

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
