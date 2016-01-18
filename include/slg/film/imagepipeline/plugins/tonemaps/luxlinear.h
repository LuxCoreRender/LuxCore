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

#ifndef _SLG_LUXLINEAR_TONEMAP_H
#define	_SLG_LUXLINEAR_TONEMAP_H

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
// LuxRender Linear tone mapping
//------------------------------------------------------------------------------

class LuxLinearToneMap : public ToneMap {
public:
	LuxLinearToneMap();
	LuxLinearToneMap(const float s, const float e, const float f);
	virtual ~LuxLinearToneMap();

	virtual ToneMapType GetType() const { return TONEMAP_LUXLINEAR; }

	virtual ToneMap *Copy() const {
		return new LuxLinearToneMap(sensitivity, exposure, fstop);
	}

	virtual void Apply(Film &film);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	virtual bool CanUseOpenCL() const { return true; }
	virtual void ApplyOCL(Film &film);
#endif

	float sensitivity, exposure, fstop;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ToneMap>(*this);
		ar & sensitivity;
		ar & exposure;
		ar & fstop;
	}

	float GetGammaCorrectionValue(Film &film) const;
	float GetScale(const float gamma) const;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	cl::Kernel *applyKernel;
#endif
};

}

BOOST_CLASS_VERSION(slg::LuxLinearToneMap, 1)

BOOST_CLASS_EXPORT_KEY(slg::LuxLinearToneMap)

#endif	/* _SLG_LUXLINEAR_TONEMAP_H */
