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

#ifndef _SLG_TONEMAP_H
#define	_SLG_TONEMAP_H

#include <cmath>
#include <string>

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// Tone mapping
//------------------------------------------------------------------------------

typedef enum {
	TONEMAP_LINEAR, TONEMAP_REINHARD02, TONEMAP_AUTOLINEAR, TONEMAP_LUXLINEAR,
	TONEMAP_OPENCOLORIO
} ToneMapType;

extern std::string ToneMapType2String(const ToneMapType type);
extern ToneMapType String2ToneMapType(const std::string &type);

class Film;

class ToneMap : public ImagePipelinePlugin {
public:
	ToneMap() {}
	virtual ~ToneMap() {}

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
	}
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::ToneMap)

BOOST_CLASS_VERSION(slg::ToneMap, 1)

#endif	/* _SLG_TONEMAP_H */
