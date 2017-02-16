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

#ifndef _SLG_PATHTRACER_H
#define	_SLG_PATHTRACER_H

#include "slg/slg.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// Path Tracing render code
//
// All the methods of this class must be thread-safe because they will be used
// by render threads	
//------------------------------------------------------------------------------

class PathTracer {
public:
	PathTracer();
	virtual ~PathTracer();

	void InitPixelFilterDistribution(const Filter *pixelFilter);
	void DeletePixelFilterDistribution();

	void ParseOptions(const luxrays::Properties &cfg, const luxrays::Properties &defaultProps);

	void InitSampleResults(const Film *film, vector<SampleResult> &sampleResults) const;
	void RenderSample(luxrays::IntersectionDevice *device, const Scene *scene,
			const Film *film, Sampler *sampler, vector<SampleResult> &sampleResults) const;

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	
	// Used for Sampler indices
	u_int sampleBootSize, sampleStepSize, sampleSize;

	// Path depth settings
	PathDepthInfo maxPathDepth;

	u_int rrDepth;
	float rrImportanceCap;

	// Clamping settings
	float sqrtVarianceClampMaxValue;
	float pdfClampValue;

	bool forceBlackBackground;

private:
	void GenerateEyeRay(const Camera *camera, const Film *film,
			luxrays::Ray &eyeRay, Sampler *sampler, SampleResult &sampleResult) const;

	bool DirectLightSampling(
		luxrays::IntersectionDevice *device, const Scene *scene,
		const float time, const float u0,
		const float u1, const float u2,
		const float u3, const float u4,
		const luxrays::Spectrum &pathThrouput, const BSDF &bsdf,
		PathVolumeInfo volInfo, const u_int depth,
		SampleResult *sampleResult) const;

	void DirectHitFiniteLight(const Scene *scene, 
			const BSDFEvent lastBSDFEvent, const luxrays::Spectrum &pathThrouput,
			const float distance, const BSDF &bsdf, const float lastPdfW,
			SampleResult *sampleResult) const;
	void DirectHitInfiniteLight(const Scene *scene,
			const BSDFEvent lastBSDFEvent, const luxrays::Spectrum &pathThrouput,
			const luxrays::Vector &eyeDir, const float lastPdfW,
			SampleResult *sampleResult) const;

	FilterDistribution *pixelFilterDistribution;
};

}

#endif	/* _SLG_PATHTRACER_H */
