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

#ifndef _SLG_GAUSSIANBLUR_3X3_PLUGIN_H
#define	_SLG_GAUSSIANBLUR_3X3_PLUGIN_H

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
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

class GaussianBlur3x3FilterPlugin : public ImagePipelinePlugin {
public:
	GaussianBlur3x3FilterPlugin(const float w) : weight(w), tmpBuffer(NULL), tmpBufferSize(0) { }
	virtual ~GaussianBlur3x3FilterPlugin() { delete tmpBuffer; }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float weight;

	friend class boost::serialization::access;

private:
	// Used by serialization
	GaussianBlur3x3FilterPlugin() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & weight;
	}

	void ApplyBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst,
		const float aF, const float bF, const float cF) const;
	void ApplyBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst,
		const float aF, const float bF, const float cF) const;
	void ApplyGaussianBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst) const;
	void ApplyGaussianBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst) const;

	mutable luxrays::Spectrum *tmpBuffer;
	mutable size_t tmpBufferSize;
};

}

BOOST_CLASS_VERSION(slg::GaussianBlur3x3FilterPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::GaussianBlur3x3FilterPlugin)

#endif	/*  _SLG_GAUSSIANBLUR_3X3_PLUGIN_H */
