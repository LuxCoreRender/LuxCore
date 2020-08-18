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

#ifndef _SLG_BLOOM_PLUGIN_H
#define	_SLG_BLOOM_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/core/color/color.h"
#include "luxrays/core/hardwaredevice.h"
#include "luxrays/utils/serializationutils.h"
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

	virtual void Apply(Film &film, const u_int index);

	virtual bool CanUseHW() const { return true; }
	virtual void AddHWChannelsUsed(Film::FilmChannels &hwChannelsUsed) const;
	virtual void ApplyHW(Film &film, const u_int index);

	float radius, weight;

	friend class boost::serialization::access;

private:
	// Used by serialization
	BloomFilterPlugin();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & radius;
		ar & weight;
	}

	void InitFilterTable(const Film &film);
	void BloomFilterX(const Film &film, luxrays::Spectrum *pixels);
	void BloomFilterY(const Film &film);
	void BloomFilter(const Film &film, luxrays::Spectrum *pixels);

	luxrays::Spectrum *bloomBuffer;
	luxrays::Spectrum *bloomBufferTmp;
	size_t bloomBufferSize;

	float *bloomFilter;
	size_t bloomFilterSize;

	u_int bloomWidth;

	// Used inside the object destructor to free buffers
	luxrays::HardwareDevice *hardwareDevice;
	luxrays::HardwareDeviceBuffer *hwBloomBuffer;
	luxrays::HardwareDeviceBuffer *hwBloomBufferTmp;
	luxrays::HardwareDeviceBuffer *hwBloomFilter;

	luxrays::HardwareDeviceKernel *bloomFilterXKernel;
	luxrays::HardwareDeviceKernel *bloomFilterYKernel;
	luxrays::HardwareDeviceKernel *bloomFilterMergeKernel;
};

}

BOOST_CLASS_VERSION(slg::BloomFilterPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::BloomFilterPlugin)

#endif	/*  _SLG_BLOOM_PLUGIN_H */
