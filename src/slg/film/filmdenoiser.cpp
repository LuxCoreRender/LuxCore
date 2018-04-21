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

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film BCD denoiser
//------------------------------------------------------------------------------

void Film::InitDenoiser() {
	denoiserSamplesAccumulator = NULL;
	denoiserSampleScale = 1.f;
	denoiserWarmUpDone = false;
	denoiserReferenceFilm = NULL;
}

void Film::AllocDenoiserSamplesAccumulator() {
	// Get the current film luminance
	// TODO: fix imagePipelineIndex
	const u_int imagePipelineIndex = 0;
	const float filmY = GetFilmY(imagePipelineIndex);

	// Adjust the ray fusion histogram as if I'm using auto-linear tone mapping
	denoiserSampleScale = (filmY == 0.f) ? 1.f : (1.25f / filmY * powf(118.f / 255.f, 2.2f));

	// Allocate denoiser samples collector parameters
	bcd::HistogramParameters *params = new bcd::HistogramParameters();

	denoiserSamplesAccumulator = new bcd::SamplesAccumulator(width, height,
			*params);

	// This will trigger the thread using this film as reference
	denoiserWarmUpDone = true;
}

bcd::SamplesStatisticsImages Film::GetDenoiserSamplesStatistics() const {
	if (denoiserSamplesAccumulator)
		return denoiserSamplesAccumulator->getSamplesStatistics();
	else
		return bcd::SamplesStatisticsImages();
}
