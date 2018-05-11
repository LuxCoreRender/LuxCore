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

	channel_ALPHA = NULL;
	channel_DEPTH = NULL;
	channel_POSITION = NULL;
	channel_GEOMETRY_NORMAL = NULL;
	channel_SHADING_NORMAL = NULL;
	channel_MATERIAL_ID = NULL;
	channel_DIRECT_DIFFUSE = NULL;
	channel_DIRECT_GLOSSY = NULL;
	channel_EMISSION = NULL;
	channel_INDIRECT_DIFFUSE = NULL;
	channel_INDIRECT_GLOSSY = NULL;
	channel_INDIRECT_SPECULAR = NULL;
	channel_DIRECT_SHADOW_MASK = NULL;
	channel_INDIRECT_SHADOW_MASK = NULL;
	channel_UV = NULL;
	channel_RAYCOUNT = NULL;
	channel_IRRADIANCE = NULL;
	channel_OBJECT_ID = NULL;
	channel_FRAMEBUFFER_MASK = NULL;
	channel_SAMPLECOUNT = NULL;
	channel_CONVERGENCE = NULL;

	convTest = NULL;
	haltTime = 0.0;
	haltSPP = 0;
	haltThreshold = 0.f;

	isAsyncImagePipelineRunning = false;
	imagePipelineThread = NULL;

	enabledOverlappedScreenBufferUpdate = true;

	// Initialize variables to NULL
	SetUpOCL();
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

	channel_ALPHA = NULL;
	channel_DEPTH = NULL;
	channel_POSITION = NULL;
	channel_GEOMETRY_NORMAL = NULL;
	channel_SHADING_NORMAL = NULL;
	channel_MATERIAL_ID = NULL;
	channel_DIRECT_DIFFUSE = NULL;
	channel_DIRECT_GLOSSY = NULL;
	channel_EMISSION = NULL;
	channel_INDIRECT_DIFFUSE = NULL;
	channel_INDIRECT_GLOSSY = NULL;
	channel_INDIRECT_SPECULAR = NULL;
	channel_DIRECT_SHADOW_MASK = NULL;
	channel_INDIRECT_SHADOW_MASK = NULL;
	channel_UV = NULL;
	channel_RAYCOUNT = NULL;
	channel_IRRADIANCE = NULL;
	channel_OBJECT_ID = NULL;
	channel_FRAMEBUFFER_MASK = NULL;
	channel_SAMPLECOUNT = NULL;
	channel_CONVERGENCE = NULL;

	convTest = NULL;
	haltTime = 0.0;

	haltSPP = 0;
	haltThreshold = .02f;
	haltThresholdWarmUp = 64;
	haltThresholdTestStep = 64;
	haltThresholdUseFilter = true;
	haltThresholdStopRendering = true;

	isAsyncImagePipelineRunning = false;
	imagePipelineThread = NULL;

	enabledOverlappedScreenBufferUpdate = true;

	// Initialize variables to NULL
	SetUpOCL();
}

Film::~Film() {
	if (imagePipelineThread) {
		imagePipelineThread->interrupt();
		imagePipelineThread->join();
		delete imagePipelineThread;
	}

	BOOST_FOREACH(ImagePipeline *ip, imagePipelines)
		delete ip;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// I have to delete the OCL context after the image pipeline because it
	// can be used by plugins
	DeleteOCLContext();
#endif

	delete convTest;

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

	SetOverlappedScreenBufferUpdateFlag(film.IsOverlappedScreenBufferUpdate());

	filmDenoiser.SetEnabled(film.filmDenoiser.IsEnabled());
}

void Film::Init() {
	if (initialized)
		throw runtime_error("A Film can not be initialized multiple times");

	// FRAMEBUFFER_MASK channel is enabled by default as it is required
	// by image pipeline
	AddChannel(FRAMEBUFFER_MASK);

	if (imagePipelines.size() > 0)
		AddChannel(IMAGEPIPELINE);

	// CONVERGENCE is generated by the convergence test
	if (HasChannel(CONVERGENCE) && !convTest) {
		// The test has to be enabled to update the CONVERGNCE AOV

		// Using the default values
		convTest = new FilmConvTest(this, haltThreshold, haltThresholdWarmUp, haltThresholdTestStep, haltThresholdUseFilter);
	}

	initialized = true;

	Resize(width, height);
}

void Film::SetSampleCount(const double count) {
	statsTotalSampleCount = count;
	
	// Check the if Film denoiser warmup is done
	if (filmDenoiser.IsEnabled() && !filmDenoiser.HasReferenceFilm() &&
			!filmDenoiser.IsWarmUpDone() && (statsTotalSampleCount / pixelCount > 2.0))
		filmDenoiser.WarmUpDone();
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
		channel_RADIANCE_PER_PIXEL_NORMALIZEDs.resize(radianceGroupCount, NULL);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i] = new GenericFrameBuffer<4, 1, float>(width, height);
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->Clear();
		}
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		channel_RADIANCE_PER_SCREEN_NORMALIZEDs.resize(radianceGroupCount, NULL);
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
		channel_IMAGEPIPELINEs.resize(imagePipelines.size(), NULL);
		for (u_int i = 0; i < channel_IMAGEPIPELINEs.size(); ++i) {
			channel_IMAGEPIPELINEs[i] = new GenericFrameBuffer<3, 0, float>(width, height);
			channel_IMAGEPIPELINEs[i]->Clear();
		}

		// Resize the converge test too if required
		if (convTest)
			convTest->Reset();
	} else {
		delete convTest;
		convTest = NULL;
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
		channel_GEOMETRY_NORMAL->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(SHADING_NORMAL)) {
		channel_SHADING_NORMAL = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_SHADING_NORMAL->Clear(numeric_limits<float>::infinity());
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
	if (HasChannel(DIRECT_GLOSSY)) {
		channel_DIRECT_GLOSSY = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_GLOSSY->Clear();
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
	if (HasChannel(INDIRECT_GLOSSY)) {
		channel_INDIRECT_GLOSSY = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SPECULAR)) {
		channel_INDIRECT_SPECULAR = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_SPECULAR->Clear();
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
	if (HasChannel(FRAMEBUFFER_MASK)) {
		channel_FRAMEBUFFER_MASK = new GenericFrameBuffer<1, 0, u_int>(width, height);
		channel_FRAMEBUFFER_MASK->Clear();
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

	// Reset BCD statistcs accumulator (I need to redo the warmup period)
	filmDenoiser.Reset();

	// Initialize the statistics
	statsTotalSampleCount = 0.0;
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
	if (HasChannel(DIRECT_GLOSSY))
		channel_DIRECT_GLOSSY->Clear();
	if (HasChannel(EMISSION))
		channel_EMISSION->Clear();
	if (HasChannel(INDIRECT_DIFFUSE))
		channel_INDIRECT_DIFFUSE->Clear();
	if (HasChannel(INDIRECT_GLOSSY))
		channel_INDIRECT_GLOSSY->Clear();
	if (HasChannel(INDIRECT_SPECULAR))
		channel_INDIRECT_SPECULAR->Clear();
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
	if (HasChannel(FRAMEBUFFER_MASK))
		channel_FRAMEBUFFER_MASK->Clear();
	if (HasChannel(SAMPLECOUNT))
		channel_SAMPLECOUNT->Clear();
	// channel_CONVERGENCE is not cleared otherwise the result of the halt test
	// would be lost

	// denoiser is not cleared otherwise the collected data would be lost

	statsTotalSampleCount = 0.0;
	// statsConvergence is not cleared otherwise the result of the halt test
	// would be lost
}

void Film::Reset() {
	Clear();

	// denoiser  has to be reset explicitly

	// convTest has to be reset explicitly

	statsTotalSampleCount = 0.0;
	statsConvergence = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::VarianceClampFilm(const VarianceClamping &varianceClamping,
		const Film &film, const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && film.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					float *srcPixel = film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					const float *dstPixel = channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(dstOffsetX + x, dstOffsetY + y);

					varianceClamping.Clamp(dstPixel, srcPixel);
				}
			}
		}
	}
}

void Film::AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	statsTotalSampleCount += film.statsTotalSampleCount;

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && film.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) && film.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(ALPHA) && film.HasChannel(ALPHA)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_ALPHA->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_ALPHA->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(POSITION) && film.HasChannel(POSITION)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
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
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
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
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
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
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
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
				channel_DIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_GLOSSY) && film.HasChannel(DIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(EMISSION) && film.HasChannel(EMISSION)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_EMISSION->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_EMISSION->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_DIFFUSE) && film.HasChannel(INDIRECT_DIFFUSE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_DIFFUSE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_GLOSSY) && film.HasChannel(INDIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SPECULAR) && film.HasChannel(INDIRECT_SPECULAR)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SPECULAR->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_SPECULAR->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
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
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
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
				channel_IRRADIANCE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(OBJECT_ID) && film.HasChannel(OBJECT_ID)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
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
				channel_SAMPLECOUNT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	// CONVERGENCE values can not really be added, they will be updated at the next test

	// NOTE: update DEPTH channel last because it is used to merge other channels
	if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DEPTH->MinPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	//--------------------------------------------------------------------------
	// Film denoiser related code
	//--------------------------------------------------------------------------

	if (filmDenoiser.IsEnabled() && filmDenoiser.IsFilmAddEnabled()) {
		// Add denoiser SamplesAccumulator statistics
		filmDenoiser.AddDenoiser(film.GetDenoiser(),
				srcOffsetX, srcOffsetY,
				srcWidth, srcHeight,
				dstOffsetX, dstOffsetY);

		// Check if the BCD denoiser warm up period is over
		if (!filmDenoiser.HasReferenceFilm() && !filmDenoiser.IsWarmUpDone()
				&& (GetTotalSampleCount() / pixelCount > 2.0))
			filmDenoiser.WarmUpDone();
	}
}

void Film::AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	filmDenoiser.AddSample(x, y, sampleResult, weight);

	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AddWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE))
			channel_DIRECT_DIFFUSE->AddWeightedPixel(x, y, sampleResult.directDiffuse.c, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY))
			channel_DIRECT_GLOSSY->AddWeightedPixel(x, y, sampleResult.directGlossy.c, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AddWeightedPixel(x, y, sampleResult.emission.c, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE))
			channel_INDIRECT_DIFFUSE->AddWeightedPixel(x, y, sampleResult.indirectDiffuse.c, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY))
			channel_INDIRECT_GLOSSY->AddWeightedPixel(x, y, sampleResult.indirectGlossy.c, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR))
			channel_INDIRECT_SPECULAR->AddWeightedPixel(x, y, sampleResult.indirectSpecular.c, weight);

		// This is MATERIAL_ID_MASK and BY_MATERIAL_ID
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			// MATERIAL_ID_MASK
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_MATERIAL_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byMaterialIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.materialID == byMaterialIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_MATERIAL_IDs[index]->AddWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(DIRECT_SHADOW)
		if (channel_DIRECT_SHADOW_MASK && sampleResult.HasChannel(DIRECT_SHADOW_MASK))
			channel_DIRECT_SHADOW_MASK->AddWeightedPixel(x, y, &sampleResult.directShadowMask, weight);

		// Faster than HasChannel(INDIRECT_SHADOW_MASK)
		if (channel_INDIRECT_SHADOW_MASK && sampleResult.HasChannel(INDIRECT_SHADOW_MASK))
			channel_INDIRECT_SHADOW_MASK->AddWeightedPixel(x, y, &sampleResult.indirectShadowMask, weight);

		// Faster than HasChannel(IRRADIANCE)
		if (channel_IRRADIANCE && sampleResult.HasChannel(IRRADIANCE))
			channel_IRRADIANCE->AddWeightedPixel(x, y, sampleResult.irradiance.c, weight);

		// This is OBJECT_ID_MASK and BY_OBJECT_ID
		if (sampleResult.HasChannel(OBJECT_ID)) {
			// OBJECT_ID_MASK
			for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.objectID == maskObjectIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_OBJECT_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_OBJECT_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byObjectIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.objectID == byObjectIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_OBJECT_IDs[index]->AddWeightedPixel(x, y, c.c, weight);
				}
			}
		}
	}
}

void Film::AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult)  {
	bool depthWrite = true;

	// Faster than HasChannel(DEPTH)
	if (channel_DEPTH && sampleResult.HasChannel(DEPTH))
		depthWrite = channel_DEPTH->MinPixel(x, y, &sampleResult.depth);

	if (depthWrite) {
		// Faster than HasChannel(POSITION)
		if (channel_POSITION && sampleResult.HasChannel(POSITION))
			channel_POSITION->SetPixel(x, y, &sampleResult.position.x);

		// Faster than HasChannel(GEOMETRY_NORMAL)
		if (channel_GEOMETRY_NORMAL && sampleResult.HasChannel(GEOMETRY_NORMAL))
			channel_GEOMETRY_NORMAL->SetPixel(x, y, &sampleResult.geometryNormal.x);

		// Faster than HasChannel(SHADING_NORMAL)
		if (channel_SHADING_NORMAL && sampleResult.HasChannel(SHADING_NORMAL))
			channel_SHADING_NORMAL->SetPixel(x, y, &sampleResult.shadingNormal.x);

		// Faster than HasChannel(MATERIAL_ID)
		if (channel_MATERIAL_ID && sampleResult.HasChannel(MATERIAL_ID))
			channel_MATERIAL_ID->SetPixel(x, y, &sampleResult.materialID);

		// Faster than HasChannel(UV)
		if (channel_UV && sampleResult.HasChannel(UV))
			channel_UV->SetPixel(x, y, &sampleResult.uv.u);

		// Faster than HasChannel(OBJECT_ID)
		if (channel_OBJECT_ID && sampleResult.HasChannel(OBJECT_ID) &&
				(sampleResult.objectID != std::numeric_limits<u_int>::max()))
			channel_OBJECT_ID->SetPixel(x, y, &sampleResult.objectID);
	}

	if (channel_RAYCOUNT && sampleResult.HasChannel(RAYCOUNT))
		channel_RAYCOUNT->AddPixel(x, y, &sampleResult.rayCount);

	if (channel_SAMPLECOUNT && sampleResult.HasChannel(SAMPLECOUNT)) {
		static u_int one = 1;
		channel_SAMPLECOUNT->AddPixel(x, y, &one);
	}
	
	// Nothing to do for CONVERGENCE channel because it is updated
	// by the convergence test
}

void Film::AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	AddSampleResultColor(x, y, sampleResult, weight);
	if (hasDataChannel)
		AddSampleResultData(x, y, sampleResult);
}

void Film::ResetHaltTests() {
	if (convTest)
		convTest->Reset();
}

void Film::RunHaltTests() {
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
	const double spp = statsTotalSampleCount / ((subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1));
	if ((haltSPP > 0.0) && (spp > haltSPP)) {
		SLG_LOG("Samples per pixel 100%, rendering done.");
		statsConvergence = 1.f;

		return;
	}

	if (convTest) {
		assert (HasChannel(IMAGEPIPELINE));

		// Required in order to have a valid convergence test
		ExecuteImagePipeline(0);

		// Run the test
		const u_int testResult = convTest->Test();
		
		// Set statsConvergence only if the haltThresholdStopRendering is true
		if (haltThresholdStopRendering)
			statsConvergence = 1.f - testResult / static_cast<float>(pixelCount);
	}
}
