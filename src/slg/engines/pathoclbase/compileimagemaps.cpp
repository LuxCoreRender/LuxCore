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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

#include "slg/imagemap/imagemap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void CompiledScene::AddEnabledImageMapCode() {
	// ImageMap storage formats
	if (enabledCode.count("IMAGEMAPS_BYTE_FORMAT")) usedImageMapFormats.insert(ImageMapStorage::BYTE);
	if (enabledCode.count("IMAGEMAPS_HALF_FORMAT")) usedImageMapFormats.insert(ImageMapStorage::HALF);
	if (enabledCode.count("IMAGEMAPS_FLOAT_FORMAT")) usedImageMapFormats.insert(ImageMapStorage::FLOAT);

	// ImageMap channels
	if (enabledCode.count("IMAGEMAPS_1xCHANNELS")) usedImageMapChannels.insert(1);
	if (enabledCode.count("IMAGEMAPS_2xCHANNELS")) usedImageMapChannels.insert(2);
	if (enabledCode.count("IMAGEMAPS_3xCHANNELS")) usedImageMapChannels.insert(3);
	if (enabledCode.count("IMAGEMAPS_4xCHANNELS")) usedImageMapChannels.insert(4);	
}

void CompiledScene::CompileImageMaps() {
	SLG_LOG("Compile ImageMaps");

	imageMapDescs.resize(0);
	imageMapMemBlocks.resize(0);

	//--------------------------------------------------------------------------
	// Translate image maps
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedImageMapFormats.clear();
	usedImageMapChannels.clear();
	AddEnabledImageMapCode();

	vector<const ImageMap *> ims;
	scene->imgMapCache.GetImageMaps(ims);

	imageMapDescs.resize(ims.size());
	for (u_int i = 0; i < ims.size(); ++i) {
		const ImageMap *im = ims[i];
		slg::ocl::ImageMap *imd = &imageMapDescs[i];

		const u_int pixelCount = im->GetWidth() * im->GetHeight();
		const size_t memSize = RoundUp(im->GetStorage()->GetMemorySize(), sizeof(float));

		if (memSize > maxMemPageSize)
			throw runtime_error("An image map is too big to fit in a single block of memory");

		bool found = false;
		u_int page;
		for (u_int j = 0; j < imageMapMemBlocks.size(); ++j) {
			// Check if it fits in this page
			if (memSize + imageMapMemBlocks[j].size() * sizeof(float) <= maxMemPageSize) {
				found = true;
				page = j;
				break;
			}
		}

		if (!found) {
			// Check if I can add a new page
			if (imageMapMemBlocks.size() > 8)
				throw runtime_error("More than 8 blocks of memory are required for image maps");

			// Add a new page
			imageMapMemBlocks.push_back(vector<float>());
			page = imageMapMemBlocks.size() - 1;
		}

		vector<float> &imageMapMemBlock = imageMapMemBlocks[page];

		imd->channelCount = im->GetChannelCount();
		imd->width = im->GetWidth();
		imd->height = im->GetHeight();
		imd->pageIndex = page;
		imd->pixelsIndex = (u_int)imageMapMemBlock.size();

		if (im->GetStorage()->GetStorageType() == ImageMapStorage::BYTE) {
			imd->storageType = slg::ocl::BYTE;

			// Copy the image map data
			const size_t start = imageMapMemBlock.size();
			const size_t dataSize = pixelCount * imd->channelCount * sizeof(u_char);
			const size_t dataSizeInFloat = RoundUp(dataSize, sizeof(float)) / sizeof(float);
			imageMapMemBlock.resize(start + dataSizeInFloat);
			memcpy(&imageMapMemBlock[start], im->GetStorage()->GetPixelsData(), dataSize);
		} else if (im->GetStorage()->GetStorageType() == ImageMapStorage::HALF) {
			imd->storageType = slg::ocl::HALF;

			// Copy the image map data
			const size_t start = imageMapMemBlock.size();
			const size_t dataSize = pixelCount * imd->channelCount * sizeof(half);
			const size_t dataSizeInFloat = RoundUp(dataSize, sizeof(float)) / sizeof(float);
			imageMapMemBlock.resize(start + dataSizeInFloat);

			memcpy(&imageMapMemBlock[start], im->GetStorage()->GetPixelsData(), dataSize);
		} else if (im->GetStorage()->GetStorageType() == ImageMapStorage::FLOAT) {
			imd->storageType = slg::ocl::FLOAT;

			// Copy the image map data
			const size_t start = imageMapMemBlock.size();
			const size_t dataSize = pixelCount * imd->channelCount * sizeof(float);
			const size_t dataSizeInFloat = RoundUp(dataSize, sizeof(float)) / sizeof(float);
			imageMapMemBlock.resize(start + dataSizeInFloat);
			memcpy(&imageMapMemBlock[start], im->GetStorage()->GetPixelsData(), dataSize);
		}

		usedImageMapFormats.insert(im->GetStorage()->GetStorageType());
		usedImageMapChannels.insert(im->GetChannelCount());
	}

	SLG_LOG("Image maps page(s) count: " << imageMapMemBlocks.size());
	for (u_int i = 0; i < imageMapMemBlocks.size(); ++i)
		SLG_LOG(" RGB channel page " << i << " size: " << imageMapMemBlocks[i].size() * sizeof(float) / 1024 << "Kbytes");

	const double tEnd = WallClockTime();
	SLG_LOG("Texture maps compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
