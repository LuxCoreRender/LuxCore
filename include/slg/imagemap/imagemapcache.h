/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_IMAGEMAPCACHE_H
#define	_SLG_IMAGEMAPCACHE_H

#include <string>
#include <vector>

#include <boost/unordered_map.hpp>

#include "slg/imagemap/imagemap.h"

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

	// Get a path/name from imageMap object
	const std::string &GetPath(const ImageMap *im)const {
		for (boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.begin(); it != mapByName.end(); ++it) {
			if (it->second == im)
				return it->first;
		}
		throw std::runtime_error("Unknown ImageMap in ImageMapCache::GetPath()");
	}

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

	u_int GetImageMapIndex(const ImageMap *im) const;

	void GetImageMaps(std::vector<const ImageMap *> &ims);
	u_int GetSize()const { return static_cast<u_int>(mapByName.size()); }
	bool IsImageMapDefined(const std::string &name) const { return mapByName.find(name) != mapByName.end(); }

private:
	std::string GetCacheKey(const std::string &fileName, const float gamma,
		const ImageMapStorage::ChannelSelectionType selectionType,
		const ImageMapStorage::StorageType storageType) const;
	std::string GetCacheKey(const std::string &fileName) const;

	boost::unordered_map<std::string, ImageMap *> mapByName;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<ImageMap *> maps;

	float allImageScale;
};

}

#endif	/* _SLG_IMAGEMAPCACHE_H */
