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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/kernels/kernels.h"
#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void CompiledScene::CompilePathTracer() {
	compiledPathTracer.maxPathDepth.depth = pathTracer->maxPathDepth.depth;
	compiledPathTracer.maxPathDepth.diffuseDepth = pathTracer->maxPathDepth.diffuseDepth;
	compiledPathTracer.maxPathDepth.glossyDepth = pathTracer->maxPathDepth.glossyDepth;
	compiledPathTracer.maxPathDepth.specularDepth = pathTracer->maxPathDepth.specularDepth;
	
	compiledPathTracer.rrDepth = pathTracer->rrDepth;
	compiledPathTracer.rrImportanceCap = pathTracer->rrImportanceCap;
	
	compiledPathTracer.sqrtVarianceClampMaxValue = pathTracer->sqrtVarianceClampMaxValue;

	compiledPathTracer.hybridBackForward.enabled = pathTracer->hybridBackForwardEnable;
	compiledPathTracer.hybridBackForward.glossinessThreshold = pathTracer->hybridBackForwardGlossinessThreshold;
	
	compiledPathTracer.forceBlackBackground = pathTracer->forceBlackBackground;
	CompilePhotonGI();
}

#endif
