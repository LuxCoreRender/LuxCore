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

#include "slg/film/film.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmSamplesCounts
//------------------------------------------------------------------------------

FilmSamplesCounts::FilmSamplesCounts() {
	Init(1);
}

FilmSamplesCounts::~FilmSamplesCounts() {
}

void FilmSamplesCounts::Init(const u_int count) {
	assert (count > 0);

	threadCount = count;

	total_SampleCount.resize(threadCount);
	RADIANCE_PER_PIXEL_NORMALIZED_SampleCount.resize(threadCount);
	RADIANCE_PER_SCREEN_NORMALIZED_SampleCount.resize(threadCount);

	fill(total_SampleCount.begin(), total_SampleCount.end(), 0.0);
	fill(RADIANCE_PER_PIXEL_NORMALIZED_SampleCount.begin(), RADIANCE_PER_PIXEL_NORMALIZED_SampleCount.end(), 0.0);
	fill(RADIANCE_PER_SCREEN_NORMALIZED_SampleCount.begin(), RADIANCE_PER_SCREEN_NORMALIZED_SampleCount.end(), 0.0);
}

void FilmSamplesCounts::Clear() {
	fill(total_SampleCount.begin(), total_SampleCount.end(), 0.0);
	fill(RADIANCE_PER_PIXEL_NORMALIZED_SampleCount.begin(), RADIANCE_PER_PIXEL_NORMALIZED_SampleCount.end(), 0.0);
	fill(RADIANCE_PER_SCREEN_NORMALIZED_SampleCount.begin(), RADIANCE_PER_SCREEN_NORMALIZED_SampleCount.end(), 0.0);
}

void FilmSamplesCounts::SetSampleCount(const double sampleCount,
		const double RADIANCE_PER_PIXEL_NORMALIZED_count,
		const double RADIANCE_PER_SCREEN_NORMALIZED_count) {
	total_SampleCount[0] = sampleCount;
	RADIANCE_PER_PIXEL_NORMALIZED_SampleCount[0] = RADIANCE_PER_PIXEL_NORMALIZED_count;
	RADIANCE_PER_SCREEN_NORMALIZED_SampleCount[0] = RADIANCE_PER_SCREEN_NORMALIZED_count;

	for (u_int i = 1; i < threadCount; ++i) {
		total_SampleCount[0] = 0.0;
		RADIANCE_PER_PIXEL_NORMALIZED_SampleCount[i] = 0.0;
		RADIANCE_PER_SCREEN_NORMALIZED_SampleCount[i] = 0.0;
	}
}

void FilmSamplesCounts::AddSampleCount(const double sampleCount,
		const double RADIANCE_PER_PIXEL_NORMALIZED_count,
		const double RADIANCE_PER_SCREEN_NORMALIZED_count) {
	total_SampleCount[0] += sampleCount;
	RADIANCE_PER_PIXEL_NORMALIZED_SampleCount[0] += RADIANCE_PER_PIXEL_NORMALIZED_count;
	RADIANCE_PER_SCREEN_NORMALIZED_SampleCount[0] += RADIANCE_PER_SCREEN_NORMALIZED_count;
}

void FilmSamplesCounts::AddSampleCount(const u_int threadIndex,
		const double RADIANCE_PER_PIXEL_NORMALIZED_count,
		const double RADIANCE_PER_SCREEN_NORMALIZED_count) {
	assert (threadIndex < threadCount);

	total_SampleCount[threadIndex] += Max(RADIANCE_PER_PIXEL_NORMALIZED_count, RADIANCE_PER_SCREEN_NORMALIZED_count);
	RADIANCE_PER_PIXEL_NORMALIZED_SampleCount[threadIndex] += RADIANCE_PER_PIXEL_NORMALIZED_count;
	RADIANCE_PER_SCREEN_NORMALIZED_SampleCount[threadIndex] += RADIANCE_PER_SCREEN_NORMALIZED_count;
}

double FilmSamplesCounts::GetSampleCount() const {
	double result = 0.0;
	for (u_int i = 0; i < threadCount; ++i)
		result += total_SampleCount[i];
	return result;
}

double FilmSamplesCounts::GetSampleCount_RADIANCE_PER_PIXEL_NORMALIZED() const {
	double result = 0.0;
	for (u_int i = 0; i < threadCount; ++i)
		result += RADIANCE_PER_PIXEL_NORMALIZED_SampleCount[i];
	return result;	
}

double FilmSamplesCounts::GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED() const {
	double result = 0.0;
	for (u_int i = 0; i < threadCount; ++i)
		result += RADIANCE_PER_SCREEN_NORMALIZED_SampleCount[i];
	return result;	
}
