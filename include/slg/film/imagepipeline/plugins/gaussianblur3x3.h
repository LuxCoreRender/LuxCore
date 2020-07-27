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

#ifndef _SLG_GAUSSIANBLUR_3X3_PLUGIN_H
#define	_SLG_GAUSSIANBLUR_3X3_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/core/color/color.h"
#include "luxrays/core/hardwaredevice.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

class GaussianBlur3x3FilterPlugin : public ImagePipelinePlugin {
public:
	GaussianBlur3x3FilterPlugin(const float w);
	virtual ~GaussianBlur3x3FilterPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	virtual bool CanUseHW() const { return true; }
	virtual void AddHWChannelsUsed(std::unordered_set<Film::FilmChannelType> &hwChannelsUsed) const;
	virtual void ApplyHW(Film &film, const u_int index);

	template<class T> static void ApplyBlurFilter(const u_int width, const u_int height,
			T *src, T *tmpBuffer,
			const float aF, const float bF, const float cF);

	float weight;

	friend class boost::serialization::access;

private:
	// Used by serialization
	GaussianBlur3x3FilterPlugin();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & weight;
	}

	template<class T> static void ApplyBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const T *src, T *dst,
		const float aF, const float bF, const float cF);
	template<class T> static void ApplyBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const T *src, T *dst,
		const float aF, const float bF, const float cF);

	luxrays::Spectrum *tmpBuffer;
	size_t tmpBufferSize;

	// Used inside the object destructor to free buffers
	luxrays::HardwareDevice *hardwareDevice;
	luxrays::HardwareDeviceBuffer *hwTmpBuffer;

	luxrays::HardwareDeviceKernel *filterXKernel;
	luxrays::HardwareDeviceKernel *filterYKernel;
};

}

BOOST_CLASS_VERSION(slg::GaussianBlur3x3FilterPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::GaussianBlur3x3FilterPlugin)

#endif	/*  _SLG_GAUSSIANBLUR_3X3_PLUGIN_H */
