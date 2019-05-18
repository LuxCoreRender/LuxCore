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

#ifndef _SLG_LIGHTVISIBILITYCACHE_H
#define	_SLG_LIGHTVISIBILITYCACHE_H

#include <boost/atomic.hpp>

#include "luxrays/utils/mcdistribution.h"
#include "slg/slg.h"
#include "slg/scene/scene.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/metropolis.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// Env. Light visibility preprocessor
//------------------------------------------------------------------------------

typedef struct {
	const bool sampleUpperHemisphereOnly;
	const u_int width, height;
	const u_int sampleCount;
	const u_int maxDepth;
	const float glossinessUsageThreshold;
} ELVCParams;

class EnvLightVisibilityCache {
public:
	EnvLightVisibilityCache(const Scene *scene, const EnvLightSource *envLight,
			ImageMap *luminanceMapImage,
			const ELVCParams &params);
	virtual ~EnvLightVisibilityCache();

	bool IsCacheEnabled(const BSDF &bsdf) const;

	void Build();

private:
	float EvaluateBestRadius();

	const Scene *scene;
	const EnvLightSource *envLight;
	const ImageMap *luminanceMapImage;

	const ELVCParams params;
};

}

#endif	/* _SLG_LIGHTVISIBILITYCACHE_H */
