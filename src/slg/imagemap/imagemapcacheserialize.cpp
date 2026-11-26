/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include <boost/serialization/shared_ptr.hpp>

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
template void ImageMapResizePolicy::serialize(LuxOutputArchiveText &ar, const u_int version);
template void ImageMapResizePolicy::serialize(LuxInputArchiveText &ar, const u_int version);
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
		// I'm using a bool variable here as work around for MacOS
		const bool rpta = resizePolicyToApply[i];
		ar & rpta;

		// Save the ImageMap
		ar & maps[i];
	}

	ar & resizePolicy;
}

template<class Archive> void ImageMapCache::load(Archive &ar, const u_int version) {
	// Load the size
	u_int s;
	ar & s;
	mapNames.resize(s);
	maps.resize(s);

	for (u_int i = 0; i < maps.size(); ++i) {
		// Load the name
		std::string &name = mapNames[i];
		ar & name;
		SDL_LOG("Loading serialized image map: " << name);

		bool rpta;
		ar & rpta;
		resizePolicyToApply[i] = rpta;

		// Load the ImageMap
		ImageMapUPtr im;
		ar & im;
		maps[i] = std::move(im);

		// The image is internally store always with a 1.0 gamma
		const std::string key = GetCacheKey(name, ImageMapConfig(1.f,
				im->GetStorage().GetStorageType(), im->GetStorage().GetWrapType(),
				ImageMapStorage::ChannelSelectionType::DEFAULT));
		mapByKey.insert(make_pair(key, std::ref(*im)));	
	}

	ar & resizePolicy;
}

namespace slg {
// Explicit instantiations for portable archives
template void ImageMapCache::save(LuxOutputArchive &ar, const u_int version) const;
template void ImageMapCache::load(LuxInputArchive &ar, const u_int version);
template void ImageMapCache::save(LuxOutputArchiveText &ar, const u_int version) const;
template void ImageMapCache::load(LuxInputArchiveText &ar, const u_int version);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
