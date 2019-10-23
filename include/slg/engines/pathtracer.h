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

#ifndef _SLG_PATHTRACER_H
#define	_SLG_PATHTRACER_H

#include <boost/thread/mutex.hpp>

#include "slg/slg.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// Path Tracing render code
//
// All the methods of this class must be thread-safe because they will be used
// by render threads	
//------------------------------------------------------------------------------

class PathTracer;
class VarianceClamping;

class PathTracerThreadState {
public:
	PathTracerThreadState(const u_int threadIndex,
			luxrays::IntersectionDevice *device,
			Sampler *eyeSampler, Sampler *lightSampler,
			const Scene *scene, Film *film,
			const VarianceClamping *varianceClamping);
	virtual ~PathTracerThreadState();

	friend class PathTracer;

private:
	const u_int threadIndex;
	luxrays::IntersectionDevice *device;

	Sampler *eyeSampler, *lightSampler;
	const Scene *scene;
	Film *film;
	const VarianceClamping *varianceClamping;
	
	std::vector<SampleResult> eyeSampleResults, lightSampleResults;
	
	double eyeSampleCount, lightSampleCount;
};

class PhotonGICache;

class PathTracer {
public:
	PathTracer();
	virtual ~PathTracer();

	void InitPixelFilterDistribution(const Filter *pixelFilter);
	void DeletePixelFilterDistribution();

	void SetPhotonGICache(const PhotonGICache *cache) { photonGICache = cache; }

	void ParseOptions(const luxrays::Properties &cfg, const luxrays::Properties &defaultProps);

	void RenderEyeSample(const u_int threadIndex, luxrays::IntersectionDevice *device,
			const Scene *scene, const Film *film, Sampler *sampler,
			std::vector<SampleResult> &sampleResults) const;

	void RenderLightSample(const u_int threadIndex, luxrays::IntersectionDevice *device,
			const Scene *scene, const Film *film, Sampler *sampler,
			std::vector<SampleResult> &sampleResults) const;
	
	void RenderSample(PathTracerThreadState &state) const;

	static void InitEyeSampleResults(const Film *film, std::vector<SampleResult> &sampleResults);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	
	// Used for Sampler indices
	u_int eyeSampleBootSize, eyeSampleStepSize, eyeSampleSize;
	u_int lightSampleBootSize, lightSampleStepSize, lightSampleSize;

	// Used for Sampler indices (1D and 2D)
	u_int eyeSampleBootSize1D, eyeSampleStepSize1D, eyeSampleSize1D;
	u_int eyeSampleBootSize2D, eyeSampleStepSize2D, eyeSampleSize2D;
	u_int lightSampleBootSize1D, lightSampleStepSize1D, lightSampleSize1D;
	u_int lightSampleBootSize2D, lightSampleStepSize2D, lightSampleSize2D;
	std::vector<SampleSize> eyeSampleSizes, lightSampleSizes;

	// Path depth settings
	PathDepthInfo maxPathDepth;

	u_int rrDepth;
	float rrImportanceCap;

	// Clamping settings
	float sqrtVarianceClampMaxValue;

	// Hybrid backward/forward path tracing settings
	float hybridBackForwardPartition, hybridBackForwardGlossinessThreshold;

	// Option flags
	bool forceBlackBackground, hybridBackForwardEnable;

private:
	void GenerateEyeRay(const Camera *camera, const Film *film,
			luxrays::Ray &eyeRay, PathVolumeInfo &volInfo,
			Sampler *sampler, SampleResult &sampleResult) const;

	typedef enum {
		ILLUMINATED, SHADOWED, NOT_VISIBLE
	} DirectLightResult;

	// RenderEyeSample methods

	DirectLightResult DirectLightSampling(
		luxrays::IntersectionDevice *device, const Scene *scene,
		const float time, const float u0,
		const float u1, const float u2,
		const float u3, const float u4,
		const EyePathInfo &pathInfo, const luxrays::Spectrum &pathThrouput,
		const BSDF &bsdf, SampleResult *sampleResult) const;

	void DirectHitFiniteLight(const Scene *scene, const EyePathInfo &pathInfo,
			const luxrays::Spectrum &pathThrouput, const luxrays::Ray &ray,
			const float distance, const BSDF &bsdf,
			SampleResult *sampleResult) const;
	void DirectHitInfiniteLight(const Scene *scene, const EyePathInfo &pathInfo,
			const luxrays::Spectrum &pathThrouput, const luxrays::Ray &ray,
			const BSDF *bsdf, SampleResult *sampleResult) const;
	bool CheckDirectHitVisibilityFlags(const LightSource *lightSource,
			const PathDepthInfo &depthInfo,	const BSDFEvent lastBSDFEvent) const;

	// RenderLightSample methods

	SampleResult &AddLightSampleResult(std::vector<SampleResult> &sampleResults,
			const Film *film) const;
	void ConnectToEye(const u_int threadIndex,
			luxrays::IntersectionDevice *device, const Scene *scene,
			const Film *film, const float time,
			const float u0, const float u1, const float u2,
			const LightSource &light,  const BSDF &bsdf,
			const luxrays::Spectrum &flux, const LightPathInfo &pathInfo,
			std::vector<SampleResult> &sampleResults) const;

	FilterDistribution *pixelFilterDistribution;
	const PhotonGICache *photonGICache;	
};

}

#endif	/* _SLG_PATHTRACER_H */
