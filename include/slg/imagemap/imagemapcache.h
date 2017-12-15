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

#ifndef _SLG_IMAGEMAPCACHE_H
#define	_SLG_IMAGEMAPCACHE_H

#include <string>
#include <vector>

#include <boost/unordered_map.hpp>
#include <boost/serialization/version.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "slg/imagemap/imagemap.h"
#include "slg/core/sdl.h"

namespace slg {

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

class ImageMapCache {
public:
	ImageMapCache();
	~ImageMapCache();

	void SetImageResize(const float s) { allImageScale = s; }

	void DefineImageMap(const std::string &name, ImageMap *im);

	ImageMap *GetImageMap(const std::string &fileName, const float gamma,
		const ImageMapStorage::ChannelSelectionType selectionType,
		const ImageMapStorage::StorageType storageType);

	void DeleteImageMap(const ImageMap *im) {
		for (boost::unordered_map<std::string, ImageMap *>::iterator it = mapByName.begin(); it != mapByName.end(); ++it) {
			if (it->second == im) {
				delete it->second;

				maps.erase(std::find(maps.begin(), maps.end(), it->second));
				mapByName.erase(it);
				return;
			}
		}
	}

	std::string GetSequenceFileName(const ImageMap *im) const;
	u_int GetImageMapIndex(const ImageMap *im) const;

	void GetImageMaps(std::vector<const ImageMap *> &ims);
	u_int GetSize()const { return static_cast<u_int>(mapByName.size()); }
	bool IsImageMapDefined(const std::string &name) const { return mapByName.find(name) != mapByName.end(); }

	friend class boost::serialization::access;

private:
	std::string GetCacheKey(const std::string &fileName, const float gamma,
		const ImageMapStorage::ChannelSelectionType selectionType,
		const ImageMapStorage::StorageType storageType) const;
	std::string GetCacheKey(const std::string &fileName) const;

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	boost::unordered_map<std::string, ImageMap *> mapByName;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<ImageMap *> maps;

	float allImageScale;
};

template<class Archive> void ImageMapCache::load(Archive &ar, const u_int version) {
	// Load the size
	u_int s;
	ar & s;
	maps.resize(s, NULL);

	for (u_int i = 0; i < maps.size(); ++i) {
		// Load the key
		std::string key;
		ar & key;
		SDL_LOG("Loading serialized image map: " << key);

		// Load the ImageMap
		ImageMap *im;
		ar & im;
		
		mapByName.insert(make_pair(key, im));
	}

	ar & allImageScale;
}

template<class Archive> void ImageMapCache::save(Archive &ar, const u_int version) const {
	// Save the size
	const u_int s = maps.size();
	ar & s;

	for (boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.begin(); it != mapByName.end(); ++it) {
		// Save the key
		const std::string &key = it->first;
		SDL_LOG("Saving serialized image map: " << key);
		ar & key;

		// Save the ImageMap
		ImageMap *im = it->second;
		ar & im;
	}

	ar & allImageScale;
}

}

BOOST_CLASS_VERSION(slg::ImageMapCache, 1)

BOOST_CLASS_EXPORT_KEY(slg::ImageMapCache)

#endif	/* _SLG_IMAGEMAPCACHE_H */
