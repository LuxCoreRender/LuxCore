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

#include "slg/imagemap/imagemapcache.h"
#include "slg/core/sdl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapResizePolicy Serialization
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapResizePolicy)

namespace slg {
// Explicit instantiations for portable archives
template void ImageMapResizePolicy::serialize(LuxOutputArchive &ar, const u_int version);
template void ImageMapResizePolicy::serialize(LuxInputArchive &ar, const u_int version);
}

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapResizeNonePolicy)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapResizeFixedPolicy)

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImageMapCache)

template<class Archive> void ImageMapCache::save(Archive &ar, const u_int version) const {
	// Save the size
	const u_int s = maps.size();
	ar & s;

	for (u_int i = 0; i < maps.size(); ++i) {
		// Save the name
		const std::string &name = mapNames[i];
		SDL_LOG("Saving serialized image map: " << name);
		ar & name;
		ar & resizePolicyToApply[i];

		// Save the ImageMap
		ImageMap *im = maps[i];
		ar & im;
	}

	ar & resizePolicy;
}

template<class Archive> void ImageMapCache::load(Archive &ar, const u_int version) {
	// Load the size
	u_int s;
	ar & s;
	mapNames.resize(s);
	maps.resize(s, NULL);

	for (u_int i = 0; i < maps.size(); ++i) {
		// Load the name
		std::string &name = mapNames[i];
		ar & name;
		SDL_LOG("Loading serialized image map: " << name);

		bool rpta;
		ar & rpta;
		resizePolicyToApply[i] = rpta;

		// Load the ImageMap
		ImageMap *im;
		ar & im;
		maps[i] = im;

		// The image is internally store always with a 1.0 gamma
		const std::string key = GetCacheKey(name, ImageMapConfig(1.f,
				im->GetStorage()->GetStorageType(), im->GetStorage()->wrapType,
				ImageMapStorage::ChannelSelectionType::DEFAULT));
		mapByKey.insert(make_pair(key, im));	
	}

	ar & resizePolicy;
}

namespace slg {
// Explicit instantiations for portable archives
template void ImageMapCache::save(LuxOutputArchive &ar, const u_int version) const;
template void ImageMapCache::load(LuxInputArchive &ar, const u_int version);
}
