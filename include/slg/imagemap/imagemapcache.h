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

#ifndef _SLG_IMAGEMAPCACHE_H
#define	_SLG_IMAGEMAPCACHE_H

#include <string>
#include <vector>

#include <boost/unordered_map.hpp>

#include "luxrays/devices/ocldevice.h"
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

	void DefineImageMap(ImageMap *im);

	ImageMap *GetImageMap(const std::string &fileName, const ImageMapConfig &imgCfg);

	void DeleteImageMap(const ImageMap *im);

	std::string GetSequenceFileName(const ImageMap *im) const;
	u_int GetImageMapIndex(const ImageMap *im) const;

	void GetImageMaps(std::vector<const ImageMap *> &ims);
	u_int GetSize()const { return static_cast<u_int>(mapByKey.size()); }
	bool IsImageMapDefined(const std::string &name) const { return mapByKey.find(name) != mapByKey.end(); }

	friend class boost::serialization::access;

private:
	std::string GetCacheKey(const std::string &fileName,
				const ImageMapConfig &imgCfg) const;
	std::string GetCacheKey(const std::string &fileName) const;

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	boost::unordered_map<std::string, ImageMap *> mapByKey;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<std::string> mapNames;
	std::vector<ImageMap *> maps;

	float allImageScale;
};

}

BOOST_CLASS_VERSION(slg::ImageMapCache, 2)

BOOST_CLASS_EXPORT_KEY(slg::ImageMapCache)

#endif	/* _SLG_IMAGEMAPCACHE_H */
