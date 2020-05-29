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

void CompiledScene::AddToImageMapMem(slg::ocl::ImageMap &im, void *data, const size_t dataSize) {
	const size_t memSize = RoundUp(dataSize, sizeof(float));

	if (memSize > maxMemPageSize)
		throw runtime_error("An image data block is too big to fit in a single block of memory");

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

	const size_t start = imageMapMemBlock.size();
	const size_t memSizeInFloat = memSize / sizeof(float);
	imageMapMemBlock.resize(start + memSizeInFloat);
	memcpy(&imageMapMemBlock[start], data, dataSize);

	im.pageIndex = page;
	im.pixelsIndex = start;
}

void CompiledScene::CompileImageMaps() {
	SLG_LOG("Compile ImageMaps");
	wasImageMapsCompiled = true;

	imageMapDescs.resize(0);
	imageMapMemBlocks.resize(0);

	//--------------------------------------------------------------------------
	// Translate image maps
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	vector<const ImageMap *> ims;
	scene->imgMapCache.GetImageMaps(ims);

	imageMapDescs.resize(ims.size());
	for (u_int i = 0; i < ims.size(); ++i) {
		const ImageMap *im = ims[i];
		slg::ocl::ImageMap *imd = &imageMapDescs[i];

		imd->channelCount = im->GetChannelCount();
		imd->width = im->GetWidth();
		imd->height = im->GetHeight();

		switch (im->GetStorage()->wrapType) {
			case ImageMapStorage::REPEAT:
				imd->wrapType = slg::ocl::WRAP_REPEAT;
				break;
			case ImageMapStorage::BLACK:
				imd->wrapType = slg::ocl::WRAP_BLACK;
				break;
			case ImageMapStorage::WHITE:
				imd->wrapType = slg::ocl::WRAP_WHITE;
				break;
			case ImageMapStorage::CLAMP:
				imd->wrapType = slg::ocl::WRAP_CLAMP;
				break;
			default:
				throw runtime_error("Unknown wrap type in CompiledScene::CompileImageMaps(): " +
						ToString(im->GetStorage()->wrapType));
		}

		switch (im->GetStorage()->GetStorageType()) {
			case ImageMapStorage::BYTE:
				imd->storageType = slg::ocl::BYTE;
				break;
			case ImageMapStorage::HALF:
				imd->storageType = slg::ocl::HALF;
				break;
			case ImageMapStorage::FLOAT:
				imd->storageType = slg::ocl::FLOAT;
				break;
			default:
				throw runtime_error("Unknown storage type in CompiledScene::CompileImageMaps(): " +
						ToString(im->GetStorage()->GetStorageType()));
		}

		AddToImageMapMem(*imd, im->GetStorage()->GetPixelsData(), im->GetStorage()->GetMemorySize());
	}

	SLG_LOG("Image maps page(s) count: " << imageMapMemBlocks.size());
	for (u_int i = 0; i < imageMapMemBlocks.size(); ++i)
		SLG_LOG(" RGB channel page " << i << " size: " << imageMapMemBlocks[i].size() * sizeof(float) / 1024 << "Kbytes");

	const double tEnd = WallClockTime();
	SLG_LOG("Image maps compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
