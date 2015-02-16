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

#ifndef _SLG_RANDOM_SAMPLER_H
#define	_SLG_RANDOM_SAMPLER_H

#include <string>
#include <vector>

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"

namespace slg {

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

class RandomSampler : public Sampler {
public:
	RandomSampler(luxrays::RandomGenerator *rnd, Film *flm) : Sampler(rnd, flm) { }
	virtual ~RandomSampler() { }

	virtual SamplerType GetType() const { return RANDOM; }
	virtual void RequestSamples(const u_int size) { }

	virtual float GetSample(const u_int index) { return rndGen->floatValue(); }
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);
};

}

#endif	/* _SLG_RANDOM_SAMPLER_H */
