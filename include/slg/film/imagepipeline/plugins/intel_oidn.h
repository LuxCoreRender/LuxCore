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

#ifndef _SLG_INTEL_OIDN_H
#define	_SLG_INTEL_OIDN_H

#include <vector>

#include <boost/serialization/export.hpp>

#include <OpenImageDenoise/oidn.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// Intel Open Image Denoise
//------------------------------------------------------------------------------

class IntelOIDN : public ImagePipelinePlugin {
public:
	IntelOIDN(const std::string filterType,
			const int oidnMemLimit, const float sharpness);

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	// Used by serialization
	IntelOIDN();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & oidnMemLimit;
		ar & iTileCount;
		ar & jTileCount;
		ar & sharpness;
	}

	std::string filterType;
	u_int iTileCount;
	u_int jTileCount;
	int oidnMemLimit; //needs to be signed int for OIDN call
	float sharpness;
};

}


BOOST_CLASS_VERSION(slg::IntelOIDN, 3)

BOOST_CLASS_EXPORT_KEY(slg::IntelOIDN)

#endif /* _SLG_INTEL_OIDN_H */