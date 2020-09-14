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

#include <limits>
#include <algorithm>
#include <exception>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "slg/film/film.h"
#include "slg/film/sampleresult.h"
#include "slg/utils/varianceclamping.h"
#include "slg/film/denoiser/filmdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film::Film() : filmDenoiser(this) {
	initialized = false;

	width = 0;
	height = 0;
	subRegion[0] = 0;
	subRegion[1] = 0;
	subRegion[2] = 0;
	subRegion[3] = 0;
	radianceGroupCount = 0;

	channel_ALPHA = nullptr;
	channel_DEPTH = nullptr;
	channel_POSITION = nullptr;
	channel_GEOMETRY_NORMAL = nullptr;
	channel_SHADING_NORMAL = nullptr;
	channel_MATERIAL_ID = nullptr;
	channel_DIRECT_DIFFUSE = nullptr;
	channel_DIRECT_DIFFUSE_REFLECT = nullptr;
	channel_DIRECT_DIFFUSE_TRANSMIT = nullptr;
	channel_DIRECT_GLOSSY = nullptr;
	channel_DIRECT_GLOSSY_REFLECT = nullptr;
	channel_DIRECT_GLOSSY_TRANSMIT = nullptr;
	channel_EMISSION = nullptr;
	channel_INDIRECT_DIFFUSE = nullptr;
	channel_INDIRECT_DIFFUSE_REFLECT = nullptr;
	channel_INDIRECT_DIFFUSE_TRANSMIT = nullptr;
	channel_INDIRECT_GLOSSY = nullptr;
	channel_INDIRECT_GLOSSY_REFLECT = nullptr;
	channel_INDIRECT_GLOSSY_TRANSMIT = nullptr;
	channel_INDIRECT_SPECULAR = nullptr;
	channel_INDIRECT_SPECULAR_REFLECT = nullptr;
	channel_INDIRECT_SPECULAR_TRANSMIT = nullptr;
	channel_DIRECT_SHADOW_MASK = nullptr;
	channel_INDIRECT_SHADOW_MASK = nullptr;
	channel_UV = nullptr;
	channel_RAYCOUNT = nullptr;
	channel_IRRADIANCE = nullptr;
	channel_OBJECT_ID = nullptr;
	channel_SAMPLECOUNT = nullptr;
	channel_CONVERGENCE = nullptr;
	channel_MATERIAL_ID_COLOR = nullptr;
	channel_ALBEDO = nullptr;
	channel_AVG_SHADING_NORMAL = nullptr;
	channel_NOISE = nullptr;
	channel_USER_IMPORTANCE = nullptr;

	convTest = nullptr;
	noiseEstimation = nullptr;
	haltTime = 0.0;
	haltSPP = 0;
	haltSPP_PixelNormalized = 0;
	haltSPP_ScreenNormalized = 0;
	haltNoiseThreshold = 0.f;

	isAsyncImagePipelineRunning = false;
	imagePipelineThread = nullptr;

	// Initialize variables to nullptr
	SetUpHW();
}

Film::Film(const u_int w, const u_int h, const u_int *sr) : filmDenoiser(this) {
	if ((w == 0) || (h == 0))
		throw runtime_error("Film can not have 0 width or a height");

	initialized = false;

	width = w;
	height = h;
	if (sr) {
		subRegion[0] = sr[0];
		subRegion[1] = sr[1];
		subRegion[2] = sr[2];
		subRegion[3] = sr[3];
	} else {
		subRegion[0] = 0;
		subRegion[1] = w - 1;
		subRegion[2] = 0;
		subRegion[3] = h - 1;
	}
	radianceGroupCount = 1;

	channel_ALPHA = nullptr;
	channel_DEPTH = nullptr;
	channel_POSITION = nullptr;
	channel_GEOMETRY_NORMAL = nullptr;
	channel_SHADING_NORMAL = nullptr;
	channel_MATERIAL_ID = nullptr;
	channel_DIRECT_DIFFUSE = nullptr;
	channel_DIRECT_DIFFUSE_REFLECT = nullptr;
	channel_DIRECT_DIFFUSE_TRANSMIT = nullptr;
	channel_DIRECT_GLOSSY = nullptr;
	channel_DIRECT_GLOSSY_REFLECT = nullptr;
	channel_DIRECT_GLOSSY_TRANSMIT = nullptr;
	channel_EMISSION = nullptr;
	channel_INDIRECT_DIFFUSE = nullptr;
	channel_INDIRECT_DIFFUSE_REFLECT = nullptr;
	channel_INDIRECT_DIFFUSE_TRANSMIT = nullptr;
	channel_INDIRECT_GLOSSY = nullptr;
	channel_INDIRECT_GLOSSY_REFLECT = nullptr;
	channel_INDIRECT_GLOSSY_TRANSMIT = nullptr;
	channel_INDIRECT_SPECULAR = nullptr;
	channel_INDIRECT_SPECULAR_REFLECT = nullptr;
	channel_INDIRECT_SPECULAR_TRANSMIT = nullptr;
	channel_DIRECT_SHADOW_MASK = nullptr;
	channel_INDIRECT_SHADOW_MASK = nullptr;
	channel_UV = nullptr;
	channel_RAYCOUNT = nullptr;
	channel_IRRADIANCE = nullptr;
	channel_OBJECT_ID = nullptr;
	channel_SAMPLECOUNT = nullptr;
	channel_CONVERGENCE = nullptr;
	channel_MATERIAL_ID_COLOR = nullptr;
	channel_ALBEDO = nullptr;
	channel_AVG_SHADING_NORMAL = nullptr;
	channel_NOISE = nullptr;
	channel_USER_IMPORTANCE = nullptr;

	convTest = nullptr;
	noiseEstimation = nullptr;
	haltTime = 0.0;

	haltSPP = 0;
	haltSPP_PixelNormalized = 0;
	haltSPP_ScreenNormalized = 0;

	// Noise halt threshold related variables
	haltNoiseThreshold = .02f;
	haltNoiseThresholdWarmUp = 64;
	haltNoiseThresholdTestStep = 64;
	haltNoiseThresholdUseFilter = true;
	haltNoiseThresholdStopRendering = true;
	haltNoiseThresholdImagePipelineIndex = 0;
	
	// Adaptive sampling related variables
	noiseEstimationWarmUp = 32;
	noiseEstimationTestStep = 32;
	noiseEstimationFilterScale = 4;
	noiseEstimationImagePipelineIndex = 0;

	isAsyncImagePipelineRunning = false;
	imagePipelineThread = nullptr;

	// Initialize variables to nullptr
	SetUpHW();
}
#include <slg/film/imagepipeline/plugins/optixdenoiser.h>
Film::~Film() {
	if (imagePipelineThread) {
		imagePipelineThread->interrupt();
		imagePipelineThread->join();
		delete imagePipelineThread;
	}

	// The image pipeline plugin destructor can use the hardware device to free
	// some memory so I have to set the current context
	if (hardwareDevice)
		hardwareDevice->PushThreadCurrentDevice();
		
	BOOST_FOREACH(ImagePipeline *ip, imagePipelines)
		delete ip;

	if (hardwareDevice)
		hardwareDevice->PopThreadCurrentDevice();

	// I have to delete the OCL context after the image pipeline because it
	// can be used by plugins
	DeleteHWContext();

	delete convTest;
	delete noiseEstimation;

	FreeChannels();
}

void Film::CopyDynamicSettings(const Film &film) {
	channels = film.channels;
	maskMaterialIDs = film.maskMaterialIDs;
	byMaterialIDs = film.byMaterialIDs;
	maskObjectIDs = film.maskObjectIDs;
	byObjectIDs = film.byObjectIDs;
	radianceGroupCount = film.radianceGroupCount;

	// Copy the image pipeline
	imagePipelines.resize(0);
	BOOST_FOREACH(ImagePipeline *ip, film.imagePipelines)
		imagePipelines.push_back(ip->Copy());

	filmDenoiser.SetEnabled(film.filmDenoiser.IsEnabled());
}

void Film::CopyHaltSettings(const Film &film) {
	haltTime = film.haltTime;
	haltSPP = film.haltSPP;
	haltSPP_PixelNormalized = film.haltSPP_PixelNormalized;
	haltSPP_ScreenNormalized = film.haltSPP_ScreenNormalized;
	
	haltNoiseThreshold = film.haltNoiseThreshold;
	haltNoiseThresholdWarmUp = film.haltNoiseThresholdWarmUp;
	haltNoiseThresholdTestStep = film.haltNoiseThresholdTestStep;
	haltNoiseThresholdImagePipelineIndex = film.haltNoiseThresholdImagePipelineIndex;
	
	haltNoiseThresholdUseFilter = film.haltNoiseThresholdUseFilter;
	haltNoiseThresholdStopRendering = film.haltNoiseThresholdStopRendering;

	noiseEstimationWarmUp = film.noiseEstimationWarmUp;
	noiseEstimationTestStep = film.noiseEstimationTestStep;
	noiseEstimationFilterScale = film.noiseEstimationFilterScale;
	noiseEstimationImagePipelineIndex = film.noiseEstimationImagePipelineIndex;

	if (film.convTest) {
		delete convTest;
		convTest = nullptr;

		convTest = new FilmConvTest(this, haltNoiseThreshold, haltNoiseThresholdWarmUp,
				haltNoiseThresholdTestStep, haltNoiseThresholdUseFilter,
				haltNoiseThresholdImagePipelineIndex);
	}
	
	if (film.noiseEstimation) {
		delete noiseEstimation;
		noiseEstimation = nullptr;

		noiseEstimation = new FilmNoiseEstimation(this, noiseEstimationWarmUp,
				noiseEstimationTestStep, noiseEstimationFilterScale, noiseEstimationImagePipelineIndex);
	}
}

void Film::SetThreadCount(const u_int threadCount) {
	if (initialized)
		throw runtime_error("The thread count of a Film can not be initialized after Film::Init()");

	samplesCounts.Init(threadCount);
}

void Film::Init() {
	if (initialized)
		throw runtime_error("A Film can not be initialized multiple times");

	if (imagePipelines.size() > 0)
		AddChannel(IMAGEPIPELINE);

	// CONVERGENCE is generated by the convergence test
	if (HasChannel(CONVERGENCE) && !convTest) {
		// The test has to be enabled to update the CONVERGENCE AOV

		// Using the default values
		convTest = new FilmConvTest(this, haltNoiseThreshold, haltNoiseThresholdWarmUp,
				haltNoiseThresholdTestStep, haltNoiseThresholdUseFilter,
				haltNoiseThresholdImagePipelineIndex);
	}

	// NOISE is generated by the noise estimation test
	if (HasChannel(NOISE) && !noiseEstimation)
		noiseEstimation = new FilmNoiseEstimation(this, noiseEstimationWarmUp, noiseEstimationTestStep, noiseEstimationFilterScale, noiseEstimationImagePipelineIndex);

	initialized = true;

	Resize(width, height);
}

void Film::SetSampleCount(const double totalSampleCount,
		const double RADIANCE_PER_PIXEL_NORMALIZED_count,
		const double RADIANCE_PER_SCREEN_NORMALIZED_count) {
	samplesCounts.SetSampleCount(totalSampleCount,
			RADIANCE_PER_PIXEL_NORMALIZED_count,
			RADIANCE_PER_SCREEN_NORMALIZED_count);

	// Check the if Film denoiser warmup is done
	if (filmDenoiser.IsEnabled() && !filmDenoiser.HasReferenceFilm() &&
			!filmDenoiser.IsWarmUpDone())
		filmDenoiser.CheckIfWarmUpDone();
}

void Film::AddSampleCount(const u_int threadIndex,
		const double RADIANCE_PER_PIXEL_NORMALIZED_count,
		const double RADIANCE_PER_SCREEN_NORMALIZED_count) {
	samplesCounts.AddSampleCount(threadIndex,
			RADIANCE_PER_PIXEL_NORMALIZED_count,
			RADIANCE_PER_SCREEN_NORMALIZED_count);
}

void Film::Resize(const u_int w, const u_int h) {
	if ((w == 0) || (h == 0))
		throw runtime_error("Film can not have 0 width or a height");

	width = w;
	height = h;
	pixelCount = w * h;

	// Delete all already allocated channels
	FreeChannels();

	// Allocate all required channels
	hasDataChannel = false;
	hasComposingChannel = false;
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		channel_RADIANCE_PER_PIXEL_NORMALIZEDs.resize(radianceGroupCount, nullptr);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i] = new GenericFrameBuffer<4, 1, float>(width, height);
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->Clear();
		}
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		channel_RADIANCE_PER_SCREEN_NORMALIZEDs.resize(radianceGroupCount, nullptr);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i] = new GenericFrameBuffer<3, 0, float>(width, height);
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->Clear();
		}
	}

	// Update the radiance group count of all image pipelines
	BOOST_FOREACH(ImagePipeline *ip, imagePipelines)
		ip->SetRadianceGroupCount(radianceGroupCount);

	if (HasChannel(ALPHA)) {
		channel_ALPHA = new GenericFrameBuffer<2, 1, float>(width, height);
		channel_ALPHA->Clear();
	}
	if (HasChannel(IMAGEPIPELINE)) {
		channel_IMAGEPIPELINEs.resize(imagePipelines.size(), nullptr);
		for (u_int i = 0; i < channel_IMAGEPIPELINEs.size(); ++i) {
			channel_IMAGEPIPELINEs[i] = new GenericFrameBuffer<3, 0, float>(width, height);
			channel_IMAGEPIPELINEs[i]->Clear();
		}

		// Resize the converge test too if required
		if (convTest)
			convTest->Reset();

		// Resize the noise estimation too if required
		if (noiseEstimation)
			noiseEstimation->Reset();
	} else {
		delete convTest;
		convTest = nullptr;
		
		delete noiseEstimation;
		noiseEstimation = nullptr;
	}
	if (HasChannel(DEPTH)) {
		channel_DEPTH = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_DEPTH->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(POSITION)) {
		channel_POSITION = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_POSITION->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(GEOMETRY_NORMAL)) {
		channel_GEOMETRY_NORMAL = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_GEOMETRY_NORMAL->Clear();
		hasDataChannel = true;
	}
	if (HasChannel(SHADING_NORMAL)) {
		channel_SHADING_NORMAL = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_SHADING_NORMAL->Clear();
		hasDataChannel = true;
	}
	if (HasChannel(MATERIAL_ID)) {
		channel_MATERIAL_ID = new GenericFrameBuffer<1, 0, u_int>(width, height);
		channel_MATERIAL_ID->Clear(numeric_limits<u_int>::max());
		hasDataChannel = true;
	}
	if (HasChannel(DIRECT_DIFFUSE)) {
		channel_DIRECT_DIFFUSE = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_DIFFUSE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_DIFFUSE_REFLECT)) {
		channel_DIRECT_DIFFUSE_REFLECT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_DIFFUSE_REFLECT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_DIFFUSE_TRANSMIT)) {
		channel_DIRECT_DIFFUSE_TRANSMIT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_DIFFUSE_TRANSMIT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_GLOSSY)) {
		channel_DIRECT_GLOSSY = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_GLOSSY_REFLECT)) {
		channel_DIRECT_GLOSSY_REFLECT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_GLOSSY_REFLECT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_GLOSSY_TRANSMIT)) {
		channel_DIRECT_GLOSSY_TRANSMIT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_GLOSSY_TRANSMIT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(EMISSION)) {
		channel_EMISSION = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_EMISSION->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_DIFFUSE)) {
		channel_INDIRECT_DIFFUSE = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_DIFFUSE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_DIFFUSE_REFLECT)) {
		channel_INDIRECT_DIFFUSE_REFLECT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_DIFFUSE_REFLECT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_DIFFUSE_TRANSMIT)) {
		channel_INDIRECT_DIFFUSE_TRANSMIT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_DIFFUSE_TRANSMIT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_GLOSSY)) {
		channel_INDIRECT_GLOSSY = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_GLOSSY_REFLECT)) {
		channel_INDIRECT_GLOSSY_REFLECT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_GLOSSY_REFLECT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_GLOSSY_TRANSMIT)) {
		channel_INDIRECT_GLOSSY_TRANSMIT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_GLOSSY_TRANSMIT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SPECULAR)) {
		channel_INDIRECT_SPECULAR = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_SPECULAR->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SPECULAR_REFLECT)) {
		channel_INDIRECT_SPECULAR_REFLECT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_SPECULAR_REFLECT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SPECULAR_TRANSMIT)) {
		channel_INDIRECT_SPECULAR_TRANSMIT = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_SPECULAR_TRANSMIT->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
			GenericFrameBuffer<2, 1, float> *buf = new GenericFrameBuffer<2, 1, float>(width, height);
			buf->Clear();
			channel_MATERIAL_ID_MASKs.push_back(buf);
		}
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_SHADOW_MASK)) {
		channel_DIRECT_SHADOW_MASK = new GenericFrameBuffer<2, 1, float>(width, height);
		channel_DIRECT_SHADOW_MASK->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SHADOW_MASK)) {
		channel_INDIRECT_SHADOW_MASK = new GenericFrameBuffer<2, 1, float>(width, height);
		channel_INDIRECT_SHADOW_MASK->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(UV)) {
		channel_UV = new GenericFrameBuffer<2, 0, float>(width, height);
		channel_UV->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(RAYCOUNT)) {
		channel_RAYCOUNT = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_RAYCOUNT->Clear();
		hasDataChannel = true;
	}
	if (HasChannel(BY_MATERIAL_ID)) {
		for (u_int i = 0; i < byMaterialIDs.size(); ++i) {
			GenericFrameBuffer<4, 1, float> *buf = new GenericFrameBuffer<4, 1, float>(width, height);
			buf->Clear();
			channel_BY_MATERIAL_IDs.push_back(buf);
		}
		hasComposingChannel = true;
	}
	if (HasChannel(IRRADIANCE)) {
		channel_IRRADIANCE = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_IRRADIANCE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(OBJECT_ID)) {
		channel_OBJECT_ID = new GenericFrameBuffer<1, 0, u_int>(width, height);
		channel_OBJECT_ID->Clear(numeric_limits<u_int>::max());
		hasDataChannel = true;
	}
	if (HasChannel(OBJECT_ID_MASK)) {
		for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
			GenericFrameBuffer<2, 1, float> *buf = new GenericFrameBuffer<2, 1, float>(width, height);
			buf->Clear();
			channel_OBJECT_ID_MASKs.push_back(buf);
		}
		hasComposingChannel = true;
	}
	if (HasChannel(BY_OBJECT_ID)) {
		for (u_int i = 0; i < byObjectIDs.size(); ++i) {
			GenericFrameBuffer<4, 1, float> *buf = new GenericFrameBuffer<4, 1, float>(width, height);
			buf->Clear();
			channel_BY_OBJECT_IDs.push_back(buf);
		}
		hasComposingChannel = true;
	}
	if (HasChannel(SAMPLECOUNT)) {
		channel_SAMPLECOUNT = new GenericFrameBuffer<1, 0, u_int>(width, height);
		channel_SAMPLECOUNT->Clear();
		hasDataChannel = true;
	}
	if (HasChannel(CONVERGENCE)) {
		channel_CONVERGENCE = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_CONVERGENCE->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(MATERIAL_ID_COLOR)) {
		channel_MATERIAL_ID_COLOR = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_MATERIAL_ID_COLOR->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(ALBEDO)) {
		channel_ALBEDO = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_ALBEDO->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(AVG_SHADING_NORMAL)) {
		channel_AVG_SHADING_NORMAL = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_AVG_SHADING_NORMAL->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(NOISE)) {
		channel_NOISE = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_NOISE->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(USER_IMPORTANCE)) {
		channel_USER_IMPORTANCE = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_USER_IMPORTANCE->Clear(1.f);
		hasDataChannel = true;
	}

	// Reset BCD statistics accumulator (I need to redo the warmup period)
	filmDenoiser.Reset();

	// Initialize the statistics
	samplesCounts.Clear();
	statsConvergence = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::Clear() {
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i)
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->Clear();
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i)
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->Clear();
	}
	if (HasChannel(ALPHA))
		channel_ALPHA->Clear();
	if (HasChannel(DEPTH))
		channel_DEPTH->Clear(numeric_limits<float>::infinity());
	if (HasChannel(POSITION))
		channel_POSITION->Clear(numeric_limits<float>::infinity());
	if (HasChannel(GEOMETRY_NORMAL))
		channel_GEOMETRY_NORMAL->Clear(numeric_limits<float>::infinity());
	if (HasChannel(SHADING_NORMAL))
		channel_SHADING_NORMAL->Clear(numeric_limits<float>::infinity());
	if (HasChannel(MATERIAL_ID))
		channel_MATERIAL_ID->Clear(numeric_limits<u_int>::max());
	if (HasChannel(DIRECT_DIFFUSE))
		channel_DIRECT_DIFFUSE->Clear();
	if (HasChannel(DIRECT_DIFFUSE_REFLECT))
		channel_DIRECT_DIFFUSE_REFLECT->Clear();
	if (HasChannel(DIRECT_DIFFUSE_TRANSMIT))
		channel_DIRECT_DIFFUSE_TRANSMIT->Clear();
	if (HasChannel(DIRECT_GLOSSY))
		channel_DIRECT_GLOSSY->Clear();
	if (HasChannel(DIRECT_GLOSSY_REFLECT))
		channel_DIRECT_GLOSSY_REFLECT->Clear();
	if (HasChannel(DIRECT_GLOSSY_TRANSMIT))
		channel_DIRECT_GLOSSY_TRANSMIT->Clear();
	if (HasChannel(EMISSION))
		channel_EMISSION->Clear();
	if (HasChannel(INDIRECT_DIFFUSE))
		channel_INDIRECT_DIFFUSE->Clear();
	if (HasChannel(INDIRECT_DIFFUSE_REFLECT))
		channel_INDIRECT_DIFFUSE_REFLECT->Clear();
	if (HasChannel(INDIRECT_DIFFUSE_TRANSMIT))
		channel_INDIRECT_DIFFUSE_TRANSMIT->Clear();
	if (HasChannel(INDIRECT_GLOSSY))
		channel_INDIRECT_GLOSSY->Clear();
	if (HasChannel(INDIRECT_GLOSSY_REFLECT))
		channel_INDIRECT_GLOSSY_REFLECT->Clear();
	if (HasChannel(INDIRECT_GLOSSY_TRANSMIT))
		channel_INDIRECT_GLOSSY_TRANSMIT->Clear();
	if (HasChannel(INDIRECT_SPECULAR))
		channel_INDIRECT_SPECULAR->Clear();
	if (HasChannel(INDIRECT_SPECULAR_REFLECT))
		channel_INDIRECT_SPECULAR_REFLECT->Clear();
	if (HasChannel(INDIRECT_SPECULAR_TRANSMIT))
		channel_INDIRECT_SPECULAR_TRANSMIT->Clear();
	if (HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < channel_MATERIAL_ID_MASKs.size(); ++i)
			channel_MATERIAL_ID_MASKs[i]->Clear();
	}
	if (HasChannel(DIRECT_SHADOW_MASK))
		channel_DIRECT_SHADOW_MASK->Clear();
	if (HasChannel(INDIRECT_SHADOW_MASK))
		channel_INDIRECT_SHADOW_MASK->Clear();
	if (HasChannel(UV))
		channel_UV->Clear();
	if (HasChannel(RAYCOUNT))
		channel_RAYCOUNT->Clear();
	if (HasChannel(BY_MATERIAL_ID)) {
		for (u_int i = 0; i < channel_BY_MATERIAL_IDs.size(); ++i)
			channel_BY_MATERIAL_IDs[i]->Clear();
	}
	if (HasChannel(IRRADIANCE))
		channel_IRRADIANCE->Clear();
	if (HasChannel(OBJECT_ID))
		channel_OBJECT_ID->Clear(numeric_limits<u_int>::max());
	if (HasChannel(OBJECT_ID_MASK)) {
		for (u_int i = 0; i < channel_OBJECT_ID_MASKs.size(); ++i)
			channel_OBJECT_ID_MASKs[i]->Clear();
	}
	if (HasChannel(BY_OBJECT_ID)) {
		for (u_int i = 0; i < channel_BY_OBJECT_IDs.size(); ++i)
			channel_BY_OBJECT_IDs[i]->Clear();
	}
	if (HasChannel(SAMPLECOUNT))
		channel_SAMPLECOUNT->Clear();
	// channel_CONVERGENCE, channel_NOISE and channel_USER_IMPORTANCE are not
	// cleared otherwise the result of the halt test and adaptive sampling
	// would be lost
	if (HasChannel(MATERIAL_ID_COLOR))
		channel_MATERIAL_ID_COLOR->Clear();
	if (HasChannel(ALBEDO))
		channel_ALBEDO->Clear();
	if (HasChannel(AVG_SHADING_NORMAL))
		channel_AVG_SHADING_NORMAL->Clear();

	// denoiser is not cleared otherwise the collected data would be lost

	samplesCounts.Clear();
	// statsConvergence is not cleared otherwise the result of the halt test
	// would be lost
}

void Film::Reset(const bool onlyCounters) {
	if (!onlyCounters)
		Clear();

	// denoiser has to be reset explicitly

	// convTest has to be reset explicitly

	samplesCounts.Clear();
	statsConvergence = 0.0;
	statsStartSampleTime = WallClockTime();
}

template <bool overwrite>
void Film::AddFilmImpl(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	const double additional_SampleCount = film.samplesCounts.GetSampleCount();
	double additional_RADIANCE_PER_PIXEL_NORMALIZED_SampleCount = 0;
	double additional_RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = 0;

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && film.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		additional_RADIANCE_PER_PIXEL_NORMALIZED_SampleCount = film.samplesCounts.GetSampleCount_RADIANCE_PER_PIXEL_NORMALIZED();

		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					if (overwrite)
						channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					else
						channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) && film.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		additional_RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = film.samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();

		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					if (overwrite)
						channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					else
						channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	samplesCounts.AddSampleCount(additional_SampleCount,
			additional_RADIANCE_PER_PIXEL_NORMALIZED_SampleCount,
			additional_RADIANCE_PER_SCREEN_NORMALIZED_SampleCount);

	if (HasChannel(ALPHA) && film.HasChannel(ALPHA)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_ALPHA->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_ALPHA->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_ALPHA->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(POSITION) && film.HasChannel(POSITION)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH) && !overwrite) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_POSITION->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_POSITION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_POSITION->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_POSITION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(GEOMETRY_NORMAL) && film.HasChannel(GEOMETRY_NORMAL)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH) && !overwrite) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_GEOMETRY_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_GEOMETRY_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_GEOMETRY_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_GEOMETRY_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(SHADING_NORMAL) && film.HasChannel(SHADING_NORMAL)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH) && !overwrite) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(MATERIAL_ID) && film.HasChannel(MATERIAL_ID)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH) && !overwrite) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const u_int *srcPixel = film.channel_MATERIAL_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_MATERIAL_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const u_int *srcPixel = film.channel_MATERIAL_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_MATERIAL_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(DIRECT_DIFFUSE) && film.HasChannel(DIRECT_DIFFUSE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_DIFFUSE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_DIFFUSE->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}
	if (HasChannel(DIRECT_DIFFUSE_REFLECT) && film.HasChannel(DIRECT_DIFFUSE_REFLECT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_DIFFUSE_REFLECT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_DIFFUSE_REFLECT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_DIFFUSE_REFLECT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_DIFFUSE_TRANSMIT) && film.HasChannel(DIRECT_DIFFUSE_TRANSMIT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_DIFFUSE_TRANSMIT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_DIFFUSE_TRANSMIT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_DIFFUSE_TRANSMIT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_GLOSSY) && film.HasChannel(DIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_GLOSSY->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_GLOSSY_REFLECT) && film.HasChannel(DIRECT_GLOSSY_REFLECT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_GLOSSY_REFLECT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_GLOSSY_REFLECT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_GLOSSY_REFLECT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_GLOSSY_TRANSMIT) && film.HasChannel(DIRECT_GLOSSY_TRANSMIT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_GLOSSY_TRANSMIT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_GLOSSY_TRANSMIT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_GLOSSY_TRANSMIT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(EMISSION) && film.HasChannel(EMISSION)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_EMISSION->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_EMISSION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_EMISSION->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_DIFFUSE) && film.HasChannel(INDIRECT_DIFFUSE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_DIFFUSE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_DIFFUSE->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_DIFFUSE_REFLECT) && film.HasChannel(INDIRECT_DIFFUSE_REFLECT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_DIFFUSE_REFLECT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_DIFFUSE_REFLECT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_DIFFUSE_REFLECT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_DIFFUSE_TRANSMIT) && film.HasChannel(INDIRECT_DIFFUSE_TRANSMIT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_DIFFUSE_TRANSMIT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_DIFFUSE_TRANSMIT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_DIFFUSE_TRANSMIT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_GLOSSY) && film.HasChannel(INDIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_GLOSSY->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_GLOSSY_REFLECT) && film.HasChannel(INDIRECT_GLOSSY_REFLECT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_GLOSSY_REFLECT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_GLOSSY_REFLECT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_GLOSSY_REFLECT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_GLOSSY_TRANSMIT) && film.HasChannel(INDIRECT_GLOSSY_TRANSMIT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_GLOSSY_TRANSMIT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_GLOSSY_TRANSMIT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_GLOSSY_TRANSMIT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SPECULAR) && film.HasChannel(INDIRECT_SPECULAR)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SPECULAR->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_SPECULAR->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_SPECULAR->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SPECULAR_REFLECT) && film.HasChannel(INDIRECT_SPECULAR_REFLECT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SPECULAR_REFLECT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_SPECULAR_REFLECT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_SPECULAR_REFLECT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SPECULAR_TRANSMIT) && film.HasChannel(INDIRECT_SPECULAR_TRANSMIT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SPECULAR_TRANSMIT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_INDIRECT_SPECULAR_TRANSMIT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_INDIRECT_SPECULAR_TRANSMIT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(MATERIAL_ID_MASK) && film.HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < channel_MATERIAL_ID_MASKs.size(); ++i) {
			for (u_int j = 0; j < film.maskMaterialIDs.size(); ++j) {
				if (maskMaterialIDs[i] == film.maskMaterialIDs[j]) {
					for (u_int y = 0; y < srcHeight; ++y) {
						for (u_int x = 0; x < srcWidth; ++x) {
							const float *srcPixel = film.channel_MATERIAL_ID_MASKs[j]->GetPixel(srcOffsetX + x, srcOffsetY + y);
							if (overwrite)
								channel_MATERIAL_ID_MASKs[i]->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
							else
								channel_MATERIAL_ID_MASKs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
						}
					}
				}
			}
		}
	}

	if (HasChannel(DIRECT_SHADOW_MASK) && film.HasChannel(DIRECT_SHADOW_MASK)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_SHADOW_MASK->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DIRECT_SHADOW_MASK->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DIRECT_SHADOW_MASK->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SHADOW_MASK) && film.HasChannel(INDIRECT_SHADOW_MASK)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SHADOW_MASK->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_SHADOW_MASK->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(UV) && film.HasChannel(UV)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH) && !overwrite) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_UV->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_UV->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_UV->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_UV->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(RAYCOUNT) && film.HasChannel(RAYCOUNT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_RAYCOUNT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_RAYCOUNT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_RAYCOUNT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(BY_MATERIAL_ID) && film.HasChannel(BY_MATERIAL_ID)) {
		for (u_int i = 0; i < channel_BY_MATERIAL_IDs.size(); ++i) {
			for (u_int j = 0; j < film.byMaterialIDs.size(); ++j) {
				if (byMaterialIDs[i] == film.byMaterialIDs[j]) {
					for (u_int y = 0; y < srcHeight; ++y) {
						for (u_int x = 0; x < srcWidth; ++x) {
							const float *srcPixel = film.channel_BY_MATERIAL_IDs[j]->GetPixel(srcOffsetX + x, srcOffsetY + y);
							if (overwrite)
								channel_BY_MATERIAL_IDs[i]->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
							else
								channel_BY_MATERIAL_IDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
						}
					}
				}
			}
		}
	}

	if (HasChannel(IRRADIANCE) && film.HasChannel(IRRADIANCE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_IRRADIANCE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_IRRADIANCE->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_IRRADIANCE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(OBJECT_ID) && film.HasChannel(OBJECT_ID)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH) && !overwrite) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const u_int *srcPixel = film.channel_OBJECT_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_OBJECT_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const u_int *srcPixel = film.channel_OBJECT_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_OBJECT_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(OBJECT_ID_MASK) && film.HasChannel(OBJECT_ID_MASK)) {
		for (u_int i = 0; i < channel_OBJECT_ID_MASKs.size(); ++i) {
			for (u_int j = 0; j < film.maskObjectIDs.size(); ++j) {
				if (maskObjectIDs[i] == film.maskObjectIDs[j]) {
					for (u_int y = 0; y < srcHeight; ++y) {
						for (u_int x = 0; x < srcWidth; ++x) {
							const float *srcPixel = film.channel_OBJECT_ID_MASKs[j]->GetPixel(srcOffsetX + x, srcOffsetY + y);
							if (overwrite)
								channel_OBJECT_ID_MASKs[i]->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
							else
								channel_OBJECT_ID_MASKs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
						}
					}
				}
			}
		}
	}

	if (HasChannel(BY_OBJECT_ID) && film.HasChannel(BY_OBJECT_ID)) {
		for (u_int i = 0; i < channel_BY_OBJECT_IDs.size(); ++i) {
			for (u_int j = 0; j < film.byObjectIDs.size(); ++j) {
				if (byObjectIDs[i] == film.byObjectIDs[j]) {
					for (u_int y = 0; y < srcHeight; ++y) {
						for (u_int x = 0; x < srcWidth; ++x) {
							const float *srcPixel = film.channel_BY_OBJECT_IDs[j]->GetPixel(srcOffsetX + x, srcOffsetY + y);
							if (overwrite)
								channel_BY_OBJECT_IDs[i]->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
							else
								channel_BY_OBJECT_IDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
						}
					}
				}
			}
		}
	}

	if (HasChannel(SAMPLECOUNT) && film.HasChannel(SAMPLECOUNT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const u_int *srcPixel = film.channel_SAMPLECOUNT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_SAMPLECOUNT->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_SAMPLECOUNT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	// CONVERGENCE values can not really be added, they will be updated at the next test

	if (HasChannel(MATERIAL_ID_COLOR) && film.HasChannel(MATERIAL_ID_COLOR)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_MATERIAL_ID_COLOR->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_MATERIAL_ID_COLOR->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_MATERIAL_ID_COLOR->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(ALBEDO) && film.HasChannel(ALBEDO)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_ALBEDO->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_ALBEDO->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_ALBEDO->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}


	if (HasChannel(AVG_SHADING_NORMAL) && film.HasChannel(AVG_SHADING_NORMAL)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_AVG_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_AVG_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_AVG_SHADING_NORMAL->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	// NOTE: update DEPTH channel last because it is used to merge other channels
	if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y);
				if (overwrite)
					channel_DEPTH->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				else
					channel_DEPTH->MinPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (overwrite) {
		if (HasChannel(NOISE) && film.HasChannel(NOISE)) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_NOISE->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_NOISE->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	} else {
		// NOISE values can not really be added, they will be updated at the next test
	}

	if (overwrite) {
		if (HasChannel(USER_IMPORTANCE) && film.HasChannel(USER_IMPORTANCE)) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_USER_IMPORTANCE->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_USER_IMPORTANCE->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	} else {
		// USER_IMPORTANCE values can not really be added, I will keep the one in the
		// current film
	}

	//--------------------------------------------------------------------------
	// Film denoiser related code
	//--------------------------------------------------------------------------

	if (filmDenoiser.IsEnabled() && !filmDenoiser.HasReferenceFilm()) {
		if (overwrite)
			filmDenoiser.Reset();

		// Add denoiser SamplesAccumulator statistics
		filmDenoiser.AddDenoiser(film.GetDenoiser(),
				srcOffsetX, srcOffsetY,
				srcWidth, srcHeight,
				dstOffsetX, dstOffsetY);

		// Check if the BCD denoiser warm up period is over
		if (!filmDenoiser.IsWarmUpDone())
			filmDenoiser.CheckIfWarmUpDone();
	}
}

void Film::SetFilm(const Film &film,
	const u_int srcOffsetX, const u_int srcOffsetY,
	const u_int srcWidth, const u_int srcHeight,
	const u_int dstOffsetX, const u_int dstOffsetY) {
	AddFilmImpl<true>(film, srcOffsetX, srcOffsetY,
			srcWidth, srcHeight, dstOffsetX, dstOffsetY);
}

void Film::AddFilm(const Film &film,
	const u_int srcOffsetX, const u_int srcOffsetY,
	const u_int srcWidth, const u_int srcHeight,
	const u_int dstOffsetX, const u_int dstOffsetY) {
	AddFilmImpl<false>(film, srcOffsetX, srcOffsetY,
			srcWidth, srcHeight, dstOffsetX, dstOffsetY);
}

void Film::ResetTests() {
	if (convTest)
		convTest->Reset();
	if (noiseEstimation)
		noiseEstimation->Reset();
}

void Film::RunTests() {
	if (statsConvergence == 1.f)
		return;

	// Check if it is time to stop
	if ((haltTime > 0.0) && (GetTotalTime() > haltTime)) {
		SLG_LOG("Time 100%, rendering done.");
		statsConvergence = 1.f;

		return;
	}

	// Check the halt SPP condition with the average samples of
	// the rendered region

	const u_int regionPixelsCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
	const double spp = GetTotalSampleCount() / regionPixelsCount;
	const double spp_PixelNormalized = GetTotalEyeSampleCount() / regionPixelsCount;
	const double spp_ScreenNormalized = GetTotalLightSampleCount() / regionPixelsCount;

	bool haltSPPStop = false;

	// All halt SPP cases
	if (
			(haltSPP_PixelNormalized > 0) && (spp_PixelNormalized > haltSPP_PixelNormalized) &&
			(haltSPP_ScreenNormalized > 0) && (haltSPP_ScreenNormalized > spp_ScreenNormalized)
		)
		haltSPPStop = true;
	if (
			(haltSPP_PixelNormalized > 0) && (spp_PixelNormalized > haltSPP_PixelNormalized) &&
			(haltSPP_ScreenNormalized == 0)
		)
		haltSPPStop = true;
	if (
			(haltSPP_PixelNormalized == 0) &&
			(haltSPP_ScreenNormalized > 0) && (spp_ScreenNormalized > haltSPP_ScreenNormalized)
		)
		haltSPPStop = true;
	if (
			(haltSPP_PixelNormalized == 0) &&
			(haltSPP_ScreenNormalized == 0) &&
			(haltSPP > 0) && (spp > haltSPP)
		)
		haltSPPStop = true;
		
	if (haltSPPStop) {
		SLG_LOG("Samples per pixel 100%, rendering done.");
		statsConvergence = 1.f;

		return;
	}

	if (convTest) {
		assert (HasChannel(IMAGEPIPELINE));

		// Required in order to have a valid convergence test
		ExecuteImagePipeline(0);
	}

	const bool convTestRequired = (convTest && convTest->IsTestUpdateRequired());
	const bool noiseEstimationRequired = (noiseEstimation && noiseEstimation->IsTestUpdateRequired());
	if (convTestRequired || noiseEstimationRequired) {
		assert (HasChannel(IMAGEPIPELINE));

		// Required in order to have a valid convergence test
		ExecuteImagePipeline(0);
	}

	if (convTestRequired) {
		// Run the convergence test
		const u_int testResult = convTest->Test();
		
		// Set statsConvergence only if haltNoiseThreshold is enabled
		if (haltNoiseThreshold > 0.f)
			statsConvergence = 1.f - testResult / static_cast<float>(pixelCount);
	}
	
	if (noiseEstimationRequired) {
		ExecuteImagePipeline(noiseEstimationImagePipelineIndex);
		// Run the noise estimation test
		noiseEstimation->Test();
	}
}
