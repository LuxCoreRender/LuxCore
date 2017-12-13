/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_LINEAR_TONEMAP_H
#define	_SLG_LINEAR_TONEMAP_H

#include <cmath>
#include <string>
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "slg/film/imagepipeline/plugins/tonemaps/tonemap.h"

namespace slg {

//------------------------------------------------------------------------------
// Linear tone mapping
//------------------------------------------------------------------------------

class LinearToneMap : public ToneMap {
public:
	LinearToneMap();
	LinearToneMap(const float s);
	virtual ~LinearToneMap();

	virtual ToneMapType GetType() const { return TONEMAP_LINEAR; }

	virtual ToneMap *Copy() const {
		return new LinearToneMap(scale);
	}

	virtual void Apply(Film &film, const u_int index);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	virtual bool CanUseOpenCL() const { return true; }
	virtual void ApplyOCL(Film &film, const u_int index);
#endif

	float scale;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ToneMap);
		ar & scale;
	}

#if !defined(LUXRAYS_DISABLE_OPENCL)
	cl::Kernel *applyKernel;
#endif
};

}

BOOST_CLASS_VERSION(slg::LinearToneMap, 1)

BOOST_CLASS_EXPORT_KEY(slg::LinearToneMap)

#endif	/* _SLG_LINEAR_TONEMAP_H */
