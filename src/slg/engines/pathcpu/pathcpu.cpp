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

#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/film/filters/filter.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathCPURenderEngine
//------------------------------------------------------------------------------

PathCPURenderEngine::PathCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUNoTileRenderEngine(rcfg, flm, flmMutex), pixelFilterDistribution(NULL), sampleSplatter(NULL) {
	InitFilm();
}

PathCPURenderEngine::~PathCPURenderEngine() {
	delete pixelFilterDistribution;
	delete sampleSplatter;
}

void PathCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

void PathCPURenderEngine::InitPixelFilterDistribution() {
	// Compile sample distribution
	delete pixelFilterDistribution;
	pixelFilterDistribution = new FilterDistribution(pixelFilter, 64);
}

void PathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxPathDepth = (u_int)Max(1, cfg.Get(GetDefaultProps().Get("path.maxdepth")).Get<int>());
	rrDepth = (u_int)Max(1, cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")).Get<float>(), 0.f, 1.f);

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("path.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("path.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(GetDefaultProps().Get("path.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);
	pdfClampValue = Max(0.f, cfg.Get(GetDefaultProps().Get("path.clamping.pdf.value")).Get<float>());

	useFastPixelFilter = cfg.Get(GetDefaultProps().Get("path.fastpixelfilter.enable")).Get<bool>();
	forceBlackBackground = cfg.Get(GetDefaultProps().Get("path.forceblackbackground.enable")).Get<bool>();

	//--------------------------------------------------------------------------

	delete sampleSplatter;
	sampleSplatter = NULL;
	if (useFastPixelFilter)
		InitPixelFilterDistribution();
	else
		sampleSplatter = new FilmSampleSplatter(pixelFilter);

	CPUNoTileRenderEngine::StartLockLess();
}

void PathCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	delete pixelFilterDistribution;
	pixelFilterDistribution = NULL;
	delete sampleSplatter;
	sampleSplatter = NULL;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties PathCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("path.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")) <<
			cfg.Get(GetDefaultProps().Get("path.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("path.clamping.pdf.value")) <<
			cfg.Get(GetDefaultProps().Get("path.fastpixelfilter.enable")) <<
			cfg.Get(GetDefaultProps().Get("path.forceblackbackground.enable")) <<
			Sampler::ToProperties(cfg);
}

RenderEngine *PathCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new PathCPURenderEngine(rcfg, flm, flmMutex);
}

const Properties &PathCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("path.maxdepth")(5) <<
			Property("path.russianroulette.depth")(3) <<
			Property("path.russianroulette.cap")(.5f) <<
			Property("path.clamping.variance.maxvalue")(0.f) <<
			Property("path.clamping.pdf.value")(0.f) <<
			Property("path.fastpixelfilter.enable")(true) <<
			Property("path.forceblackbackground.enable")(false);

	return props;
}
