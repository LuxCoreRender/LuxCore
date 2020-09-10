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

#ifndef _SLG_FILM_H
#define	_SLG_FILM_H

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_set>

#include <boost/thread/mutex.hpp>
#include <bcd/core/SamplesAccumulator.h>

#include "luxrays/core/hardwaredevice.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/film/framebuffer.h"
#include "slg/film/filmoutputs.h"
#include "slg/film/convtest/filmconvtest.h"
#include "slg/film/noiseestimation/filmnoiseestimation.h"
#include "slg/film/denoiser/filmdenoiser.h"
#include "slg/utils/varianceclamping.h"
#include "denoiser/filmdenoiser.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/film/film_types.cl"
}

//------------------------------------------------------------------------------
// FilmSamplesCount
//------------------------------------------------------------------------------

class FilmSamplesCounts {
public:
	FilmSamplesCounts();
	~FilmSamplesCounts();

	void Init(const u_int threadCount);
	void Clear();

	void SetSampleCount(const double totalSampleCount,
			const double RADIANCE_PER_PIXEL_NORMALIZED_count,
			const double RADIANCE_PER_SCREEN_NORMALIZED_count);
	void AddSampleCount(const double totalSampleCount,
			const double RADIANCE_PER_PIXEL_NORMALIZED_count,
			const double RADIANCE_PER_SCREEN_NORMALIZED_count);

	void AddSampleCount(const u_int threadIndex,
			const double RADIANCE_PER_PIXEL_NORMALIZED_count,
			const double RADIANCE_PER_SCREEN_NORMALIZED_count);

	double GetSampleCount() const;
	double GetSampleCount_RADIANCE_PER_PIXEL_NORMALIZED() const;
	double GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED() const;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const unsigned int version) {
		ar & threadCount;
		ar & total_SampleCount;
		ar & RADIANCE_PER_PIXEL_NORMALIZED_SampleCount;
		ar & RADIANCE_PER_SCREEN_NORMALIZED_SampleCount;
	}

	u_int threadCount;
	std::vector<double> total_SampleCount;
	std::vector<double> RADIANCE_PER_PIXEL_NORMALIZED_SampleCount, RADIANCE_PER_SCREEN_NORMALIZED_SampleCount;
};

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

class SampleResult;
class ImagePipeline;

class Film {
public:
	typedef enum {
		RADIANCE_PER_PIXEL_NORMALIZED,
		RADIANCE_PER_SCREEN_NORMALIZED,
		ALPHA,
		// RGB_TONEMAPPED is deprecated and replaced by IMAGEPIPELINE
		IMAGEPIPELINE,
		DEPTH,
		POSITION,
		GEOMETRY_NORMAL,
		SHADING_NORMAL,
		MATERIAL_ID,
		DIRECT_DIFFUSE,
		DIRECT_DIFFUSE_REFLECT,
		DIRECT_DIFFUSE_TRANSMIT,
		DIRECT_GLOSSY,
		DIRECT_GLOSSY_REFLECT,
		DIRECT_GLOSSY_TRANSMIT,
		EMISSION,
		INDIRECT_DIFFUSE,
		INDIRECT_DIFFUSE_REFLECT,
		INDIRECT_DIFFUSE_TRANSMIT,
		INDIRECT_GLOSSY,
		INDIRECT_GLOSSY_REFLECT,
		INDIRECT_GLOSSY_TRANSMIT,
		INDIRECT_SPECULAR,
		INDIRECT_SPECULAR_REFLECT,
		INDIRECT_SPECULAR_TRANSMIT,
		MATERIAL_ID_MASK,
		DIRECT_SHADOW_MASK,
		INDIRECT_SHADOW_MASK,
		UV,
		RAYCOUNT,
		BY_MATERIAL_ID,
		IRRADIANCE,
		OBJECT_ID,
		OBJECT_ID_MASK,
		BY_OBJECT_ID,
		SAMPLECOUNT,
		CONVERGENCE,
		MATERIAL_ID_COLOR,
		ALBEDO,
		AVG_SHADING_NORMAL,
		NOISE,
		USER_IMPORTANCE
	} FilmChannelType;
	
	typedef std::unordered_set<FilmChannelType, std::hash<int> > FilmChannels;

	Film(const u_int width, const u_int height, const u_int *subRegion = NULL);
	~Film();

	void SetThreadCount(const u_int threadCount);

	void Init();
	bool IsInitiliazed() const { return initialized; }
	void Resize(const u_int w, const u_int h);
	void Reset(const bool onlyCounters = false);
	void Clear();
	void Parse(const luxrays::Properties &props);

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void SetImagePipelines(const u_int index, ImagePipeline *newImagePiepeline);
	void SetImagePipelines(ImagePipeline *newImagePiepeline);
	void SetImagePipelines(std::vector<ImagePipeline *> &newImagePiepelines);
	const u_int GetImagePipelineCount() const { return imagePipelines.size(); }
	const ImagePipeline *GetImagePipeline(const u_int index) const { return imagePipelines[index]; }

	void CopyDynamicSettings(const Film &film);
	void CopyHaltSettings(const Film &film);

	//--------------------------------------------------------------------------

	float GetFilmY(const u_int imagePipeLineIndex) const;
	float GetFilmMaxValue(const u_int imagePipeLineIndex) const;

	void SetFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void SetFilm(const Film &film) {
		SetFilm(film, 0, 0, width, height, 0, 0);
	}

	void AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void AddFilm(const Film &film) {
		AddFilm(film, 0, 0, width, height, 0, 0);
	}

	//--------------------------------------------------------------------------
	// Channels
	//--------------------------------------------------------------------------

	bool HasChannel(const FilmChannelType type) const { return channels.count(type) > 0; }
	u_int GetChannelCount(const FilmChannelType type) const;
	const FilmChannels &GetChannels() const { return channels; }

	// This one must be called before Init()
	void AddChannel(const FilmChannelType type,
		const luxrays::Properties *prop = NULL);
	// This one must be called before Init()
	void RemoveChannel(const FilmChannelType type);
	// This one must be called before Init()
	void SetRadianceGroupCount(const u_int count) {
		// I can not have less than 1 radiance group
		radianceGroupCount = luxrays::Max(count, 1u);
	}

	u_int GetRadianceGroupCount() const { return radianceGroupCount; }
	u_int GetMaskMaterialID(const u_int index) const { return maskMaterialIDs[index]; }
	u_int GetByMaterialID(const u_int index) const { return byMaterialIDs[index]; }
	u_int GetMaskObjectID(const u_int index) const { return maskObjectIDs[index]; }
	u_int GetByObjectID(const u_int index) const { return byObjectIDs[index]; }

	template<class T> T *GetChannel(const FilmChannelType type, const u_int index = 0,
			const bool executeImagePipeline = true) {
		throw std::runtime_error("Called Film::GetChannel() with wrong type");
	}

	bool HasDataChannel() { return hasDataChannel; }
	bool HasComposingChannel() { return hasComposingChannel; }

	void AsyncExecuteImagePipeline(const u_int index);
	void WaitAsyncExecuteImagePipeline();
	bool HasDoneAsyncExecuteImagePipeline();
	void ExecuteImagePipeline(const u_int index);

	//--------------------------------------------------------------------------
	// Outputs
	//--------------------------------------------------------------------------

	bool HasOutput(const FilmOutputs::FilmOutputType type) const;
	u_int GetOutputCount(const FilmOutputs::FilmOutputType type) const;
	size_t GetOutputSize(const FilmOutputs::FilmOutputType type) const;

	void Output();
	void Output(const std::string &fileName, const FilmOutputs::FilmOutputType type,
			const luxrays::Properties *props = NULL, const bool executeImagePipeline = true);

	template<class T> void GetOutput(const FilmOutputs::FilmOutputType type, T *buffer,
			const u_int index = 0, const bool executeImagePipeline = true) {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	//--------------------------------------------------------------------------

	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	u_int GetPixelCount() const { return pixelCount; }
	const u_int *GetSubRegion() const { return subRegion; }
	double GetTotalSampleCount() const {
		return samplesCounts.GetSampleCount();
	}
	double GetTotalEyeSampleCount() const {
		return samplesCounts.GetSampleCount_RADIANCE_PER_PIXEL_NORMALIZED();
	}
	double GetTotalLightSampleCount() const {
		return samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();
	}
	double GetTotalTime() const {
		return luxrays::WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		const double t = GetTotalTime();
		return (t > 0.0) ? (GetTotalSampleCount() / t) : 0.0;
	}
	double GetAvgEyeSampleSec() {
		const double t = GetTotalTime();
		return (t > 0.0) ? (GetTotalEyeSampleCount() / t) : 0.0;
	}
	double GetAvgLightSampleSec() {
		const double t = GetTotalTime();
		return (t > 0.0) ? (GetTotalLightSampleCount() / t) : 0.0;
	}

	//--------------------------------------------------------------------------
	// Tests related methods (halt conditions, noise estimation, etc.)
	//--------------------------------------------------------------------------

	void ResetTests();
	void RunTests();
	// Convergence can be set by external source (like TileRepository convergence test)
	void SetConvergence(const float conv) { statsConvergence = conv; }
	float GetConvergence() { return statsConvergence; }

	//--------------------------------------------------------------------------
	// Used by BCD denoiser plugin
	//--------------------------------------------------------------------------

	const FilmDenoiser &GetDenoiser() const { return filmDenoiser; }
	FilmDenoiser &GetDenoiser() { return filmDenoiser; }

	//--------------------------------------------------------------------------
	// Samples related methods
	//--------------------------------------------------------------------------

	void SetSampleCount(const double totalSampleCount,
			const double RADIANCE_PER_PIXEL_NORMALIZED_count,
			const double RADIANCE_PER_SCREEN_NORMALIZED_count);
	void AddSampleCount(const u_int threadIndex,
			const double RADIANCE_PER_PIXEL_NORMALIZED_count,
			const double RADIANCE_PER_SCREEN_NORMALIZED_count);

	// Normal method versions
	void AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight = 1.f);
	void AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult);
	
	// Atomic method versions
	void AtomicAddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight = 1.f);
	void AtomicAddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AtomicAddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult);

	void ReadHWBuffer_IMAGEPIPELINE(const u_int index);
	void WriteHWBuffer_IMAGEPIPELINE(const u_int index);

	void GetPixelFromMergedSampleBuffers(
		const bool use_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool use_RADIANCE_PER_SCREEN_NORMALIZEDs,
		const std::vector<RadianceChannelScale> *radianceChannelScales,
		const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
		const u_int index, float *c) const;
	void GetPixelFromMergedSampleBuffers(
		const bool use_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool use_RADIANCE_PER_SCREEN_NORMALIZEDs,
		const std::vector<RadianceChannelScale> *radianceChannelScales,
		const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
		const u_int x, const u_int y, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int imagePipelineIndex,
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
			const u_int x, const u_int y, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int imagePipelineIndex,
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
			const u_int index, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int imagePipelineIndex,
			const bool use_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool use_RADIANCE_PER_SCREEN_NORMALIZEDs,
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
			const u_int x, const u_int y, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int imagePipelineIndex,
			const bool use_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool use_RADIANCE_PER_SCREEN_NORMALIZEDs,
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
			const u_int index, float *c) const;
	
	bool HasThresholdSamples(const bool has_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool has_RADIANCE_PER_SCREEN_NORMALIZEDs,
			const u_int index, const float threshold) const {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (has_RADIANCE_PER_PIXEL_NORMALIZEDs && channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(index)[3] > threshold)
				return true;

			if (has_RADIANCE_PER_SCREEN_NORMALIZEDs) {
				luxrays::Spectrum s(channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(index));

				if (!s.Black())
					return true;
			}
		}

		return false;
	}
		
	bool HasThresholdSamples(const bool has_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool has_RADIANCE_PER_SCREEN_NORMALIZEDs,
		const u_int x, const u_int y, const float threshold) const {
		return HasSamples(has_RADIANCE_PER_PIXEL_NORMALIZEDs, has_RADIANCE_PER_SCREEN_NORMALIZEDs, x + y * width, threshold);
	}
	
	bool HasSamples(const bool has_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool has_RADIANCE_PER_SCREEN_NORMALIZEDs,
			const u_int index) const {
		return HasThresholdSamples(has_RADIANCE_PER_PIXEL_NORMALIZEDs, has_RADIANCE_PER_SCREEN_NORMALIZEDs, index, 0.f);
	}
		
	bool HasSamples(const bool has_RADIANCE_PER_PIXEL_NORMALIZEDs, const bool has_RADIANCE_PER_SCREEN_NORMALIZEDs,
		const u_int x, const u_int y) const {
		return HasThresholdSamples(has_RADIANCE_PER_PIXEL_NORMALIZEDs, has_RADIANCE_PER_SCREEN_NORMALIZEDs, x + y * width, 0.f);
	}


	std::vector<GenericFrameBuffer<4, 1, float> *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	GenericFrameBuffer<2, 1, float> *channel_ALPHA;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_IMAGEPIPELINEs;
	GenericFrameBuffer<1, 0, float> *channel_DEPTH;
	GenericFrameBuffer<3, 0, float> *channel_POSITION;
	GenericFrameBuffer<3, 0, float> *channel_GEOMETRY_NORMAL;
	GenericFrameBuffer<3, 0, float> *channel_SHADING_NORMAL;
	GenericFrameBuffer<4, 1, float> *channel_AVG_SHADING_NORMAL;
	GenericFrameBuffer<1, 0, u_int> *channel_MATERIAL_ID;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_DIFFUSE;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_DIFFUSE_REFLECT;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_DIFFUSE_TRANSMIT;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_GLOSSY;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_GLOSSY_REFLECT;
	GenericFrameBuffer<4, 1, float> *channel_DIRECT_GLOSSY_TRANSMIT;
	GenericFrameBuffer<4, 1, float> *channel_EMISSION;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_DIFFUSE;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_DIFFUSE_REFLECT;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_DIFFUSE_TRANSMIT;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_GLOSSY;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_GLOSSY_REFLECT;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_GLOSSY_TRANSMIT;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_SPECULAR;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_SPECULAR_REFLECT;
	GenericFrameBuffer<4, 1, float> *channel_INDIRECT_SPECULAR_TRANSMIT;
	std::vector<GenericFrameBuffer<2, 1, float> *> channel_MATERIAL_ID_MASKs;
	GenericFrameBuffer<2, 1, float> *channel_DIRECT_SHADOW_MASK;
	GenericFrameBuffer<2, 1, float> *channel_INDIRECT_SHADOW_MASK;
	GenericFrameBuffer<2, 0, float> *channel_UV;
	GenericFrameBuffer<1, 0, float> *channel_RAYCOUNT;
	std::vector<GenericFrameBuffer<4, 1, float> *> channel_BY_MATERIAL_IDs;
	GenericFrameBuffer<4, 1, float> *channel_IRRADIANCE;
	GenericFrameBuffer<1, 0, u_int> *channel_OBJECT_ID;
	std::vector<GenericFrameBuffer<2, 1, float> *> channel_OBJECT_ID_MASKs;
	std::vector<GenericFrameBuffer<4, 1, float> *> channel_BY_OBJECT_IDs;
	GenericFrameBuffer<1, 0, u_int> *channel_SAMPLECOUNT;
	GenericFrameBuffer<1, 0, float> *channel_CONVERGENCE;
	GenericFrameBuffer<4, 1, float> *channel_MATERIAL_ID_COLOR;
	GenericFrameBuffer<4, 1, float> *channel_ALBEDO;
	GenericFrameBuffer<1, 0, float> *channel_NOISE;
	GenericFrameBuffer<1, 0, float> *channel_USER_IMPORTANCE;

	// (Optional) LuxRays HardwareDevice context
	bool hwEnable;
	int hwDeviceIndex;

	luxrays::Context *ctx;
	luxrays::DataSet *dataSet;
	luxrays::HardwareDevice *hardwareDevice;

	luxrays::HardwareDeviceBuffer *hw_IMAGEPIPELINE;
	luxrays::HardwareDeviceBuffer *hw_ALPHA;
	luxrays::HardwareDeviceBuffer *hw_OBJECT_ID;
	luxrays::HardwareDeviceBuffer *hw_ALBEDO;
	luxrays::HardwareDeviceBuffer *hw_AVG_SHADING_NORMAL;
	
	luxrays::HardwareDeviceBuffer *hw_mergeBuffer;
	
	luxrays::HardwareDeviceKernel *mergeInitializeKernel;
	luxrays::HardwareDeviceKernel *mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel;
	luxrays::HardwareDeviceKernel *mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel;
	luxrays::HardwareDeviceKernel *mergeFinalizeKernel;

	static Film *LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, const Film *film);

	static bool GetFilmSize(const luxrays::Properties &cfg,
		u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Film *FromProperties(const luxrays::Properties &cfg);

	static FilmChannelType String2FilmChannelType(const std::string &type);
	static const std::string FilmChannelType2String(const FilmChannelType type);

	friend class FilmDenoiser;
	friend class boost::serialization::access;

private:
	// Used by serialization
	Film();

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	void FreeChannels();
	void MergeSampleBuffers(const u_int imagePipelineIndex);

	void ParseRadianceGroupsScale(const luxrays::Properties &props, const u_int imagePipelineIndex,
			const std::string &radianceGroupsScalePrefix);
	void ParseRadianceGroupsScales(const luxrays::Properties &props);
	void ParseOutputs(const luxrays::Properties &props);
	ImagePipeline *CreateImagePipeline(const luxrays::Properties &props,
			const std::string &imagePipelinePrefix);
	void ParseImagePipelines(const luxrays::Properties &props);

	void SetUpHW();
	void CreateHWContext();
	void DeleteHWContext();
	void AllocateHWBuffers();
	void CompileHWKernels();
	void WriteAllHWBuffers();
	void MergeSampleBuffersHW(const u_int imagePipelineIndex);

	void ExecuteImagePipelineThreadImpl(const u_int index);
	void ExecuteImagePipelineImpl(const u_int index);

	template <bool overwrite>
	void AddFilmImpl(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);

	FilmChannels channels;
	u_int width, height, pixelCount, radianceGroupCount;
	u_int subRegion[4];
	std::vector<u_int> maskMaterialIDs, byMaterialIDs;
	std::vector<u_int> maskObjectIDs, byObjectIDs;

	// Used to speedup sample splatting, initialized inside Init()
	bool hasDataChannel, hasComposingChannel;

	double statsStartSampleTime, statsConvergence;
	FilmSamplesCounts samplesCounts;

	std::vector<ImagePipeline *> imagePipelines;
	boost::thread *imagePipelineThread;
	bool isAsyncImagePipelineRunning;

	// Halt conditions
	FilmConvTest *convTest;
	double haltTime;
	u_int haltSPP, haltSPP_PixelNormalized, haltSPP_ScreenNormalized;
	
	float haltNoiseThreshold;
	u_int haltNoiseThresholdWarmUp, haltNoiseThresholdTestStep, haltNoiseThresholdImagePipelineIndex;
	bool haltNoiseThresholdUseFilter, haltNoiseThresholdStopRendering;

	// Adaptive sampling
	FilmNoiseEstimation *noiseEstimation;

	u_int noiseEstimationWarmUp, noiseEstimationTestStep;
	u_int noiseEstimationFilterScale;
	u_int noiseEstimationImagePipelineIndex;

	FilmOutputs filmOutputs;

	FilmDenoiser filmDenoiser;
	
	bool initialized;
};

template<> float *Film::GetChannel<float>(const FilmChannelType type, const u_int index, const bool executeImagePipeline);
template<> u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index, const bool executeImagePipeline);
template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer, const u_int index, const bool executeImagePipeline);
template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer, const u_int index, const bool executeImagePipeline);

}

BOOST_CLASS_VERSION(slg::Film, 27)

BOOST_CLASS_EXPORT_KEY(slg::Film)

#endif	/* _SLG_FILM_H */
