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

#ifndef _SLG_RADIANCECHANNELSCALE_H
#define	_SLG_RADIANCECHANNELSCALE_H

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/utils/serializationutils.h"

namespace slg {

//------------------------------------------------------------------------------
// RadianceChannelScale
//------------------------------------------------------------------------------

class RadianceChannelScale {
public:
	RadianceChannelScale();

	void Init();

	// It is very important for performance to have Scale() methods in-lined
	void Scale(float v[3]) const {
		v[0] *= scale.c[0];
		v[1] *= scale.c[1];
		v[2] *= scale.c[2];
	}

	luxrays::Spectrum Scale(const luxrays::Spectrum &v) const {
		return v * scale;
	}

	const luxrays::Spectrum &GetScale() const { return scale; }

	float globalScale, temperature;
	luxrays::Spectrum rgbScale;
	bool reverse, normalize, enabled;

	friend class boost::serialization::access;

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	luxrays::Spectrum scale;
};

}

BOOST_CLASS_VERSION(slg::RadianceChannelScale, 2)
		
BOOST_CLASS_EXPORT_KEY(slg::RadianceChannelScale)

#endif	/*  _SLG_RADIANCECHANNELSCALE_H */
