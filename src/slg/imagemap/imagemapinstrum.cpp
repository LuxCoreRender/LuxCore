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

#include "luxrays/utils/atomic.h"
#include "slg/imagemap/imagemap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMap instrumentation
//------------------------------------------------------------------------------

void ImageMap::SetUpInstrumentation(const u_int originalWidth, const u_int originalHeigth,
		const ImageMapConfig &imgCfg) {
	instrumentationInfo = new InstrumentationInfo(originalWidth, originalHeigth, imgCfg);
}

void ImageMap::EnableInstrumentation() {
	instrumentationInfo->enabled = true;
}

void ImageMap::DisableInstrumentation() {
	instrumentationInfo->enabled = false;
}

void ImageMap::DeleteInstrumentation() {
	delete instrumentationInfo;
	instrumentationInfo = nullptr;
}

//------------------------------------------------------------------------------
// ImageMap InstrumentationInfo
//------------------------------------------------------------------------------

ImageMap::InstrumentationInfo::InstrumentationInfo(const u_int w, const u_int h,
		const ImageMapConfig &imgCfg) : originalWidth(w), originalHeigth(h),
		originalImgCfg(imgCfg),
		optimalWidth(0), optimalHeigth(0),
		enabled(false) {
}

ImageMap::InstrumentationInfo::~InstrumentationInfo() {
	for (auto k : threadInfo)
		delete k.second;
}

void ImageMap::InstrumentationInfo::ThreadSetUp() {
//	cout << "ImageMap::InstrumentationInfo::ThreadSetUp() [" << this << "]" << endl;

	boost::unique_lock<boost::mutex> lock(classLock);
	threadInfo[boost::this_thread::get_id()] = new ThreadData();
}

void ImageMap::InstrumentationInfo::ThreadFinalize() {
//	cout << "ImageMap::InstrumentationInfo::ThreadFinalize() [" << this << "]" << endl;

	 ThreadData *ti = threadInfo[boost::this_thread::get_id()];
	if (ti->samplesCount > 0) {
//		cout << "Max. U distance in pixel: " << (ti->maxDistance.u * originalWidth) << endl;
//		cout << "Max. V distance in pixel: " << (ti->maxDistance.v * originalHeigth) << endl;

		// Max distance U/V should match one pixel
		const u_int w = (ti->maxDistance.u > 0.f) ?
			(u_int)(originalWidth / (ti->maxDistance.u * originalWidth)) : 0;
		const u_int h = (ti->maxDistance.v > 0.f) ?
			(u_int)(originalHeigth / (ti->maxDistance.v * originalHeigth)) : 0;

		AtomicMax(&optimalWidth, w);
		AtomicMax(&optimalHeigth, h);
	}

	boost::unique_lock<boost::mutex> lock(classLock);
	delete ti;
	threadInfo.erase(boost::this_thread::get_id());
}

void ImageMap::InstrumentationInfo::ThreadSetSampleIndex(const InstrumentationSampleIndex index) {
//	cout << "ImageMap::InstrumentationInfo::ThreadSetSampleIndex(" << index << ") [" << this << "]" << endl;

	ThreadData *ti = threadInfo[boost::this_thread::get_id()];
	ti->currentSamplesIndex = (u_int)index;
}

void ImageMap::InstrumentationInfo::ThreadAddSample(const luxrays::UV &uv) {
//	cout << "ImageMap::InstrumentationInfo::ThreadAddSample(" << uv << ") [" << this << ", " << ti->currentSamplesIndex << "]" << endl;

	ThreadData *ti = threadInfo[boost::this_thread::get_id()];
	ti->samples[ti->currentSamplesIndex].push_back(uv);
}

void ImageMap::InstrumentationInfo::ThreadAccumulateSamples() {
//	cout << "ImageMap::InstrumentationInfo::ThreadAccumulateSamples() [" << this << "]" << endl;

	ThreadData *ti = threadInfo[boost::this_thread::get_id()];
	if ((ti->samples[0].size() > 0) &&
			(ti->samples[0].size() == ti->samples[1].size()) &&
			(ti->samples[0].size() == ti->samples[2].size())) {
		for (u_int i = 0; i < ti->samples[0].size(); ++i) {
			for (u_int j = 1; j <= 2; ++j) {
				const UV dist = ti->samples[j][i] - ti->samples[0][i];

				ti->maxDistance.u = Max(ti->maxDistance.u, fabsf(dist.u));
				ti->maxDistance.v = Max(ti->maxDistance.v, fabsf(dist.v));				
			}

			ti->samplesCount += 1;
		}
	}
	
//	cout << "[ti->maxDistance = " << ti->maxDistance << "][ti->samplesCount = " << ti->samplesCount << "]" << endl;

	ti->currentSamplesIndex = 0;
	ti->samples[0].clear();
	ti->samples[1].clear();
	ti->samples[2].clear();
}
