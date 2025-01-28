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

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <spdlog/spdlog.h>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/utils.h"
#include "slg/slg.h"
#include "slg/engines/tilerepository.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/utils/filenameresolver.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/luxcorelogger.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;
using namespace luxcore::detail;
OIIO_NAMESPACE_USING

//------------------------------------------------------------------------------
// ParseLXS
//------------------------------------------------------------------------------

extern FILE *luxcore_parserlxs_yyin;
extern int luxcore_parserlxs_yyparse(void);
extern void luxcore_parserlxs_yyrestart(FILE *new_file);

namespace luxcore { namespace parselxs {
extern void IncludeClear();
extern void ResetParser();

extern string currentFile;
extern unsigned int lineNum;

extern Properties overwriteProps;
extern Properties *renderConfigProps;
extern Properties *sceneProps;

} }

void luxcore::ParseLXS(const string &fileName, Properties &renderConfigProps, Properties &sceneProps) {
	API_BEGIN("{}, {}, {}", ToArgString(fileName), ToArgString(renderConfigProps), ToArgString(sceneProps));

	// Otherwise the code is not thread-safe
	static boost::mutex parseLXSMutex;
	boost::unique_lock<boost::mutex> lock(parseLXSMutex);

	luxcore::parselxs::renderConfigProps = &renderConfigProps;
	luxcore::parselxs::sceneProps = &sceneProps;
	luxcore::parselxs::ResetParser();

	bool parseSuccess = false;

	if (fileName == "-")
		luxcore_parserlxs_yyin = stdin;
	else
		luxcore_parserlxs_yyin = fopen(fileName.c_str(), "r");

	if (luxcore_parserlxs_yyin != nullptr) {
		luxcore::parselxs::currentFile = fileName;
		if (luxcore_parserlxs_yyin == stdin)
			luxcore::parselxs::currentFile = "<standard input>";
		luxcore::parselxs::lineNum = 1;
		// Make sure to flush any buffers before parsing
		luxcore::parselxs::IncludeClear();
		luxcore_parserlxs_yyrestart(luxcore_parserlxs_yyin);
		try {
			parseSuccess = (luxcore_parserlxs_yyparse() == 0);

			// Overwrite properties with Renderer command one
			luxcore::parselxs::renderConfigProps->Set(luxcore::parselxs::overwriteProps);
		} catch (std::runtime_error &e) {
			throw runtime_error("Exception during parsing (file '" + luxcore::parselxs::currentFile +
					"', line: " + ToString(luxcore::parselxs::lineNum) + "): " + e.what());
		}
		
		if (luxcore_parserlxs_yyin != stdin)
			fclose(luxcore_parserlxs_yyin);
	} else
		throw runtime_error("Unable to read scene file: " + fileName);

	luxcore::parselxs::currentFile = "";
	luxcore::parselxs::lineNum = 0;

	if ((luxcore_parserlxs_yyin == nullptr) || !parseSuccess)
		throw runtime_error("Parsing failed: " + fileName);

	// For some debugging
	/*cout << "================ ParseLXS RenderConfig Properties ================\n";
	cout << renderConfigProps;
	cout << "================ ParseLXS RenderConfig Properties ================\n";
	cout << sceneProps;
	cout << "==================================================================\n";*/
	
	API_END();
}

//------------------------------------------------------------------------------
// MakeTx
//------------------------------------------------------------------------------

void luxcore::MakeTx(const string &srcFileName, const string &dstFileName) {
	slg::ImageMap::MakeTx(srcFileName, dstFileName);
}

//------------------------------------------------------------------------------
// GetPlatformDesc
//------------------------------------------------------------------------------

Properties luxcore::GetPlatformDesc() {
	API_BEGIN_NOARGS();

	Properties props;

	static const string luxCoreVersion(LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);
	props << Property("version.number")(luxCoreVersion);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	props << Property("compile.LUXRAYS_DISABLE_OPENCL")(!luxrays::isOpenCLAvilable);
	props << Property("compile.LUXRAYS_ENABLE_OPENCL")(luxrays::isOpenCLAvilable);
#else
	props << Property("compile.LUXRAYS_DISABLE_OPENCL")(true);
	props << Property("compile.LUXRAYS_ENABLE_OPENCL")(false);
#endif

#if !defined(LUXRAYS_DISABLE_CUDA)
	props << Property("compile.LUXRAYS_DISABLE_CUDA")(!luxrays::isCudaAvilable);
	props << Property("compile.LUXRAYS_ENABLE_CUDA")(luxrays::isCudaAvilable);
	props << Property("compile.LUXRAYS_ENABLE_OPTIX")(luxrays::isOptixAvilable);
#else
	props << Property("compile.LUXRAYS_DISABLE_CUDA")(true);
	props << Property("compile.LUXRAYS_ENABLE_CUDA")(false);
	props << Property("compile.LUXRAYS_ENABLE_OPTIX")(false);
#endif

#if !defined(LUXCORE_DISABLE_OIDN)
	props << Property("compile.LUXCORE_ENABLE_OIDN")(true);
	props << Property("compile.LUXCORE_DISABLE_OIDN")(false);
#else
	props << Property("compile.LUXCORE_ENABLE_OIDN")(false);
	props << Property("compile.LUXCORE_DISABLE_OIDN")(true);
#endif

	props << Property("compile.LUXCORE_DISABLE_EMBREE_BVH_BUILDER")(false);
	props << Property("compile.LC_MESH_MAX_DATA_COUNT")(LC_MESH_MAX_DATA_COUNT);

	API_RETURN("{}", ToArgString(props));

	return props;
}

//------------------------------------------------------------------------------
// GetOpenCLDeviceDescs
//------------------------------------------------------------------------------

Properties luxcore::GetOpenCLDeviceDescs() {
	API_BEGIN_NOARGS();

	Properties props;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	Context ctx;
	vector<DeviceDescription *> deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

	// Select only OpenCL devices
	DeviceDescription::Filter((DeviceType)(DEVICE_TYPE_OPENCL_ALL | DEVICE_TYPE_CUDA_ALL), deviceDescriptions);

	// Add all device information to the list
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		DeviceDescription *desc = deviceDescriptions[i];

		string platformName = "UNKNOWN";
		string platformVersion = "UNKNOWN";
		int deviceClock = 0;
		unsigned long long deviceLocalMem = 0;
		unsigned long long deviceConstMem = 0;
		if (desc->GetType() & DEVICE_TYPE_OPENCL_ALL) {
			OpenCLDeviceDescription *oclDesc = (OpenCLDeviceDescription *)deviceDescriptions[i];

			platformName = oclDesc->GetOpenCLPlatform();
			platformVersion = oclDesc->GetOpenCLVersion();
			deviceClock = oclDesc->GetClock();
			deviceLocalMem = oclDesc->GetLocalMem();
			deviceConstMem = oclDesc->GetConstMem();
		} else if (desc->GetType() & DEVICE_TYPE_CUDA_ALL) {
			platformName = "NVIDIA";
		}

		const string prefix = "opencl.device." + ToString(i);
		props <<
				Property(prefix + ".platform.name")(platformName) <<
				Property(prefix + ".platform.version")(platformVersion) <<
				Property(prefix + ".name")(desc->GetName()) <<
				Property(prefix + ".type")(DeviceDescription::GetDeviceType(desc->GetType())) <<
				Property(prefix + ".units")(desc->GetComputeUnits()) <<
				Property(prefix + ".clock")(deviceClock) <<
				Property(prefix + ".nativevectorwidthfloat")(desc->GetNativeVectorWidthFloat()) <<
				Property(prefix + ".maxmemory")((unsigned long long)desc->GetMaxMemory()) <<
				Property(prefix + ".maxmemoryallocsize")((unsigned long long)desc->GetMaxMemoryAllocSize()) <<
				Property(prefix + ".localmemory")((unsigned long long)deviceLocalMem) <<
				Property(prefix + ".constmemory")((unsigned long long)deviceConstMem);

#if !defined(LUXRAYS_DISABLE_CUDA)
		if (desc->GetType() & DEVICE_TYPE_CUDA_ALL) {
			const CUDADeviceDescription *cudaDesc = (CUDADeviceDescription *)desc;
			
			props <<
					Property(prefix + ".cuda.compute.major")(cudaDesc->GetCUDAComputeCapabilityMajor()) <<
					Property(prefix + ".cuda.compute.minor")(cudaDesc->GetCUDAComputeCapabilityMinor());
		}
#endif
	}
#endif
	
	API_RETURN("{}", ToArgString(props));

	return props;
}

//------------------------------------------------------------------------------
// FileNameResolver
//------------------------------------------------------------------------------

void luxcore::ClearFileNameResolverPaths() {
	API_BEGIN_NOARGS();
	
	slg::SLG_FileNameResolver.Clear();
	
	API_END();
}

void luxcore::AddFileNameResolverPath(const std::string &path) {
	API_BEGIN("{}", ToArgString(path));

	slg::SLG_FileNameResolver.AddPath(path);
	
	API_END();
}

vector<string> luxcore::GetFileNameResolverPaths() {
	API_BEGIN_NOARGS();

	const vector<string> &result = slg::SLG_FileNameResolver.GetPaths();
	
	API_RETURN("{}", ToArgString(result));

	return result;
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film *Film::Create(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	Film *result = new luxcore::detail::FilmImpl(fileName);
	
	API_RETURN("{}", (void *)result);
	
	return result;
}

Film *Film::Create(const luxrays::Properties &props,
		const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel) {
	API_BEGIN("{}, {}, {}", ToArgString(props), hasPixelNormalizedChannel, hasScreenNormalizedChannel);

	Film *result = new luxcore::detail::FilmImpl(props, hasPixelNormalizedChannel, hasScreenNormalizedChannel);

	API_RETURN("{}", (void *)result);
	
	return result;
}

Film::~Film() {
	API_BEGIN_NOARGS();
	API_END();
}

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	GetOutputFloat(type, buffer, index, executeImagePipeline);

	API_END();
}

template<> void Film::GetOutput<unsigned int>(const FilmOutputType type, unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	GetOutputUInt(type, buffer, index, executeImagePipeline);
	
	API_END();
}

template<> void Film::UpdateOutput<float>(const FilmOutputType type, const float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	UpdateOutputFloat(type, buffer, index, executeImagePipeline);
	
	API_END();
}

template<> void Film::UpdateOutput<unsigned int>(const FilmOutputType type, const unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	UpdateOutputUInt(type, buffer, index, executeImagePipeline);
	
	API_END();
}

template<> const float *Film::GetChannel<float>(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const float *result = GetChannelFloat(type, index, executeImagePipeline);

	API_RETURN("{}", (void *)result);
	
	return result;
}

template<> const unsigned int *Film::GetChannel(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const unsigned int *result =  GetChannelUInt(type, index, executeImagePipeline);

	API_RETURN("{}", (void *)result);
	
	return result;
}

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------

Camera::~Camera() {
	API_BEGIN_NOARGS();
	API_END();
}

//------------------------------------------------------------------------------
// Scene
//------------------------------------------------------------------------------

Scene *Scene::Create(const luxrays::Properties *resizePolicyProps) {
	API_BEGIN("{}", (void *)resizePolicyProps);

	Scene *result = new luxcore::detail::SceneImpl(resizePolicyProps);

	API_RETURN("{}", (void *)result);
	
	return result;
}

Scene *Scene::Create(const luxrays::Properties &props, const luxrays::Properties *resizePolicyProps) {
	API_BEGIN("{}, {}", ToArgString(props), (void *)resizePolicyProps);

	Scene *result = new luxcore::detail::SceneImpl(resizePolicyProps);

	API_RETURN("{}", (void *)result);
	
	return result;
}

Scene *Scene::Create(const string &fileName, const luxrays::Properties *resizePolicyProps) {
	API_BEGIN("{}, {}", ToArgString(fileName), (void *)resizePolicyProps);

	Scene *result = new luxcore::detail::SceneImpl(fileName, resizePolicyProps);

	API_RETURN("{}", (void *)result);
	
	return result;
}

Scene::~Scene() {
	API_BEGIN_NOARGS();
	API_END();
}

template<> void Scene::DefineImageMap<unsigned char>(const std::string &imgMapName,
		unsigned char *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType, Scene::WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels,
			gamma, channels, width, height, ToArgString(selectionType), ToArgString(wrapType));

	DefineImageMapUChar(imgMapName, pixels, gamma, channels, width, height,
			selectionType, wrapType);
	
	API_END();
}

template<> void Scene::DefineImageMap<unsigned short>(const std::string &imgMapName,
		unsigned short *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType, Scene::WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels,
			gamma, channels, width, height, ToArgString(selectionType), ToArgString(wrapType));

	DefineImageMapHalf(imgMapName, pixels, gamma, channels, width, height,
			selectionType, wrapType);
	
	API_END();
}

template<> void Scene::DefineImageMap<float>(const std::string &imgMapName,
		float *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType, Scene::WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels,
			gamma, channels, width, height, ToArgString(selectionType), ToArgString(wrapType));

	DefineImageMapFloat(imgMapName, pixels, gamma, channels, width, height,
			selectionType, wrapType);
	
	API_END();
}

float *Scene::AllocVerticesBuffer(const unsigned int meshVertCount) {
	API_BEGIN("{}", meshVertCount);

	float *result = (float *)luxcore::detail::SceneImpl::AllocVerticesBuffer(meshVertCount);
	
	API_RETURN("{}", (void *)result);
	
	return result;
}

unsigned int *Scene::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	API_BEGIN("{}", meshTriCount);

	unsigned int *result =  (unsigned int *)luxcore::detail::SceneImpl::AllocTrianglesBuffer(meshTriCount);

	API_RETURN("{}", (void *)result);
	
	return result;
}

//------------------------------------------------------------------------------
// RenderConfig
//------------------------------------------------------------------------------

RenderConfig *RenderConfig::Create(const Properties &props, Scene *scn) {
	API_BEGIN("{}, {}", ToArgString(props), (void *)scn);

	luxcore::detail::SceneImpl *scnImpl = dynamic_cast<luxcore::detail::SceneImpl *>(scn);

	RenderConfig *result = new luxcore::detail::RenderConfigImpl(props, scnImpl);

	API_RETURN("{}", (void *)result);
	
	return result;
}

RenderConfig *RenderConfig::Create(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	RenderConfig *result = new luxcore::detail::RenderConfigImpl(fileName);

	API_RETURN("{}", (void *)result);
	
	return result;
}

RenderConfig *RenderConfig::Create(const std::string &fileName, RenderState **startState,
		Film **startFilm) {
	API_BEGIN("{}, {}, {}", ToArgString(fileName), (void *)startState, (void *)startFilm);

	luxcore::detail::RenderStateImpl *ss;
	luxcore::detail::FilmImpl *sf;
	RenderConfig *rcfg = new luxcore::detail::RenderConfigImpl(fileName, &ss, &sf);
	
	*startState = ss;
	*startFilm = sf;

	API_RETURN("{}", (void *)rcfg);
	
	return rcfg;
}

RenderConfig::~RenderConfig() {
	API_BEGIN_NOARGS();
	API_END();
}

const Properties &RenderConfig::GetDefaultProperties() {
	API_BEGIN_NOARGS();

	const Properties &result = luxcore::detail::RenderConfigImpl::GetDefaultProperties();
	
	API_RETURN("{}", ToArgString(result));
	
	return result;
}

//------------------------------------------------------------------------------
// RenderState
//------------------------------------------------------------------------------

RenderState *RenderState::Create(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	RenderState *result = new luxcore::detail::RenderStateImpl(fileName);

	API_RETURN("{}", ToArgString(result));
	
	return result;
}

RenderState::~RenderState() {
	API_BEGIN_NOARGS();
	API_END();
}

//------------------------------------------------------------------------------
// RenderSession
//------------------------------------------------------------------------------

RenderSession *RenderSession::Create(const RenderConfig *config, RenderState *startState, Film *startFilm) {
	API_BEGIN("{}, {}, {}", (void *)config, (void *)startState, (void *)startFilm);

	const luxcore::detail::RenderConfigImpl *configImpl = dynamic_cast<const luxcore::detail::RenderConfigImpl *>(config);
	luxcore::detail::RenderStateImpl *startStateImpl = dynamic_cast<luxcore::detail::RenderStateImpl *>(startState);
	luxcore::detail::FilmImpl *startFilmImpl = dynamic_cast<luxcore::detail::FilmImpl *>(startFilm);

	RenderSession *result = new luxcore::detail::RenderSessionImpl(configImpl, startStateImpl, startFilmImpl);

	API_RETURN("{}", ToArgString(result));
	
	return result;
}

RenderSession *RenderSession::Create(const RenderConfig *config, const std::string &startStateFileName,
		const std::string &startFilmFileName) {
	API_BEGIN("{}, {}, {}", (void *)config, ToArgString(startStateFileName), ToArgString(startFilmFileName));

	const luxcore::detail::RenderConfigImpl *configImpl = dynamic_cast<const luxcore::detail::RenderConfigImpl *>(config);

	RenderSession *result = new luxcore::detail::RenderSessionImpl(configImpl, startStateFileName, startFilmFileName);

	API_RETURN("{}", ToArgString(result));
	
	return result;
}

RenderSession::~RenderSession() {
	API_BEGIN_NOARGS();
	API_END();
}
