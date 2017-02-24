/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_VARIANCECLAMPING_H
#define	_SLG_VARIANCECLAMPING_H

#include "luxrays/luxrays.h"

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

namespace slg {

class Film;
class SampleResult;

class VarianceClamping {
public:
	VarianceClamping();
	VarianceClamping(const float sqrtMaxValue);
	
	bool hasClamping() const { return (sqrtVarianceClampMaxValue > 0.f); }
	
	void Clamp(const Film &film, SampleResult &sampleResult) const;
	// Used by Film::VarianceClampFilm()
	void Clamp(const float expectedValue[4], float value[4]) const;

	float sqrtVarianceClampMaxValue;

	friend class boost::serialization::access;	
	
private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & sqrtVarianceClampMaxValue;
	}
};

}

BOOST_CLASS_VERSION(slg::VarianceClamping, 1)

#endif	/* _SLG_VARIANCECLAMPING_H */

