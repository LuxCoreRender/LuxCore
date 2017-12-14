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

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "slg/imagemap/imagemapcache.h"
#include "slg/core/sdl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapCache)

ImageMapCache::ImageMapCache() {
	allImageScale = 1.f;
}

ImageMapCache::~ImageMapCache() {
	BOOST_FOREACH(ImageMap *m, maps)
		delete m;
}

string ImageMapCache::GetCacheKey(const string &fileName, const float gamma,
		const ImageMapStorage::ChannelSelectionType selectionType,
		const ImageMapStorage::StorageType storageType) const {
	return fileName + "_#_" + ToString(gamma) + "_#_" + ToString(selectionType) +
			"_#_" + ToString(storageType);
}

string ImageMapCache::GetCacheKey(const string &fileName) const {
	return fileName;
}

ImageMap *ImageMapCache::GetImageMap(const string &fileName, const float gamma,
		const ImageMapStorage::ChannelSelectionType selectionType,
		const ImageMapStorage::StorageType storageType) {
	// Compose the cache key
	string key = GetCacheKey(fileName);

	// Check if the imagemap has been already defined
	boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.find(key);

	if (it != mapByName.end()) {
		//SDL_LOG("Cached defined image map: " << fileName);
		ImageMap *im = (it->second);
		return im;
	}

	// Check if it is a reference to a file
	key = GetCacheKey(fileName, gamma, selectionType, storageType);
	it = mapByName.find(key);

	if (it != mapByName.end()) {
		//SDL_LOG("Cached file image map: " << fileName);
		ImageMap *im = (it->second);
		return im;
	}

	// I haven't yet loaded the file

	ImageMap *im = new ImageMap(fileName, gamma, storageType);
	im->SelectChannel(selectionType);

	// Scale the image if required
	const u_int width = im->GetWidth();
	const u_int height = im->GetHeight();
	if (allImageScale > 1.f) {
		// Enlarge all images
		const u_int newWidth = width * allImageScale;
		const u_int newHeight = height * allImageScale;
		im->Resize(newWidth, newHeight);
	} else if ((allImageScale < 1.f) && (width > 128) && (height > 128)) {
		const u_int newWidth = Max<u_int>(128, width * allImageScale);
		const u_int newHeight = Max<u_int>(128, height * allImageScale);
		im->Resize(newWidth, newHeight);
	}

	mapByName.insert(make_pair(key, im));
	maps.push_back(im);

	return im;
}

void ImageMapCache::DefineImageMap(const string &name, ImageMap *im) {
	SDL_LOG("Define ImageMap: " << name);

	// Compose the cache key
	const string key = GetCacheKey(name);

	boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.find(key);
	if (it == mapByName.end()) {
		// Add the new image definition
		mapByName.insert(make_pair(key, im));
		maps.push_back(im);
	} else {
		// Overwrite the existing image definition
		const u_int index = GetImageMapIndex(it->second);
		delete maps[index];
		maps[index] = im;

		// I have to modify mapByName for last or it iterator would be modified
		// otherwise (it->second would point to the new ImageMap and not to the old one)
		mapByName.erase(key);
		mapByName.insert(make_pair(key, im));
	}
}

u_int ImageMapCache::GetImageMapIndex(const ImageMap *im) const {
	for (u_int i = 0; i < maps.size(); ++i) {
		if (maps[i] == im)
			return i;
	}

	throw runtime_error("Unknown image map: " + boost::lexical_cast<string>(im));
}

void ImageMapCache::GetImageMaps(vector<const ImageMap *> &ims) {
	ims.reserve(maps.size());

	BOOST_FOREACH(ImageMap *im, maps)
		ims.push_back(im);
}
