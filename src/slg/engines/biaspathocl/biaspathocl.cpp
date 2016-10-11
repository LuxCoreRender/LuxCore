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

#include <boost/format.hpp>

#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/biaspathocl/biaspathocl.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathOCLRenderEngine
//------------------------------------------------------------------------------

BiasPathOCLRenderEngine::BiasPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm,
		boost::mutex *flmMutex) : PathOCLStateKernelBaseRenderEngine(rcfg, flm, flmMutex) {
	tileRepository = NULL;
}

BiasPathOCLRenderEngine::~BiasPathOCLRenderEngine() {
	delete tileRepository;
}

PathOCLBaseRenderThread *BiasPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new BiasPathOCLRenderThread(index, device, this);
}

void BiasPathOCLRenderEngine::InitTileRepository() {
	if (tileRepository)
		delete tileRepository;
	tileRepository = NULL;

	Properties tileProps = renderConfig->cfg.GetAllProperties("tile");
	if (GetType() == RTBIASPATHOCL) {
		tileProps.Delete("tile.size");

		// Check if I'm going to use a single device
		u_int tileWidth, tileHeight;
		if (intersectionDevices.size() == 1) {
			// The best configuration, with a single device, is to use a tile
			// as large as the complete image
			tileWidth = film->GetWidth();
			tileHeight = film->GetHeight();
		} else {
			// One slice for each device
			tileWidth = (film->GetWidth() + 1) / intersectionDevices.size();
			tileHeight = film->GetHeight();
		}

		// Tile width must be a multiple of RESOLUTION_REDUCTION to support RT variable resolution rendering
		RTBiasPathOCLRenderEngine *rtengine = (RTBiasPathOCLRenderEngine *)this;
		u_int rup = Max(rtengine->previewResolutionReduction, rtengine->resolutionReduction);
		tileWidth = RoundUp(tileWidth, rup);

		tileProps <<
				Property("tile.size.x")(tileWidth) <<
				Property("tile.size.y")(tileHeight);
	}

	tileRepository = TileRepository::FromProperties(tileProps);
	if (GetType() == RTBIASPATHOCL)
		tileRepository->enableMultipassRendering = false;
	tileRepository->varianceClamping = VarianceClamping(sqrtVarianceClampMaxValue);
	tileRepository->InitTiles(*film);

	if (GetType() == RTBIASPATHOCL) {
		// Check the maximum number of task to execute. I have to
		// consider preview and normal phase

		const u_int tileWidth = tileRepository->tileWidth;
		const u_int tileHeight = tileRepository->tileHeight;
		const u_int threadFilmPixelCount = tileWidth * tileHeight;

		RTBiasPathOCLRenderEngine *rtengine = (RTBiasPathOCLRenderEngine *)this;
		taskCount = threadFilmPixelCount / Sqr(rtengine->previewResolutionReduction);
		taskCount = Max(taskCount, threadFilmPixelCount / Sqr(rtengine->resolutionReduction));
	} else
		taskCount = tileRepository->tileWidth * tileRepository->tileHeight * aaSamples * aaSamples;

	// I don't know yet the workgroup size of each device so I can not
	// round up task count to be a multiple of workgroups size of all devices
	// used. Rounding to 8192 is a simple trick based on the assumption that
	// workgroup size is a power of 2 and <= 8192.
	taskCount = RoundUp<u_int>(taskCount, 8192);
	//SLG_LOG("[BiasPathOCLRenderEngine] OpenCL task count: " << taskCount);
}

void BiasPathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
	if (samplerType != "BIASPATHSAMPLER")
		throw runtime_error("(RT)BIASPATHOCL render engine can use only BIASPATHSAMPLER");

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	Properties defaultProps = (GetType() == BIASPATHOCL) ?
		BiasPathOCLRenderEngine::GetDefaultProps() :
		RTBiasPathOCLRenderEngine::GetDefaultProps();

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.total")).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.diffuse")).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.glossy")).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.specular")).Get<int>());

	// For compatibility with the past
	if (cfg.IsDefined("biaspath.maxdepth") &&
			!cfg.IsDefined("biaspath.pathdepth.total") &&
			!cfg.IsDefined("biaspath.pathdepth.diffuse") &&
			!cfg.IsDefined("biaspath.pathdepth.glossy") &&
			!cfg.IsDefined("biaspath.pathdepth.specular")) {
		const u_int maxDepth = Max(0, cfg.Get("biaspath.maxdepth").Get<int>());
		maxPathDepth.depth = maxDepth;
		maxPathDepth.diffuseDepth = maxDepth;
		maxPathDepth.glossyDepth = maxDepth;
		maxPathDepth.specularDepth = maxDepth;
	}

	// Samples settings
	aaSamples = (GetType() == BIASPATHOCL) ?
		Max(1, cfg.Get(defaultProps.Get("biaspath.sampling.aa.size")).Get<int>()) :
		1;

	// Russian Roulette settings
	rrDepth = (u_int)Max(1, cfg.Get(defaultProps.Get("biaspath.russianroulette.depth")).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(defaultProps.Get("biaspath.russianroulette.cap")).Get<float>(), 0.f, 1.f);

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("biaspath.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("biaspath.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(defaultProps.Get("biaspath.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);
	pdfClampValue = Max(0.f, cfg.Get(defaultProps.Get("biaspath.clamping.pdf.value")).Get<float>());

	usePixelAtomics = true;
	forceBlackBackground = cfg.Get(GetDefaultProps().Get("biaspath.forceblackbackground.enable")).Get<bool>();

	maxTilePerDevice = cfg.Get(Property("biaspathocl.devices.maxtiles")(16)).Get<u_int>();

	film->Reset();

	//--------------------------------------------------------------------------
	// Tile related parameters
	//--------------------------------------------------------------------------

	InitTileRepository();
	
	PathOCLStateKernelBaseRenderEngine::StartLockLess();
}

void BiasPathOCLRenderEngine::StopLockLess() {
	PathOCLBaseRenderEngine::StopLockLess();

	delete tileRepository;
	tileRepository = NULL;
}

void BiasPathOCLRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	if (GetType() != RTBIASPATHOCL) {
		// RTBIASPATHOCL will InitTiles() on next frame
		tileRepository->Clear();
		tileRepository->InitTiles(*film);
	}

	PathOCLBaseRenderEngine::EndSceneEditLockLess(editActions);
}

void BiasPathOCLRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		totalCount += intersectionDevices[i]->GetTotalRaysCount();
	raysCount = totalCount;

	if (!tileRepository->done) {
		// Update the time only while rendering is not finished
		elapsedTime = WallClockTime() - startTime;
	} else
		convergence = 1.f;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BiasPathOCLRenderEngine::ToProperties(const Properties &cfg) {
	return OCLRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.russianroulette.cap")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.clamping.pdf.value")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.forceblackbackground.enable")) <<
			cfg.Get(GetDefaultProps().Get("biaspathocl.devices.maxtiles")) <<
			TileRepository::ToProperties(cfg) <<
			Sampler::ToProperties(cfg);
}

RenderEngine *BiasPathOCLRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new BiasPathOCLRenderEngine(rcfg, flm, flmMutex);
}

const Properties &BiasPathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			OCLRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("biaspath.pathdepth.total")(5) <<
			Property("biaspath.pathdepth.diffuse")(4) <<
			Property("biaspath.pathdepth.glossy")(3) <<
			Property("biaspath.pathdepth.specular")(3) <<
			Property("biaspath.sampling.aa.size")(3) <<
			Property("biaspath.russianroulette.depth")(3) <<
			Property("biaspath.russianroulette.cap")(.5f) <<
			Property("biaspath.clamping.variance.maxvalue")(0.f) <<
			Property("biaspath.clamping.pdf.value")(0.f) <<
			Property("biaspath.forceblackbackground.enable")(false) <<
			Property("biaspathocl.devices.maxtiles")(16) <<
			TileRepository::GetDefaultProps();

	return props;
}

#endif
