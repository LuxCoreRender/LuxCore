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

#ifndef _SLG_BLOOM_PLUGIN_H
#define	_SLG_BLOOM_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// Bloom filter plugin
//------------------------------------------------------------------------------

class BloomFilterPlugin : public ImagePipelinePlugin {
public:
	BloomFilterPlugin(const float r, const float w);
	virtual ~BloomFilterPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float radius, weight;

	friend class boost::serialization::access;

private:
	// Used by serialization
	BloomFilterPlugin() {
		bloomBuffer = NULL;
		bloomFilter = NULL;
	}

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & radius;
		ar & weight;
	}

	void BloomFilterX(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;
	void BloomFilterY(const Film &film, std::vector<bool> &pixelsMask) const;
	void BloomFilter(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	mutable luxrays::Spectrum *bloomBuffer;
	mutable luxrays::Spectrum *bloomBufferTmp;
	mutable float *bloomFilter;
	mutable size_t bloomBufferSize;
	mutable u_int bloomWidth;
};

}

BOOST_CLASS_VERSION(slg::BloomFilterPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::BloomFilterPlugin)

#endif	/*  _SLG_BLOOM_PLUGIN_H */
