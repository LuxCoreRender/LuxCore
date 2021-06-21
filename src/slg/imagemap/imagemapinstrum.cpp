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
// ImageMap InstrumentationInfo ThreadData
//------------------------------------------------------------------------------

ImageMap::InstrumentationInfo::ThreadData::ThreadData() : currentSamplesIndex(0), samplesCount(0),
		minDistance(numeric_limits<float>::infinity()) {
}

ImageMap::InstrumentationInfo::ThreadData::~ThreadData() {
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
	
	boost::unique_lock<boost::mutex> lock(classLock);
	ThreadData *ti = threadInfo[boost::this_thread::get_id()];
	if (ti->samplesCount > 0) {
//		cout << "Min. U distance in pixel: " << (ti->minDistance * originalWidth) << endl;
//		cout << "Min. V distance in pixel: " << (ti->minDistance * originalHeigth) << endl;

		// Min distance U/V should match one pixel
		if (ti->minDistance != numeric_limits<float>::infinity()) {
			const u_int w = (u_int)(originalWidth / (ti->minDistance * originalWidth));
			const u_int h = (u_int)(originalHeigth / (ti->minDistance * originalHeigth));

			AtomicMax(&optimalWidth, w);
			AtomicMax(&optimalHeigth, h);
		}
	}

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
				const UV distUV = ti->samples[j][i] - ti->samples[0][i];
				const float dist = sqrtf(distUV.u * distUV.u  + distUV.v * distUV.v);

				ti->minDistance = Min(ti->minDistance, dist);
			}

			ti->samplesCount += 1;
		}
	}
	
//	cout << "[ti->minDistance = " << ti->minDistance << "][ti->samplesCount = " << ti->samplesCount << "]" << endl;

	ti->currentSamplesIndex = 0;
	ti->samples[0].clear();
	ti->samples[1].clear();
	ti->samples[2].clear();
}
