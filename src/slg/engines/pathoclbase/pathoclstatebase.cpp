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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>

#include "slg/slg.h"
#include "slg/engines/pathoclbase/pathoclstatebase.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLStateKernelBaseRenderEngine
//------------------------------------------------------------------------------

PathOCLStateKernelBaseRenderEngine::PathOCLStateKernelBaseRenderEngine(const RenderConfig *rcfg, Film *flm,
		boost::mutex *flmMutex) : PathOCLBaseRenderEngine(rcfg, flm, flmMutex) {
	usePixelAtomics = false;
	useFastPixelFilter = true;
	forceBlackBackground = false;

	pixelFilterDistribution = NULL;

	oclSampler = NULL;
	oclPixelFilter = NULL;
}

PathOCLStateKernelBaseRenderEngine::~PathOCLStateKernelBaseRenderEngine() {
	delete[] pixelFilterDistribution;
	delete oclSampler;
	delete oclPixelFilter;
}

void PathOCLStateKernelBaseRenderEngine::InitPixelFilterDistribution() {
	auto_ptr<Filter> pixelFilter(renderConfig->AllocPixelFilter());

	// Compile sample distribution
	delete[] pixelFilterDistribution;
	const FilterDistribution filterDistribution(pixelFilter.get(), 64);
	pixelFilterDistribution = CompiledScene::CompileDistribution2D(
			filterDistribution.GetDistribution2D(), &pixelFilterDistributionSize);
}

void PathOCLStateKernelBaseRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	oclSampler = Sampler::FromPropertiesOCL(cfg);

	//--------------------------------------------------------------------------
	// Filter
	//--------------------------------------------------------------------------

	oclPixelFilter = Filter::FromPropertiesOCL(cfg);

	if (useFastPixelFilter)
		InitPixelFilterDistribution();

	PathOCLBaseRenderEngine::StartLockLess();
}

void PathOCLStateKernelBaseRenderEngine::StopLockLess() {
	PathOCLBaseRenderEngine::StopLockLess();

	delete[] pixelFilterDistribution;
	pixelFilterDistribution = NULL;
}

#endif
