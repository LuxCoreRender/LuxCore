/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <limits>
#include <algorithm>
#include <exception>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "slg/film/film.h"
#include "slg/film/sampleresult.h"
#include "slg/film/denoiser/filmdenoiser.h"
#include "slg/film/imagepipeline/imagepipeline.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film BCD denoiser
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::FilmDenoiser)

FilmDenoiser::FilmDenoiser() {
	Init();
}

FilmDenoiser::FilmDenoiser(const Film *f) : film(f) {
	Init();
	film = f;
}

void FilmDenoiser::Init() {
	film = NULL;
	samplesAccumulator = NULL;
	radianceChannelScales = NULL;
	sampleScale = 1.f;
	warmUpDone = false;
	referenceFilm = NULL;
	
	enabled = false;
}

FilmDenoiser::~FilmDenoiser() {
	if (!referenceFilm)
		delete samplesAccumulator;
}

void FilmDenoiser::Reset() {
	if (!referenceFilm)
		delete samplesAccumulator;

	samplesAccumulator = NULL;
	radianceChannelScales = NULL;
	sampleScale = 1.f;
	warmUpDone = false;
}

void FilmDenoiser::WarmUpDone() {
	SLG_LOG("BCD denoiser warmup done");

	// Get the current film luminance
	// I use the pipeline of the first BCD plugin
	const u_int denoiserImagePipelineIndex = ImagePipelinePlugin::GetBCDPipelineIndex(*film);
	const float filmY = film->GetFilmY(denoiserImagePipelineIndex);

	radianceChannelScales = &film->GetImagePipeline(denoiserImagePipelineIndex)->radianceChannelScales;

	// Adjust the ray fusion histogram as if I'm using auto-linear tone mapping
	sampleScale = (filmY == 0.f) ? 1.f : (1.25f / filmY * powf(118.f / 255.f, 2.2f));

	// Allocate denoiser samples collector
	samplesAccumulator = new SamplesAccumulator(film->GetWidth(), film->GetHeight(),
			bcd::HistogramParameters());

	// This will trigger the thread using this film as reference
	warmUpDone = true;
}

bcd::SamplesStatisticsImages FilmDenoiser::GetSamplesStatistics() const {
	if (samplesAccumulator)
		return samplesAccumulator->getSamplesStatistics();
	else
		return bcd::SamplesStatisticsImages();
}

void FilmDenoiser::AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	if (!enabled)
		return;

	if (samplesAccumulator) {
		const int line = film->GetHeight() - y - 1;
		const int column = x;

		const Spectrum sample = (sampleResult.GetSpectrum(*radianceChannelScales) * sampleScale).Clamp(
				0.f, samplesAccumulator->GetHistogramParameters().m_maxValue);

		if (!sample.IsNaN() && !sample.IsInf())
			samplesAccumulator->addSampleAtomic(line, column,
					sample.c[0], sample.c[1], sample.c[2],
					weight);
	} else {
		// Check if I have to allocate denoiser statistics collector

		if (referenceFilm) {
			if (referenceFilm->filmDenoiser.warmUpDone) {
				// Look for the BCD image pipeline plugin
				sampleScale = referenceFilm->filmDenoiser.sampleScale;
				radianceChannelScales = referenceFilm->filmDenoiser.radianceChannelScales;
				warmUpDone = true;

				samplesAccumulator = referenceFilm->filmDenoiser.samplesAccumulator;
			}
		} else if (film->GetTotalSampleCount() / (film->GetWidth() * film->GetHeight()) > 2.0) {
			// The warmup period is over and I can allocate denoiserSamplesAccumulator

			WarmUpDone();
		}
	}
}
