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
//Intel Open Image Denoise
//------------------------------------------------------------------------------

class IntelOIDN : public ImagePipelinePlugin {
public:
	IntelOIDN(u_int n, u_int o, u_int t, bool b);

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	virtual void ApplyTiled(Film & film, const u_int index);

	virtual void ApplySingle(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
	}

	u_int nTiles;
	u_int pixelOverlap;
	u_int pixelThreshold;
	bool benchMode;

};

}


BOOST_CLASS_VERSION(slg::IntelOIDN, 1)

BOOST_CLASS_EXPORT_KEY(slg::IntelOIDN)

#endif /* _SLG_INTEL_OIDN_H */