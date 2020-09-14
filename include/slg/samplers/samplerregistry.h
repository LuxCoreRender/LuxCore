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

#ifndef _SLG_SAMPLERREGISTRY_H
#define	_SLG_SAMPLERREGISTRY_H

#include <string>
#include <vector>

#include "slg/core/objectstaticregistry.h"
#include "slg/samplers/random.h"
#include "slg/samplers/sobol.h"
#include "slg/samplers/metropolis.h"
#include "slg/samplers/rtpathcpusampler.h"
#include "slg/samplers/tilepathsampler.h"

namespace slg {

//------------------------------------------------------------------------------
// SamplerSharedDataRegistry
//------------------------------------------------------------------------------

// For an easy the declaration and registration of each Sampler sub-class
// with Sampler StaticTable
#define SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(R, C) \
STATICTABLE_DECLARE_REGISTRATION(R, C, std::string, FromProperties)

#define SAMPLERSHAREDDATA_STATICTABLE_REGISTER(STRTAG, C) \
STATICTABLE_REGISTER(SamplerSharedDataRegistry, C, STRTAG, std::string, FromProperties)

class SamplerSharedDataRegistry {
	SamplerSharedDataRegistry() { }

protected:
	// Used to register all sub-class FromProperties() static methods
	typedef SamplerSharedData *(*FromProperties)(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen, Film *film);
	STATICTABLE_DECLARE_DECLARATION(SamplerSharedDataRegistry, std::string, FromProperties);

	// For the registration of each SamplerSharedData sub-class with SamplerSharedData StaticTable
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(SamplerSharedDataRegistry, RandomSamplerSharedData);
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(SamplerSharedDataRegistry, SobolSamplerSharedData);
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(SamplerSharedDataRegistry, MetropolisSamplerSharedData);
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(SamplerSharedDataRegistry, RTPathCPUSamplerSharedData);
	SAMPLERSHAREDDATA_STATICTABLE_DECLARE_REGISTRATION(SamplerSharedDataRegistry, TilePathSamplerSharedData);
	// Just add here any new SamplerSharedData (don't forget in the .cpp too)

	friend class SamplerSharedData;
};

//------------------------------------------------------------------------------
// SamplerRegistry
//------------------------------------------------------------------------------

// Add the AddRequiredChannels() to the list of standard methods

#define SAMPLER_OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(R, C) \
OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(R, C); \
STATICTABLE_DECLARE_REGISTRATION(R, C, std::string, AddRequiredChannels)

#define SAMPLER_OBJECTSTATICREGISTRY_REGISTER(R, C) \
OBJECTSTATICREGISTRY_REGISTER(R, C); \
STATICTABLE_REGISTER(R, C, C::GetObjectTag(), std::string, AddRequiredChannels)

#define SAMPLER_OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(R) \
OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(R); \
STATICTABLE_DECLARE_DECLARATION(R, std::string, AddRequiredChannels)

#define SAMPLER_OBJECTSTATICREGISTRY_STATICFIELDS(R) \
OBJECTSTATICREGISTRY_STATICFIELDS(R); \
STATICTABLE_DECLARATION(R, std::string, AddRequiredChannels)

class SamplerRegistry {
protected:
	SamplerRegistry() { }

	//--------------------------------------------------------------------------

	typedef SamplerType ObjectType;
	// Used to register all sub-class String2SamplerType() static methods
	typedef SamplerType (*GetObjectType)();
	// Used to register all sub-class SamplerType2String() static methods
	typedef std::string (*GetObjectTag)();
	// Used to register all sub-class ToProperties() static methods
	typedef luxrays::Properties (*ToProperties)(const luxrays::Properties &cfg);
	// Used to register all sub-class FromProperties() static methods
	typedef Sampler *(*FromProperties)(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	// Used to register all sub-class FromPropertiesOCL() static methods
	typedef slg::ocl::Sampler *(*FromPropertiesOCL)(const luxrays::Properties &cfg);
	// Used to register all sub-class AddRequiredChannels() static methods
	typedef void (*AddRequiredChannels)(Film::FilmChannels &channels, const luxrays::Properties &cfg);

	SAMPLER_OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(SamplerRegistry);

	//--------------------------------------------------------------------------

	SAMPLER_OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(SamplerRegistry, RandomSampler);
	SAMPLER_OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(SamplerRegistry, SobolSampler);
	SAMPLER_OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(SamplerRegistry, MetropolisSampler);
	SAMPLER_OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(SamplerRegistry, RTPathCPUSampler);
	SAMPLER_OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(SamplerRegistry, TilePathSampler);
	// Just add here any new Sampler (don't forget in the .cpp too)

	friend class Sampler;
};

}

#endif	/* _SLG_SAMPLERREGISTRY_H */
