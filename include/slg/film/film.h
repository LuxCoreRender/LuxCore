/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_FILM_H
#define	_SLG_FILM_H

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>

#include <boost/thread/mutex.hpp>

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/utils/properties.h"
#include "slg/slg.h"
#include "slg/sdl/bsdf.h"
#include "slg/film/filter.h"
#include "slg/film/imagepipeline.h"
#include "slg/film/framebuffer.h"
#include "slg/utils/convtest/convtest.h"

namespace slg {

//------------------------------------------------------------------------------
// FilmOutput
//------------------------------------------------------------------------------

class FilmOutputs {
public:
	typedef enum {
		RGB, RGBA, RGB_TONEMAPPED, RGBA_TONEMAPPED, ALPHA, DEPTH, POSITION,
		GEOMETRY_NORMAL, SHADING_NORMAL, MATERIAL_ID, DIRECT_DIFFUSE,
		DIRECT_GLOSSY, EMISSION, INDIRECT_DIFFUSE, INDIRECT_GLOSSY,
		INDIRECT_SPECULAR, MATERIAL_ID_MASK, DIRECT_SHADOW_MASK, INDIRECT_SHADOW_MASK,
		RADIANCE_GROUP, UV, RAYCOUNT, BY_MATERIAL_ID
	} FilmOutputType;

	FilmOutputs() { }
	~FilmOutputs() { }

	u_int GetCount() const { return types.size(); }
	FilmOutputType GetType(const u_int index) const { return types[index]; }
	const std::string &GetFileName(const u_int index) const { return fileNames[index]; }
	const luxrays::Properties &GetProperties(const u_int index) const { return props[index]; }

	void Add(const FilmOutputType type, const std::string &fileName,
		const luxrays::Properties *prop = NULL);

private:
	std::vector<FilmOutputType> types;
	std::vector<std::string> fileNames;
	std::vector<luxrays::Properties> props;
};

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

class SampleResult;

class Film {
public:
	typedef enum {
		RADIANCE_PER_PIXEL_NORMALIZED = 1<<0,
		RADIANCE_PER_SCREEN_NORMALIZED = 1<<1,
		ALPHA = 1<<2,
		RGB_TONEMAPPED = 1<<3,
		DEPTH = 1<<4,
		POSITION = 1<<5,
		GEOMETRY_NORMAL = 1<<6,
		SHADING_NORMAL = 1<<7,
		MATERIAL_ID = 1<<8,
		DIRECT_DIFFUSE = 1<<9,
		DIRECT_GLOSSY = 1<<10,
		EMISSION = 1<<11,
		INDIRECT_DIFFUSE = 1<<12,
		INDIRECT_GLOSSY = 1<<13,
		INDIRECT_SPECULAR = 1<<14,
		MATERIAL_ID_MASK = 1<<15,
		DIRECT_SHADOW_MASK = 1<<16,
		INDIRECT_SHADOW_MASK = 1<<17,
		UV = 1<<18,
		RAYCOUNT = 1<<19,
		BY_MATERIAL_ID = 1<<20
	} FilmChannelType;

	Film(const u_int w, const u_int h);
	~Film();

	bool HasChannel(const FilmChannelType type) const { return channels.count(type) > 0; }
	// This one must be called before Init()
	void AddChannel(const FilmChannelType type,
		const luxrays::Properties *prop = NULL);
	// This one must be called before Init()
	void RemoveChannel(const FilmChannelType type);
	// This one must be called before Init()
	void SetRadianceGroupCount(const u_int count) { radianceGroupCount = count; }
	u_int GetRadianceGroupCount() const { return radianceGroupCount; }
	u_int GetMaskMaterialIDCount() const { return maskMaterialIDs.size(); }
	u_int GetMaskMaterialID(const u_int index) const { return maskMaterialIDs[index]; }
	u_int GetByMaterialIDCount() const { return byMaterialIDs.size(); }
	u_int GetByMaterialID(const u_int index) const { return byMaterialIDs[index]; }
	
	void Init();
	void Resize(const u_int w, const u_int h);
	void Reset();

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void SetOverlappedScreenBufferUpdateFlag(const bool overlappedScreenBufferUpdate) {
		enabledOverlappedScreenBufferUpdate = overlappedScreenBufferUpdate;
	}
	bool IsOverlappedScreenBufferUpdate() const { return enabledOverlappedScreenBufferUpdate; }

	void SetFilter(Filter *flt);
	const Filter *GetFilter() const { return filter; }

	// Note: used mostly for RT modes
	void SetRGBTonemapUpdateFlag(const bool v) { rgbTonemapUpdate = v; }
	void SetImagePipeline(ImagePipeline *ip) { imagePipeline = ip; }
	const ImagePipeline *GetImagePipeline() const { return imagePipeline; }

	void CopyDynamicSettings(const Film &film) {
		channels = film.channels;
		maskMaterialIDs = film.maskMaterialIDs;
		byMaterialIDs = film.byMaterialIDs;
		radianceGroupCount = film.radianceGroupCount;
		SetFilter(film.GetFilter() ? film.GetFilter()->Clone() : NULL);
		SetRGBTonemapUpdateFlag(film.rgbTonemapUpdate);
		SetImagePipeline(film.GetImagePipeline()->Copy());
		SetOverlappedScreenBufferUpdateFlag(film.IsOverlappedScreenBufferUpdate());
	}

	//--------------------------------------------------------------------------

	void AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void AddFilm(const Film &film) {
		AddFilm(film, 0, 0, width, height, 0, 0);
	}

	u_int GetChannelCount(const FilmChannelType type) const;
	size_t GetOutputSize(const FilmOutputs::FilmOutputType type) const;
	bool HasOutput(const FilmOutputs::FilmOutputType type) const;
	void Output(const FilmOutputs &filmOutputs);
	void Output(const FilmOutputs::FilmOutputType type, const std::string &fileName,
		const luxrays::Properties *props = NULL);

	template<class T> const T *GetChannel(const FilmChannelType type, const u_int index = 0) {
		throw std::runtime_error("Called Film::GetChannel() with wrong type");
	}
	template<class T> void GetOutput(const FilmOutputs::FilmOutputType type, T *buffer, const u_int index = 0) {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	void ExecuteImagePipeline();

	//--------------------------------------------------------------------------

	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	double GetTotalSampleCount() const {
		return statsTotalSampleCount;
	}
	double GetTotalTime() const {
		return luxrays::WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		return GetTotalSampleCount() / GetTotalTime();
	}

	//--------------------------------------------------------------------------

	void ResetConvergenceTest();
	u_int RunConvergenceTest();

	//--------------------------------------------------------------------------

	void SetSampleCount(const double count) {
		statsTotalSampleCount = count;
	}
	void AddSampleCount(const double count) {
		statsTotalSampleCount += count;
	}

	void AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight = 1.f);
	void SplatSample(const SampleResult &sampleResult, const float weight = 1.f);

	std::vector<GenericFrameBuffer<4, 1, float> *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	GenericFrameBuffer<2, 1, float> *channel_ALPHA;
	GenericFrameBuffer<3, 0, float> *channel_RGB_TONEMAPPED;
	GenericFrameBuffer<1, 0, float> *channel_DEPTH;
	GenericFrameBuffer<3, 0, float> *channel_POSITION;
	GenericFrameBuffer<3, 0, float> *channel_GEOMETRY_NORMAL;
	GenericFrameBuffer<3, 0, float> *channel_SHADING_NORMAL;
	GenericFrameBuffer<1, 0, u_int> *channel_MATERIAL_ID;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_DIFFUSE;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_GLOSSY;
	GenericFrameBuffer<4, 1, float> *channel_EMISSION;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_DIFFUSE;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_GLOSSY;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_SPECULAR;
	std::vector<GenericFrameBuffer<2, 1, float> *> channel_MATERIAL_ID_MASKs;
	GenericFrameBuffer<2, 1, float> *channel_DIRECT_SHADOW_MASK;
	GenericFrameBuffer<2, 1, float> *channel_INDIRECT_SHADOW_MASK;
	GenericFrameBuffer<2, 0, float> *channel_UV;
	GenericFrameBuffer<1, 0, float> *channel_RAYCOUNT;
	std::vector<GenericFrameBuffer<4, 1, float> *> channel_BY_MATERIAL_IDs;

	static FilmChannelType String2FilmChannelType(const std::string &type);
	static const std::string FilmChannelType2String(const FilmChannelType type);

private:
	void MergeSampleBuffers(luxrays::Spectrum *p, std::vector<bool> &frameBufferMask) const;
	void GetPixelFromMergedSampleBuffers(const u_int index, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int x, const u_int y, float *c) const {
		GetPixelFromMergedSampleBuffers(x + y * width, c);
	}

	void AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult);

	std::set<FilmChannelType> channels;
	u_int width, height, pixelCount, radianceGroupCount;
	std::vector<u_int> maskMaterialIDs, byMaterialIDs;

	// Used to speedup sample splatting, initialized inside Init()
	bool hasDataChannel, hasComposingChannel;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	ImagePipeline *imagePipeline;
	ConvergenceTest *convTest;
	Filter *filter;
	FilterLUTs *filterLUTs;

	bool initialized, enabledOverlappedScreenBufferUpdate, rgbTonemapUpdate;
};

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index);
template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index);
template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer, const u_int index);
template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer, const u_int index);

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

class SampleResult {
public:
	SampleResult() { }
	SampleResult(const u_int channelTypes, const u_int radianceGroupCount) {
		Init(channelTypes, radianceGroupCount);
	}
	~SampleResult() { }

	void Init(const u_int channelTypes, const u_int radianceGroupCount) {
		channels = channelTypes;
		if (channels | Film::RADIANCE_PER_PIXEL_NORMALIZED)
			radiancePerPixelNormalized.resize(radianceGroupCount);
		if (channels | Film::RADIANCE_PER_SCREEN_NORMALIZED)
			radiancePerScreenNormalized.resize(radianceGroupCount);

		firstPathVertex = true;
		// lastPathVertex can not be really initialized here without knowing
		// the max. path depth.
		lastPathVertex = false;
		firstPathVertexEvent = NONE;
	}
	bool HasChannel(const Film::FilmChannelType type) const { return (channels & type) != 0; }

	void AddEmission(const u_int lightID, const luxrays::Spectrum &radiance);
	void AddDirectLight(const u_int lightID, const BSDFEvent bsdfEvent,
		const luxrays::Spectrum &radiance, const float lightScale);

	void ClampRadiance(const float cap) {
		for (u_int i = 0; i < radiancePerPixelNormalized.size(); ++i)
			radiancePerPixelNormalized[i] = radiancePerPixelNormalized[i].Clamp(0.f, cap);
	}
	
	static void AddSampleResult(std::vector<SampleResult> &sampleResults,
		const float filmX, const float filmY,
		const luxrays::Spectrum &radiancePPN,
		const float alpha);

	static void AddSampleResult(std::vector<SampleResult> &sampleResults,
		const float filmX, const float filmY,
		const luxrays::Spectrum &radiancePSN);

	float filmX, filmY;
	std::vector<luxrays::Spectrum> radiancePerPixelNormalized, radiancePerScreenNormalized;
	float alpha, depth;
	luxrays::Point position;
	luxrays::Normal geometryNormal, shadingNormal;
	// Note: MATERIAL_ID_MASK is calculated starting from materialID field
	u_int materialID;
	luxrays::Spectrum directDiffuse, directGlossy;
	luxrays::Spectrum emission;
	luxrays::Spectrum indirectDiffuse, indirectGlossy, indirectSpecular;
	float directShadowMask, indirectShadowMask;
	luxrays::UV uv;
	float rayCount;

	BSDFEvent firstPathVertexEvent;
	bool firstPathVertex, lastPathVertex;

private:
	u_int channels;
};

}

#endif	/* _SLG_FILM_H */
