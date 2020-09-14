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

#include "luxrays/core/color/color.h"
#include "slg/samplers/rtpathcpusampler.h"
#include "slg/engines/rtpathcpu/rtpathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathCPU specific sampler shared data
//------------------------------------------------------------------------------

RTPathCPUSamplerSharedData::RTPathCPUSamplerSharedData(Film *film) :
		SamplerSharedData() {
	filmSubRegion[0] = 0;
	filmSubRegion[1] = 0;
	filmSubRegion[2] = 0;
	filmSubRegion[3] = 0;
	filmSubRegionWidth =  0;
	filmSubRegionHeight = 0;

	Reset(film);
}

void RTPathCPUSamplerSharedData::Reset(Film *film) {
	engineFilm = film;
	Reset();	
}

void RTPathCPUSamplerSharedData::Reset() {
	const u_int *subRegion = engineFilm->GetSubRegion();

	// Check if something has changed
	if ((filmSubRegion[0] != subRegion[0]) || (filmSubRegion[1] != subRegion[1]) ||
			(filmSubRegion[2] != subRegion[2]) || (filmSubRegion[3] != subRegion[3])) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
		filmSubRegionWidth = subRegion[1] - subRegion[0] + 1;
		filmSubRegionHeight = subRegion[3] - subRegion[2] + 1;
		

		const u_int pixelCount = filmSubRegionWidth * filmSubRegionHeight;
		pixelRenderSequence.resize(pixelCount);

		for (u_int y = 0; y < filmSubRegionHeight; ++y) {
			for (u_int x = 0; x < filmSubRegionWidth; ++x) {
				const u_int index = x + y * filmSubRegionWidth;

				pixelRenderSequence[index].x = x + subRegion[0];
				pixelRenderSequence[index].y = y + subRegion[2];
			}
		}

		// Randomly shuffle elements
		RandomGenerator rnd(123);
		for (u_int i = 0; i < pixelCount; i++) {
			const u_int j = Min(Floor2UInt(rnd.floatValue() * pixelCount), pixelCount - 1);
			Swap(pixelRenderSequence[i], pixelRenderSequence[j]);
		}
	}

	step = 0;
}

SamplerSharedData *RTPathCPUSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new RTPathCPUSamplerSharedData(film);
}

//------------------------------------------------------------------------------
// RTPathCPU specific sampler
//------------------------------------------------------------------------------

RTPathCPUSampler::RTPathCPUSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter,
			RTPathCPUSamplerSharedData *samplerSharedData) :
			Sampler(rnd, flm, flmSplatter, true), sharedData(samplerSharedData) {
	film = flm;
	// Disable denoiser statistics collection
	film->GetDenoiser().SetEnabled(false);

	// NOTE: The sampler can not be used until the call of SetRenderEngine()
}

RTPathCPUSampler::~RTPathCPUSampler() {
}

void RTPathCPUSampler::SetRenderEngine(RTPathCPURenderEngine *re) {
	engine = re;

	Reset(film);
}

void RTPathCPUSampler::Reset(Film *flm) {
	film = flm;
	// Disable denoiser statistics collection
	film->GetDenoiser().SetEnabled(false);

	myStep = sharedData->step.fetch_add(1);
	frameHeight = RoundUp<u_int>(sharedData->filmSubRegionHeight, engine->zoomFactor);
	currentX = 0;
	currentY = (myStep * engine->zoomFactor) % frameHeight;
	linesDone = 0;
	firstFrameDone = false;
}

void RTPathCPUSampler::NextPixel() {
	if (!firstFrameDone) {
		// Render one pixel every engine->zoomFactor x engine->zoomFactor on the first frame
		currentX += engine->zoomFactor;

		if (currentX >= sharedData->filmSubRegionWidth) {
			// This should be done as atomic operation but it is only for statistics
			// (adding the effective number of samples rendered, not the pixels count)
			film->AddSampleCount(threadIndex, sharedData->filmSubRegionWidth / (double)engine->zoomFactor, 0.0);
			currentX = 0;
			myStep = sharedData->step.fetch_add(1);
			currentY = (myStep * engine->zoomFactor) % frameHeight;
			linesDone = 0;

			const bool stillOnFirstFrame = (myStep * engine->zoomFactor < frameHeight);
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

		if (currentX >= sharedData->filmSubRegionWidth) {
			currentX = 0;
			++linesDone;
			++currentY;

			if ((currentY >= sharedData->filmSubRegionHeight) || (linesDone == engine->zoomFactor)) {
				// This should be done as atomic operation but it is only for statistics
				film->AddSampleCount(threadIndex, sharedData->filmSubRegionWidth * linesDone, 0.0);

				myStep = sharedData->step.fetch_add(1);
				currentY = (myStep * engine->zoomFactor) % frameHeight;
				linesDone = 0;
			}
		}
	}
}

float RTPathCPUSampler::GetSample(const u_int index) {
	assert (index < requestedSamples);

	float u;
	switch (index) {
		case 0: {
			const u_int px = firstFrameDone ?
				sharedData->pixelRenderSequence[currentX + currentY * sharedData->filmSubRegionWidth].x :
				(currentX + sharedData->filmSubRegion[0]);
			u = px + rndGen->floatValue();
			break;
		}
		case 1: {
			const u_int py = firstFrameDone ?
				sharedData->pixelRenderSequence[currentX + currentY * sharedData->filmSubRegionWidth].y :
				(currentY + sharedData->filmSubRegion[2]);
			u = py + rndGen->floatValue();
			break;
		}
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
	// to render 1 sample every engine->zoomFactor x engine->zoomFactor pixels on the first frame
	if (firstFrameDone)
		film->AddSample(sr->pixelX, sr->pixelY, *sr, 1.f);
	else {
		// A fake weight so the first frame is replaced in a short amount of time
		const float w = engine->zoomWeight;

		for (u_int py = 0; py < engine->zoomFactor; ++py) {
			for (u_int px = 0; px < engine->zoomFactor; ++px) {
				const u_int x = sr->pixelX + px;
				const u_int y = sr->pixelY + py;

				if ((x >= sharedData->filmSubRegion[0]) &&
						(x <= sharedData->filmSubRegion[1]) &&
						(y >= sharedData->filmSubRegion[2]) &&
						(y <= sharedData->filmSubRegion[3]))
					film->AddSample(x, y, *sr, w);
			}
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

void RTPathCPUSampler::AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg) {
	// No additional channels required
}

const Properties &RTPathCPUSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag());

	return props;
}
