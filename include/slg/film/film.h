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
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/utils/oclcache.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/framebuffer.h"
#include "slg/film/filmoutputs.h"
#include "slg/film/filmconvtest.h"
#include "slg/utils/varianceclamping.h"

namespace slg {

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

class SampleResult;

class Film {
public:
	typedef enum {
		RADIANCE_PER_PIXEL_NORMALIZED = 1 << 0,
		RADIANCE_PER_SCREEN_NORMALIZED = 1 << 1,
		ALPHA = 1 << 2,
		// RGB_TONEMAPPED is deprecated and replaced by IMAGEPIPELINE
		IMAGEPIPELINE = 1 << 3,
		DEPTH = 1 << 4,
		POSITION = 1 << 5,
		GEOMETRY_NORMAL = 1 << 6,
		SHADING_NORMAL = 1 << 7,
		MATERIAL_ID = 1 << 8,
		DIRECT_DIFFUSE = 1 << 9,
		DIRECT_GLOSSY = 1 << 10,
		EMISSION = 1 << 11,
		INDIRECT_DIFFUSE = 1 << 12,
		INDIRECT_GLOSSY = 1 << 13,
		INDIRECT_SPECULAR = 1 << 14,
		MATERIAL_ID_MASK = 1 << 15,
		DIRECT_SHADOW_MASK = 1 << 16,
		INDIRECT_SHADOW_MASK = 1 << 17,
		UV = 1 << 18,
		RAYCOUNT = 1 << 19,
		BY_MATERIAL_ID = 1 << 20,
		IRRADIANCE = 1 << 21,
		OBJECT_ID = 1 << 22,
		OBJECT_ID_MASK = 1 << 23,
		BY_OBJECT_ID = 1 << 24,
		FRAMEBUFFER_MASK = 1 << 25
	} FilmChannelType;

	class RadianceChannelScale {
	public:
		RadianceChannelScale();

		void Init();

		// It is very important for performance to have Scale() methods in-lined
		void Scale(float v[3]) const {
			v[0] *= scale.c[0];
			v[1] *= scale.c[1];
			v[2] *= scale.c[2];
		}

		luxrays::Spectrum Scale(const luxrays::Spectrum &v) const {
			return v * scale;
		}

		const luxrays::Spectrum &GetScale() const { return scale; }
		
		float globalScale, temperature;
		luxrays::Spectrum rgbScale;
		bool enabled;

		friend class boost::serialization::access;

	private:
		template<class Archive> void serialize(Archive &ar, const u_int version);

		luxrays::Spectrum scale;
	};

	Film(const u_int width, const u_int height, const u_int *subRegion = NULL);
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
	u_int GetMaskObjectIDCount() const { return maskObjectIDs.size(); }
	u_int GetMaskObjectID(const u_int index) const { return maskObjectIDs[index]; }
	u_int GetByObjectIDCount() const { return byObjectIDs.size(); }
	u_int GetByObjectID(const u_int index) const { return byObjectIDs[index]; }

	void Init();
	void Resize(const u_int w, const u_int h);
	void Reset();
	void Clear();
	void Parse(const luxrays::Properties &props);

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void SetOverlappedScreenBufferUpdateFlag(const bool overlappedScreenBufferUpdate) {
		enabledOverlappedScreenBufferUpdate = overlappedScreenBufferUpdate;
	}
	bool IsOverlappedScreenBufferUpdate() const { return enabledOverlappedScreenBufferUpdate; }

	void SetImagePipelines(ImagePipeline *newImagePiepeline);
	void SetImagePipelines(std::vector<ImagePipeline *> &newImagePiepelines);
	const ImagePipeline *GetImagePipeline(const u_int index) const { return imagePipelines[index]; }

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

	void ExecuteImagePipeline(const u_int index);

	//--------------------------------------------------------------------------

	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	const u_int *GetSubRegion() const { return subRegion; }
	double GetTotalSampleCount() const {
		return statsTotalSampleCount;
	}
	double GetTotalTime() const {
		return luxrays::WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		return GetTotalSampleCount() / GetTotalTime();
	}
	void GetSampleXY(const float u0, const float u1, float *filmX, float *filmY) const {
		*filmX = luxrays::Min<float>(subRegion[0] + u0 * (subRegion[1] - subRegion[0] + 1), (float)(width - 1));
		*filmY = luxrays::Min<float>(subRegion[2] + u1 * (subRegion[3] - subRegion[2] + 1), (float)(height - 1));
	}

	//--------------------------------------------------------------------------
	// Halt tests related methods
	//--------------------------------------------------------------------------

	void ResetHaltTests();
	void RunHaltTests();
	// Convergence can be set by external source (like TileRepository convergence test)
	void SetConvergence(const float conv) { statsConvergence = conv; }
	float GetConvergence() { return statsConvergence; }

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

#if !defined(LUXRAYS_DISABLE_OPENCL)
	void ReadOCLBuffer_IMAGEPIPELINE(const u_int index);
	void WriteOCLBuffer_IMAGEPIPELINE(const u_int index);
#endif

	std::vector<GenericFrameBuffer<4, 1, float> *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	GenericFrameBuffer<2, 1, float> *channel_ALPHA;
	std::vector<GenericFrameBuffer<3, 0, float> *> channel_IMAGEPIPELINEs;
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
	GenericFrameBuffer<1, 0, u_int> *channel_OBJECT_ID;
	std::vector<GenericFrameBuffer<2, 1, float> *> channel_OBJECT_ID_MASKs;
	std::vector<GenericFrameBuffer<4, 1, float> *> channel_BY_OBJECT_IDs;
	// This AOV is the result of the work done to run the image pipeline. Like
	// channel_IMAGEPIPELINEs, it is the only AOV updated only after having run
	// the image pipeline. It is updated inside MergeSampleBuffers().
	GenericFrameBuffer<1, 0, u_int> *channel_FRAMEBUFFER_MASK;

	// (Optional) OpenCL context
	bool oclEnable;
	int oclPlatformIndex;
	int oclDeviceIndex;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	luxrays::Context *ctx;
	luxrays::OpenCLDeviceDescription *selectedDeviceDesc;
	luxrays::OpenCLIntersectionDevice *oclIntersectionDevice;

	luxrays::oclKernelCache *kernelCache;

	cl::Buffer *ocl_IMAGEPIPELINE;
	cl::Buffer *ocl_FRAMEBUFFER_MASK;
	cl::Buffer *ocl_ALPHA;
	cl::Buffer *ocl_OBJECT_ID;
	
	cl::Buffer *ocl_mergeBuffer;
	
	cl::Kernel *clearFRAMEBUFFER_MASKKernel;
	cl::Kernel *mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel;
	cl::Kernel *mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel;
	cl::Kernel *notOverlappedScreenBufferUpdateKernel;
#endif

	static Film *LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, const Film *film);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);

	static FilmChannelType String2FilmChannelType(const std::string &type);
	static const std::string FilmChannelType2String(const FilmChannelType type);

	friend class boost::serialization::access;

private:
	// Used by serialization
	Film();

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	void FreeChannels();
	void MergeSampleBuffers(const u_int index);
	void GetPixelFromMergedSampleBuffers(const u_int index, float *c) const;
	void GetPixelFromMergedSampleBuffers(const u_int x, const u_int y, float *c) const {
		GetPixelFromMergedSampleBuffers(x + y * width, c);
	}

	void ParseRadianceGroupsScale(const luxrays::Properties &props);
	void ParseOutputs(const luxrays::Properties &props);

	void SetUpOCL();
#if !defined(LUXRAYS_DISABLE_OPENCL)
	void CreateOCLContext();
	void DeleteOCLContext();
	void AllocateOCLBuffers();
	void CompileOCLKernels();
	void WriteAllOCLBuffers();
	void MergeSampleBuffersOCL(const u_int index);
#endif

	static ImagePipeline *AllocImagePipeline(const luxrays::Properties &props, const std::string &prefix);
	static std::vector<ImagePipeline *>AllocImagePipelines(const luxrays::Properties &props);

	std::set<FilmChannelType> channels;
	u_int width, height, pixelCount, radianceGroupCount;
	u_int subRegion[4];
	std::vector<u_int> maskMaterialIDs, byMaterialIDs;
	std::vector<u_int> maskObjectIDs, byObjectIDs;

	// Used to speedup sample splatting, initialized inside Init()
	bool hasDataChannel, hasComposingChannel;

	double statsTotalSampleCount, statsStartSampleTime, statsConvergence;

	std::vector<ImagePipeline *> imagePipelines;

	// Halt conditions
	FilmConvTest *convTest;
	double haltTime;
	u_int haltSPP;

	std::vector<RadianceChannelScale> radianceChannelScales;
	FilmOutputs filmOutputs;

	bool initialized, enabledOverlappedScreenBufferUpdate;	
};

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index);
template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index);
template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer, const u_int index);
template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer, const u_int index);

}

BOOST_CLASS_VERSION(slg::Film, 9)
BOOST_CLASS_VERSION(slg::Film::RadianceChannelScale, 1)

BOOST_CLASS_EXPORT_KEY(slg::Film)

#endif	/* _SLG_FILM_H */
