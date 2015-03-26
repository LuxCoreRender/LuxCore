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

#ifndef _SLG_AUTOLINEAR_TONEMAP_H
#define	_SLG_AUTOLINEAR_TONEMAP_H

#include <cmath>
#include <string>
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>

#include "slg/film/imagepipeline/plugins/tonemaps/tonemap.h"

namespace slg {

//------------------------------------------------------------------------------
// Auto-linear tone mapping
//------------------------------------------------------------------------------

class AutoLinearToneMap : public ToneMap {
public:
	AutoLinearToneMap() { }

	ToneMapType GetType() const { return TONEMAP_AUTOLINEAR; }

	ToneMap *Copy() const {
		return new AutoLinearToneMap();
	}

	void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ToneMap>(*this);
	}
};

}

BOOST_CLASS_VERSION(slg::AutoLinearToneMap, 1)

BOOST_CLASS_EXPORT_KEY(slg::AutoLinearToneMap)

#endif	/* _SLG_AUTOLINEAR_TONEMAP_H */
