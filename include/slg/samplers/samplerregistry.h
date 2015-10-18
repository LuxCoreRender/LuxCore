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

#ifndef _SLG_SAMPLERREGISTER_H
#define	_SLG_SAMPLERREGISTER_H

#include <string>
#include <vector>

#include "slg/core/statictable.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/random.h"
#include "slg/samplers/sobol.h"
#include "slg/samplers/metropolis.h"

namespace slg {

//------------------------------------------------------------------------------
// SamplerSharedDataRegistry
//------------------------------------------------------------------------------

	// For an easy the declaration and registration of each Sampler sub-class
// with Sampler StaticTable
#define SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(C) \
STATICTABLE_DECLARE_REGISTRATION(C, FromProperties)

#define SAMPLERSHAREDDATA_STATICTABLE_REGISTER(STRTAG, C) \
STATICTABLE_REGISTER(SamplerSharedDataRegistry, C, STRTAG, FromProperties)

class SamplerSharedDataRegistry {
	SamplerSharedDataRegistry() { }

protected:
	// Used to register all sub-class FromProperties() static methods
	typedef SamplerSharedData *(*FromPropertiesStaticTableType)(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen);
	STATICTABLE_DECLARE_DECLARATION(FromProperties);

	// For the registration of each SamplerSharedData sub-class with SamplerSharedData StaticTable
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(RandomSamplerSharedData);
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(SobolSamplerSharedData);
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(MetropolisSamplerSharedData);
	// Just add here any new SamplerSharedData (don't forget in the .cpp too)

	friend class SamplerSharedData;
};

//------------------------------------------------------------------------------
// Sampler
//------------------------------------------------------------------------------

// For an easy the declaration and registration of each Sampler sub-class
// with Sampler StaticTable
#define SAMPLER_STATICTABLE_DECLARE_REGISTRATION(C) \
STATICTABLE_DECLARE_REGISTRATION(C, ToProperties); \
STATICTABLE_DECLARE_REGISTRATION(C, FromProperties)

#define SAMPLER_STATICTABLE_REGISTER(TAG, STRTAG, C) \
STATICTABLE_REGISTER(SamplerRegistry, C, STRTAG, ToProperties); \
STATICTABLE_REGISTER(SamplerRegistry, C, STRTAG, FromProperties)

class SamplerRegistry {
protected:
	SamplerRegistry() { }

	// Used to register all sub-class ToProperties() static methods
	typedef luxrays::Properties (*ToPropertiesStaticTableType)(const luxrays::Properties &cfg);
	STATICTABLE_DECLARE_DECLARATION(ToProperties);
	// Used to register all sub-class FromProperties() static methods
	typedef Sampler *(*FromPropertiesStaticTableType)(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	STATICTABLE_DECLARE_DECLARATION(FromProperties);

	SAMPLER_STATICTABLE_DECLARE_REGISTRATION(RandomSampler);
	SAMPLER_STATICTABLE_DECLARE_REGISTRATION(SobolSampler);
	SAMPLER_STATICTABLE_DECLARE_REGISTRATION(MetropolisSampler);
	// Just add here any new Sampler (don't forget in the .cpp too)

	friend class Sampler;
};

}

#endif	/* _SLG_SAMPLERREGISTER_H */
