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

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include "slg/core/sdl.h"
#include "slg/imagemap/imagemapcache.h"
#include "slg/textures/imagemaptex.h"
#include "slg/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

ImageMapCache::ImageMapCache() {
	resizePolicy = new ImageMapResizeNonePolicy();
}

ImageMapCache::~ImageMapCache() {
	//for(ImageMapPtr m: maps) {
		//// I avoid to free the static global ImageMapTexture::randomImageMap
		//if (m != ImageMapTexture::randomImageMap)
			//delete m;
	//}

	delete resizePolicy;
}

void ImageMapCache::SetImageResizePolicy(ImageMapResizePolicy *policy) {
	delete resizePolicy;
	resizePolicy = policy;
}

string ImageMapCache::GetCacheKey(const string &fileName, const ImageMapConfig &imgCfg) const {
	string key = fileName + "_#_";

	switch (imgCfg.colorSpaceCfg.colorSpaceType) {
		case ColorSpaceConfig::NOP_COLORSPACE:
			key += "CS_NOP_#_";
			break;
		case ColorSpaceConfig::LUXCORE_COLORSPACE:
			key += "CS_LUXCORE_#_" + ToString(imgCfg.colorSpaceCfg.luxcore.gamma) + "_#_";
			break;
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE:
			key += "CS_OPENCOLORIO_#_" +
					ToString(imgCfg.colorSpaceCfg.ocio.configName) + "_#_" +
					ToString(imgCfg.colorSpaceCfg.ocio.colorSpaceName) + "_#_";
			break;
		default:
			throw runtime_error("Unknown color space type in ImageMapCache::GetCacheKey(): " + 
					ToString(imgCfg.colorSpaceCfg.colorSpaceType));
	}
	
	key += ToString(imgCfg.GetStorageType()) + "_#_" +
			ToString(imgCfg.GetWrapType()) + "_#_" +
			ToString(imgCfg.GetSelectionType());

	return key;
}

string ImageMapCache::GetCacheKey(const string &fileName) const {
	return fileName;
}

ImageMapRef ImageMapCache::GetImageMap(
	const string &fileName, const ImageMapConfig &imgCfg, const bool applyResizePolicy
) {
	// Compose the cache key
	string key = GetCacheKey(fileName);

	// Check if the image map has been already defined
	auto it = mapByKey.find(key);

	if (it != mapByKey.end()) {
		//SDL_LOG("Cached defined image map: " << fileName);
		auto im = (it->second);
		return im.get();
	}

	// Check if it is a reference to a file
	key = GetCacheKey(fileName, imgCfg);
	key += applyResizePolicy ? "_#_RESIZE_POLICY" : "_#_NO_RESIZE_POLICY";
	it = mapByKey.find(key);

	if (it != mapByKey.end()) {
		//SDL_LOG("Cached file image map: " << fileName);
		auto im = (it->second);
		return im.get();
	}

	// I haven't yet loaded the file

	ImageMapUPtr im;
	if (applyResizePolicy) {
		// Scale the image if required
		bool toApply;
		im = resizePolicy->ApplyResizePolicy(fileName, imgCfg, toApply);

		resizePolicyToApply.push_back(toApply);
	} else {
		im = std::make_unique<ImageMap>(fileName, imgCfg);

		resizePolicyToApply.push_back(false);
	}

	auto& imRef = AppendImageMap(ImageMapSPtr(std::move(im)), fileName, key);

	return std::ref(imRef);
}

ImageMapRef ImageMapCache::DefineImageMap(ImageMapUPtr&& im) {
	// Move im into a shared_ptr
	auto ims = ImageMapSPtr(std::move(im));

	return DefineImageMap(ims);
}

ImageMapRef ImageMapCache::DefineImageMap(ImageMapSPtr im) {
	const string name = im->GetName();

	SDL_LOG("Define ImageMap: " << name);

	// Compose the cache key
	const string key = GetCacheKey(name);

	auto it = mapByKey.find(key);

	if (it == mapByKey.end()) {
		// New value: append to container
		auto& imRef = AppendImageMap(im, name, key);
		return std::ref(imRef);
	}

	// Existing value: overwrite the existing image definition
	const u_int index = GetImageMapIndex(it->second.get());
	std::swap(maps[index], im);

	resizePolicyToApply[index] = false;

	// I have to modify mapByName for last or it iterator would be modified
	// otherwise (it->second would point to the new ImageMap and not to the old one)
	mapByKey.erase(key);
	mapByKey.insert(make_pair(key, std::ref(*maps[index])));

	return std::ref(*maps[index]);
}

void ImageMapCache::DeleteImageMap(ImageMapConstRef im) {
	for (auto it = mapByKey.begin(); it != mapByKey.end(); ++it) {
		if (&it->second.get() == &im) {
			mapByKey.erase(it);

			for (u_int i = 0; i < maps.size(); ++i) {
				if (&*maps[i] == &im) {
					mapNames.erase(mapNames.begin() + i);
					maps.erase(maps.begin() + i);
					resizePolicyToApply.erase(resizePolicyToApply.begin() + i);
					break;
				}
			}

			return;
		}
	}
}

string ImageMapCache::GetSequenceFileName(ImageMapConstRef im) const {
	return ("imagemap-" + ((boost::format("%05d") % GetImageMapIndex(im)).str()) +
			"." + im.GetFileExtension());
}

u_int ImageMapCache::GetImageMapIndex(ImageMapConstRef im) const {
	for (u_int i = 0; i < maps.size(); ++i) {
		if (&*maps[i] == &im)
			return i;
	}

	throw runtime_error("Unknown image map: " + ToString(&im));
}
u_int ImageMapCache::GetImageMapIndex(ImageMapConstPtr p) const {
	return GetImageMapIndex(*p);
}

void ImageMapCache::GetImageMaps(std::vector<std::reference_wrapper<const ImageMap>> &ims) const {
	ims.reserve(maps.size());

	for(auto& im: maps)
		ims.push_back(std::ref(*im));
}

void ImageMapCache::Preprocess(SceneConstRef scene, const bool useRTMode) {
	resizePolicy->Preprocess(*this, scene, useRTMode);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
