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

#ifndef _SLG_RTPATHCPU_SAMPLER_H
#define	_SLG_RTPATHCPU_SAMPLER_H

#include <string>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/thread/barrier.hpp>

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"

namespace slg {

//------------------------------------------------------------------------------
// RTPathCPU specific sampler data
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------
	
class RTPathCPUSamplerSharedData : public SamplerSharedData {
public:
	struct PixelCoord {
		u_int x, y;
	};

	RTPathCPUSamplerSharedData(Film *flm);
	virtual ~RTPathCPUSamplerSharedData() { }

	void Reset(Film *flm);

	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	boost::atomic<u_int> step;
	u_int filmWidth, filmHeight;
	std::vector<PixelCoord> pixelRenderSequence;
};

//------------------------------------------------------------------------------
// RTPathCPU specific sampler
//------------------------------------------------------------------------------

class RTPathCPURenderEngine;

class RTPathCPUSampler : public Sampler {
public:
	RTPathCPUSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter,
			RTPathCPUSamplerSharedData *samplerSharedData);
	virtual ~RTPathCPUSampler();

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const u_int size) { }

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	void SetRenderEngine(RTPathCPURenderEngine *engine);
	void Reset(Film *flm);

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return RTPATHCPUSAMPLER; }
	static std::string GetObjectTag() { return "RTPATHCPUSAMPLER"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);

private:
	static const luxrays::Properties &GetDefaultProps();

	void NextPixel();

	RTPathCPUSamplerSharedData *sharedData;
	RTPathCPURenderEngine *engine;

	u_int myStep, frameHeight;
	u_int currentX, currentY, linesDone;
	bool firstFrameDone;
};

}

#endif	/* _SLG_RTPATHCPU_SAMPLER_H */
