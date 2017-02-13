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

#ifndef _SLG_TILEPATHOCL_SAMPLER_H
#define	_SLG_TILEPATHOCL_SAMPLER_H

#include <string>
#include <vector>

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"

namespace slg {

//------------------------------------------------------------------------------
// TilePathSamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class TilePathSamplerSharedData : public SamplerSharedData {
public:
	TilePathSamplerSharedData() { }
	virtual ~TilePathSamplerSharedData() { }

	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	// Nothing to share
};

//------------------------------------------------------------------------------
// TilePath sampler
//------------------------------------------------------------------------------

class TilePathSampler : public Sampler {
public:
	TilePathSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter) : Sampler(rnd, flm, flmSplatter) { }
	virtual ~TilePathSampler() { }

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const u_int size) { }

	// This code isn't really used
	virtual float GetSample(const u_int index) { return rndGen->floatValue(); }
	// This code isn't really used
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return TILEPATHSAMPLER; }
	static std::string GetObjectTag() { return "TILEPATHSAMPLER"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);

private:
	static const luxrays::Properties &GetDefaultProps();
};

}

#endif	/* _SLG_TILEPATHOCL_SAMPLER_H */
