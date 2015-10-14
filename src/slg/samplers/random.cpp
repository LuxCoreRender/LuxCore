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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/color/color.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/random.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

void RandomSampler::NextSample(const std::vector<SampleResult> &sampleResults) {
	film->AddSampleCount(1.0);
	AddSamplesToFilm(sampleResults);
}

Properties RandomSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			Property("sampler.type")(SamplerType2String(RANDOM));
}

// Used to register ToProperties(const luxrays::Properties &cfg) static method with parent class
FuncTableRegister<Sampler::ToPropertiesFuncPtr> RandomSampler::toPropertiesFuncTableRegister(
	Sampler::toPropertiesFuncTable, Sampler::SamplerType2String(RANDOM), RandomSampler::ToProperties);
