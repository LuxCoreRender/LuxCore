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
#include <memory>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/utils.h"
#include "slg/slg.h"
#include "slg/usings.h"
#include "slg/engines/tilerepository.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/utils/filenameresolver.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/luxcorelogger.h"

#if !defined(LUXRAYS_DISABLE_CUDA)
#include "luxrays/utils/cudacache.h"
#endif
#if !defined(LUXRAYS_DISABLE_OPENCL)
#include "luxrays/utils/oclcache.h"
#endif

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
extern PropertiesPtr renderConfigProps;
extern PropertiesPtr sceneProps;

} }

void luxcore::ParseLXS(
	const string &fileName,
	PropertiesRPtr p_renderConfigProps,
	PropertiesRPtr p_sceneProps
) {
	API_BEGIN("{}, {}, {}", ToArgString(fileName), ToArgString(p_renderConfigProps), ToArgString(p_sceneProps));

	// Otherwise the code is not thread-safe
	static std::mutex parseLXSMutex;
	std::unique_lock<std::mutex> lock(parseLXSMutex);

	luxcore::parselxs::renderConfigProps = p_renderConfigProps.get();
	luxcore::parselxs::sceneProps = p_sceneProps.get();
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

PropertiesUPtr luxcore::GetPlatformDesc() {
	API_BEGIN_NOARGS();

	auto propsPtr = std::make_unique<Properties>();
	PropertiesRef props = *propsPtr;;

	static const string luxCoreVersion(LUXCORE_VERSION);
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

	return propsPtr;
}

//------------------------------------------------------------------------------
// GetOpenCLDeviceDescs
//------------------------------------------------------------------------------

PropertiesUPtr luxcore::GetOpenCLDeviceDescs() {
	API_BEGIN_NOARGS();

	PropertiesUPtr propsPtr = std::make_unique<Properties>();
	PropertiesRef props = *propsPtr;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	Context ctx;
	auto deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

	// Select only OpenCL devices
	DeviceDescription::Filter((DeviceType)(DEVICE_TYPE_OPENCL_ALL | DEVICE_TYPE_CUDA_ALL), deviceDescriptions);

	// Add all device information to the list
	for (int i=0; DeviceDescriptionRef desc : deviceDescriptions) {

		string platformName = "UNKNOWN";
		string platformVersion = "UNKNOWN";
		int deviceClock = 0;
		unsigned long long deviceLocalMem = 0;
		unsigned long long deviceConstMem = 0;
		if (desc.GetType() & DEVICE_TYPE_OPENCL_ALL) {
			OpenCLDeviceDescription oclDesc = dynamic_cast<OpenCLDeviceDescriptionRef>(desc);

			platformName = oclDesc.GetOpenCLPlatform();
			platformVersion = oclDesc.GetOpenCLVersion();
			deviceClock = oclDesc.GetClock();
			deviceLocalMem = oclDesc.GetLocalMem();
			deviceConstMem = oclDesc.GetConstMem();
		} else if (desc.GetType() & DEVICE_TYPE_CUDA_ALL) {
			platformName = "NVIDIA";
		}

		const string prefix = "opencl.device." + ToString(i++);
		props <<
				Property(prefix + ".platform.name")(platformName) <<
				Property(prefix + ".platform.version")(platformVersion) <<
				Property(prefix + ".name")(desc.GetName()) <<
				Property(prefix + ".type")(DeviceDescription::GetDeviceType(desc.GetType())) <<
				Property(prefix + ".units")(desc.GetComputeUnits()) <<
				Property(prefix + ".clock")(deviceClock) <<
				Property(prefix + ".nativevectorwidthfloat")(desc.GetNativeVectorWidthFloat()) <<
				Property(prefix + ".maxmemory")((unsigned long long)desc.GetMaxMemory()) <<
				Property(prefix + ".maxmemoryallocsize")((unsigned long long)desc.GetMaxMemoryAllocSize()) <<
				Property(prefix + ".localmemory")((unsigned long long)deviceLocalMem) <<
				Property(prefix + ".constmemory")((unsigned long long)deviceConstMem);

#if !defined(LUXRAYS_DISABLE_CUDA)
		if (desc.GetType() & DEVICE_TYPE_CUDA_ALL) {
			const auto& cudaDesc = dynamic_cast<CUDADeviceDescriptionConstRef>(desc);

			props <<
					Property(prefix + ".cuda.compute.major")(cudaDesc.GetCUDAComputeCapabilityMajor()) <<
					Property(prefix + ".cuda.compute.minor")(cudaDesc.GetCUDAComputeCapabilityMinor());
		}
#endif
	}
#endif

	API_RETURN("{}", ToArgString(props));

	return propsPtr;
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

	const std::vector<string> &result = slg::SLG_FileNameResolver.GetPaths();
	
	API_RETURN("{}", ToArgString(result));

	return result;
}

//------------------------------------------------------------------------------
// Kernel Cache Management
//------------------------------------------------------------------------------

void luxcore::ClearCUDAKernelCache() {
	API_BEGIN_NOARGS();
	
#if !defined(LUXRAYS_DISABLE_CUDA)
	luxrays::cudaKernelPersistentCache::ClearAllCaches();
#endif
	
	API_END();
}

void luxcore::ClearOCLKernelCache() {
	API_BEGIN_NOARGS();
	
#if !defined(LUXRAYS_DISABLE_OPENCL)
	luxrays::oclKernelPersistentCache::ClearAllCaches();
#endif
	
	API_END();
}

void luxcore::ClearAllKernelCaches() {
	API_BEGIN_NOARGS();
	
#if !defined(LUXRAYS_DISABLE_CUDA)
	luxrays::cudaKernelPersistentCache::ClearAllCaches();
#endif
#if !defined(LUXRAYS_DISABLE_OPENCL)
	luxrays::oclKernelPersistentCache::ClearAllCaches();
#endif
	
	API_END();
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

std::unique_ptr<Film> Film::Create(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	auto result = std::make_unique<luxcore::detail::FilmImplStandalone>(fileName);

	API_RETURN("{}", (void *)result.get());

	return result;
}

std::unique_ptr<Film> Film::Create(
		luxrays::PropertiesRPtr props,
		const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel) {
	API_BEGIN("{}, {}, {}", ToArgString(props), hasPixelNormalizedChannel, hasScreenNormalizedChannel);

	auto result = std::make_unique<luxcore::detail::FilmImplStandalone>(
		props, hasPixelNormalizedChannel, hasScreenNormalizedChannel
	);

	API_RETURN("{}", (void *)result.get());

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

std::unique_ptr<Scene> Scene::Create(
		luxrays::PropertiesRPtr resizePolicyProps
) {
	API_BEGIN("{}", (void *)resizePolicyProps.get());

	auto result = luxcore::detail::SceneImpl::Create(std::ref(resizePolicyProps));

	API_RETURN("{}", (void *)result.get());

	return result;
}

std::unique_ptr<Scene> Scene::Create(
	luxrays::PropertiesRPtr props,
	luxrays::PropertiesRPtr resizePolicyProps
) {
	API_BEGIN("{}, {}", ToArgString(props), (void *)resizePolicyProps.get());

	auto result = luxcore::detail::SceneImpl::Create(
		std::ref(props), std::ref(resizePolicyProps)
	);

	API_RETURN("{}", (void *)result.get());

	return result;
}

std::unique_ptr<Scene> Scene::Create(
	const string &fileName,
	luxrays::PropertiesRPtr resizePolicyProps
) {
	API_BEGIN("{}, {}", ToArgString(fileName), (void *)resizePolicyProps.get());

	auto result = luxcore::detail::SceneImpl::Create(
		fileName, std::ref(resizePolicyProps)
	);

	API_RETURN("{}", (void *)result.get());

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

// TODO Should be a std::unique<float[]>?
VertexBuffer Scene::AllocVerticesBuffer(const unsigned int meshVertCount) {
	API_BEGIN("{}", meshVertCount);

	VertexBuffer result = luxcore::detail::SceneImpl::AllocVerticesBuffer(meshVertCount);

	API_RETURN("{}", (void *)result.GetBytes().data());

	return result;
}

// TODO Should be a std::unique<size_t[]>?
TriangleBuffer Scene::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	API_BEGIN("{}", meshTriCount);

	TriangleBuffer result = luxcore::detail::SceneImpl::AllocTrianglesBuffer(meshTriCount);

	API_RETURN("{}", (void *)result.GetBytes().data());

	return result;
}

//------------------------------------------------------------------------------
// RenderConfig
//------------------------------------------------------------------------------

std::unique_ptr<RenderConfig> RenderConfig::Create(
	PropertiesUPtr&& props,
	const std::unique_ptr<luxcore::Scene>& scn  // We don't take ownership of the input scene
) {
	API_BEGIN("{}, {}", ToArgString(props), (void *)scn.get());

	SceneImpl& scnImpl = dynamic_cast<luxcore::detail::SceneImpl&>(*scn);

	auto result = luxcore::detail::RenderConfigImpl::Create<
		luxrays::PropertiesUPtr, SceneImpl&
	>(std::move(props), scnImpl);

	API_RETURN("{}", (void *)result.get());

	return result;
}

std::unique_ptr<RenderConfig> RenderConfig::Create(PropertiesUPtr&& props) {
	API_BEGIN("{}", ToArgString(props));

	auto result = luxcore::detail::RenderConfigImpl::Create(std::ref(props));

	API_RETURN("{}", (void *)result.get());

	return result;
}

std::unique_ptr<RenderConfig> RenderConfig::Create(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	auto result = luxcore::detail::RenderConfigImpl::Create(fileName);

	API_RETURN("{}", (void *)result.get());

	return result;
}

std::unique_ptr<RenderConfig> RenderConfig::Create(
	const std::string &fileName,
	std::shared_ptr<RenderState>& startState,  // In/out
	luxcore::FilmUPtr& startFilm  // In/out
) {
	API_BEGIN("{}, {}, {}", ToArgString(fileName), (void *)startState.get(), (void *)startFilm.get());

	std::shared_ptr<luxcore::detail::RenderStateImpl> ss;
	std::unique_ptr<luxcore::detail::FilmImpl> sf;
	auto rcfg = luxcore::detail::RenderConfigImpl::Create<
		const std::string &,
		std::shared_ptr<RenderStateImpl>& ,  // Out
		FilmImplUPtr& // Out
	>(fileName, ss, sf);

	startState = static_pointer_cast<luxcore::RenderState>(ss);
	startFilm = std::move(sf);

	API_RETURN("{}", (void *)rcfg.get());

	return rcfg;
}

PropertiesRPtr RenderConfig::GetDefaultProperties() {
	API_BEGIN_NOARGS();

	auto& result = luxcore::detail::RenderConfigImpl::GetDefaultProperties();

	API_RETURN("{}", ToArgString(result));

	return result;
}

//------------------------------------------------------------------------------
// RenderState
//------------------------------------------------------------------------------

std::shared_ptr<RenderState> RenderState::Create(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	std::shared_ptr<RenderState> result = std::make_shared<luxcore::detail::RenderStateImpl>(fileName);

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

RenderSessionRPtr RenderSession::Create(const RenderConfigRPtr & config) {
	API_BEGIN("{}", (void *) &config);

	auto& configImpl = static_cast<RenderConfigImpl &>(*config);

	auto result = RenderSessionImpl::Create(std::ref(configImpl));

	API_RETURN("{}", ToArgString(result));

	return std::move(result);
}
RenderSessionRPtr RenderSession::Create(
	const RenderConfigRPtr & config,
	std::shared_ptr<RenderState>& startState,
	FilmRef startFilm
) {
	API_BEGIN(
		"{}, {}, {}",
		(void *) &config,
		(void *)startState.get(),
		(void *)&startFilm
	);

	auto& configImpl = static_cast<RenderConfigImpl &>(*config);

	auto startStateImpl = startState ?
		static_pointer_cast<luxcore::detail::RenderStateImpl>(startState) :
		nullptr;

	using FIS = luxcore::detail::FilmImplStandalone;
	FIS& startFilmImpl = dynamic_cast<FIS&>(startFilm);

	//auto startFilmImpl = startFilm ?
		//FISPtr{static_cast<FIS&>(*startFilm)}:
		//nullptr;

	auto result = RenderSessionImpl::Create(
		std::ref(configImpl),
		startStateImpl,
		std::ref(startFilmImpl)
	);

	API_RETURN("{}", ToArgString(result));

	return result;
}

RenderSessionRPtr RenderSession::Create(
	const RenderConfigRPtr & config,
	const std::string &startStateFileName,
	const std::string &startFilmFileName
) {
	API_BEGIN("{}, {}, {}", (void*)&config, ToArgString(startStateFileName), ToArgString(startFilmFileName));

	auto& configImpl = dynamic_cast<luxcore::detail::RenderConfigImpl&>(*config);

	auto result = RenderSessionImpl::Create(
		std::ref(configImpl), startStateFileName, startFilmFileName
	);

	API_RETURN("{}", ToArgString(result));

	return result;
}

RenderSession::~RenderSession() {
	API_BEGIN_NOARGS();
	API_END();
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
