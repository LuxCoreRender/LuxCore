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
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/utils/properties.h"
#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/framebuffer.h"
#include "slg/film/filmoutputs.h"
#include "slg/utils/convtest/convtest.h"
#include "slg/utils/varianceclamping.h"

namespace slg {

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
		BY_MATERIAL_ID = 1<<20,
		IRRADIANCE = 1<<21
	} FilmChannelType;
	
	class RadianceChannelScale {
	public:
		RadianceChannelScale();

		void Init();
		void Scale(float v[3]) const;
		luxrays::Spectrum Scale(const luxrays::Spectrum &v) const;

		float globalScale, temperature;
		luxrays::Spectrum rgbScale;
		bool enabled;

		friend class boost::serialization::access;

	private:
		template<class Archive> void serialize(Archive &ar, const u_int version) {
			ar & globalScale;
			ar & temperature;
			ar & rgbScale;
			ar & enabled;

			Init();
		}

		luxrays::Spectrum scale;
	};

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
	void Parse(const luxrays::Properties &props);

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void SetOverlappedScreenBufferUpdateFlag(const bool overlappedScreenBufferUpdate) {
		enabledOverlappedScreenBufferUpdate = overlappedScreenBufferUpdate;
	}
	bool IsOverlappedScreenBufferUpdate() const { return enabledOverlappedScreenBufferUpdate; }

	// Note: used mostly for RT modes
	void SetRGBTonemapUpdateFlag(const bool v) { rgbTonemapUpdate = v; }
	void SetImagePipeline(ImagePipeline *ip) {
		delete imagePipeline;
		imagePipeline = ip;
	}
	const ImagePipeline *GetImagePipeline() const { return imagePipeline; }

	void CopyDynamicSettings(const Film &film);

	void SetRadianceChannelScale(const u_int index, const RadianceChannelScale &scale);

	//--------------------------------------------------------------------------

	void VarianceClampFilm(const VarianceClamping &varianceClamping, const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void VarianceClampFilm(const VarianceClamping &varianceClamping, const Film &film) {
		VarianceClampFilm(varianceClamping, film, 0, 0, width, height, 0, 0);
	}

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
	void Output();
	void Output(const std::string &fileName, const FilmOutputs::FilmOutputType type,
		const luxrays::Properties *props = NULL);

	template<class T> const T *GetChannel(const FilmChannelType type, const u_int index = 0) {
		throw std::runtime_error("Called Film::GetChannel() with wrong type");
	}
	template<class T> void GetOutput(const FilmOutputs::FilmOutputType type, T *buffer, const u_int index = 0) {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	bool HasDataChannel() { return hasDataChannel; }
	bool HasComposingChannel() { return hasComposingChannel; }

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
	void AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult);

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
	GenericFrameBuffer<4, 1, float> *channel_IRRADIANCE;

	static Film *LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, const Film &film);

	static FilmChannelType String2FilmChannelType(const std::string &type);
	static const std::string FilmChannelType2String(const FilmChannelType type);

	friend class boost::serialization::access;

private:
	// Used by serialization
	Film();

	template<class Archive> void load(Archive &ar, const u_int version);
	template<class Archive> void save(Archive &ar, const u_int version) const;
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	void MergeSampleBuffers(luxrays::Spectrum *p, std::vector<bool> &frameBufferMask) const;
	void GetPixelFromMergedSampleBuffers(const u_int index, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int x, const u_int y, float *c) const {
		GetPixelFromMergedSampleBuffers(x + y * width, c);
	}

	void ParseRadianceGroupsScale(const luxrays::Properties &props);
	void ParseOutputs(const luxrays::Properties &props);
	
	static ImagePipeline *AllocImagePipeline(const luxrays::Properties &props);

	std::set<FilmChannelType> channels;
	u_int width, height, pixelCount, radianceGroupCount;
	std::vector<u_int> maskMaterialIDs, byMaterialIDs;

	// Used to speedup sample splatting, initialized inside Init()
	bool hasDataChannel, hasComposingChannel;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	ImagePipeline *imagePipeline;
	ConvergenceTest *convTest;

	std::vector<RadianceChannelScale> radianceChannelScales;
	FilmOutputs filmOutputs;

	bool initialized, enabledOverlappedScreenBufferUpdate, rgbTonemapUpdate;
};

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index);
template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index);
template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer, const u_int index);
template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer, const u_int index);

template<> void Film::load<boost::archive::binary_iarchive>(boost::archive::binary_iarchive &ar, const u_int version);
template<> void Film::save<boost::archive::binary_oarchive>(boost::archive::binary_oarchive &ar, const u_int version) const;
		
}

BOOST_CLASS_VERSION(slg::Film, 4)
BOOST_CLASS_VERSION(slg::Film::RadianceChannelScale, 1)

#endif	/* _SLG_FILM_H */
