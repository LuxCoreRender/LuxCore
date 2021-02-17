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

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <boost/regex.hpp>

#include "luxrays/devices/ocldevice.h"
#include "luxrays/devices/cudadevice.h"
#include "luxrays/kernels/kernels.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

CUDADeviceDescription::CUDADeviceDescription(CUdevice dev, const size_t devIndex) :
		DeviceDescription("CUDAInitializingDevice", DEVICE_TYPE_CUDA_GPU),
		cudaDeviceIndex(devIndex), cudaDevice(dev) {
	char buff[128];
    CHECK_CUDA_ERROR(cuDeviceGetName(buff, 128, cudaDevice));
	name = string(buff);
	
	const int major = GetCUDAComputeCapabilityMajor();
	const int minor = GetCUDAComputeCapabilityMinor();
	useOptix = (isOptixAvilable && ((major > 7) || ((major == 7) && (minor == 5)))) ? true : false;
}

CUDADeviceDescription::~CUDADeviceDescription() {
}

int CUDADeviceDescription::GetCUDAComputeCapabilityMajor() const {
	int major;
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cudaDevice));

	return major;
}

// List of GPUs CUDA compute capability can be found here: https://developer.nvidia.com/cuda-gpus
int CUDADeviceDescription::GetCUDAComputeCapabilityMinor() const {
	int major;
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cudaDevice));

	return major;
}

int CUDADeviceDescription::GetComputeUnits() const {
	const int major = GetCUDAComputeCapabilityMajor();
	const int minor = GetCUDAComputeCapabilityMinor();
	
	typedef struct {
		int SM;
		int Cores;
	} SM2Cores;

	static SM2Cores archCoresPerSM[] = {
		// 0xMm (hexidecimal notation) where M = SM Major version and m = SM minor version
		{0x30, 192},
		{0x32, 192},
		{0x35, 192},
		{0x37, 192},
		{0x50, 128},
		{0x52, 128},
		{0x53, 128},
		{0x60, 64},
		{0x61, 128},
		{0x62, 128},
		{0x70, 64},
		{0x72, 64},
		{0x75, 64},
		{-1, -1}
	};

	int index = 0;
	while (archCoresPerSM[index].SM != -1) {
		if (archCoresPerSM[index].SM == ((major << 4) + minor))
			return archCoresPerSM[index].Cores;

		index++;
	}

	return DeviceDescription::GetComputeUnits();
}

u_int CUDADeviceDescription::GetNativeVectorWidthFloat() const {
	return 1;
}

size_t CUDADeviceDescription::GetMaxMemory() const {
	size_t bytes;
	CHECK_CUDA_ERROR(cuDeviceTotalMem(&bytes, cudaDevice));
	
	return bytes;
}

size_t CUDADeviceDescription::GetMaxMemoryAllocSize() const {
	return numeric_limits<size_t>::max();
}

bool CUDADeviceDescription::HasOutOfCoreMemorySupport() const {
	int v;
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&v, CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING, cudaDevice));

	return (v == 1);
}

void CUDADeviceDescription::AddDeviceDescs(vector<DeviceDescription *> &descriptions) {
	int devCount;
	CHECK_CUDA_ERROR(cuDeviceGetCount(&devCount));

	for (int i = 0; i < devCount; i++) {
		CUdevice device;
		CHECK_CUDA_ERROR(cuDeviceGet(&device, i));

		CUDADeviceDescription *desc = new CUDADeviceDescription(device, i);

		descriptions.push_back(desc);
	}
}

//------------------------------------------------------------------------------
// CUDADevice
//------------------------------------------------------------------------------

static void OptixLogCB(u_int level, const char* tag, const char *message, void *cbdata) {
	const Context *context = (Context *)cbdata;
	
	LR_LOG(context, "[Optix][" << level << "][" << tag << "] " << message);
}

CUDADevice::CUDADevice(
		const Context *context,
		CUDADeviceDescription *desc,
		const size_t devIndex) :
		Device(context, devIndex),
		deviceDesc(desc),
		cudaContext(nullptr), optixContext(nullptr) {
	deviceName = (desc->GetName() + " CUDAIntersect").c_str();

	kernelCache = new cudaKernelPersistentCache("LUXRAYS_" LUXRAYS_VERSION_MAJOR "." LUXRAYS_VERSION_MINOR);

	CHECK_CUDA_ERROR(cuCtxCreate(&cudaContext, CU_CTX_SCHED_YIELD, deviceDesc->GetCUDADevice()));

	// I prefer cache over shared memory because I pretty much never use shared memory
	CHECK_CUDA_ERROR(cuCtxSetCacheConfig(CU_FUNC_CACHE_PREFER_L1));

	if (isOptixAvilable && desc->useOptix) {
		OptixDeviceContextOptions optixOptions = {};
		optixOptions.logCallbackFunction = &OptixLogCB;
		optixOptions.logCallbackData = (void *)deviceContext;
		// For normal usage
		//optixOptions.logCallbackLevel = 1;
		// For debugging
		optixOptions.logCallbackLevel = 4;
		
		const OptixResult result = optixDeviceContextCreate(cudaContext, &optixOptions, &optixContext);
		if (result != OPTIX_SUCCESS) {
			LR_LOG(context, "WARNING unable to create Optix context for device " << deviceName << ": " <<
					std::string(optixGetErrorName(result)) << "(code: " <<  result << ")");
			optixContext = nullptr;
		}
	}
}

CUDADevice::~CUDADevice() {
	if (optixContext) {
		// cudaContext could be not the current context and Optix crashes in this case
		CHECK_CUDA_ERROR(cuCtxPushCurrent(cudaContext));

		CHECK_OPTIX_ERROR(optixDeviceContextDestroy(optixContext));

		CHECK_CUDA_ERROR(cuCtxPopCurrent(nullptr));
	}

	// Free all loaded modules
	for (auto &m : loadedModules) {
		CHECK_CUDA_ERROR(cuModuleUnload(m));
	}
	loadedModules.clear();

	if (cudaContext) {
		CHECK_CUDA_ERROR(cuCtxDestroy(cudaContext));
	}

	delete kernelCache;
}

void CUDADevice::PushThreadCurrentDevice() {
	CHECK_CUDA_ERROR(cuCtxPushCurrent(cudaContext));
}

void CUDADevice::PopThreadCurrentDevice() {
	CHECK_CUDA_ERROR(cuCtxPopCurrent(nullptr));
}

//------------------------------------------------------------------------------
// Kernels handling for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

vector<string> CUDADevice::AddKernelOpts(const vector<string> &programParameters) {
	vector<string> cudaProgramParameters = programParameters;

	cudaProgramParameters.push_back("-D LUXRAYS_CUDA_DEVICE");
#if defined (__APPLE__)
	cudaProgramParameters.push_back("-D LUXRAYS_OS_APPLE");
#elif defined (WIN32)
	cudaProgramParameters.push_back("-D LUXRAYS_OS_WINDOWS");
#elif defined (__linux__)
	cudaProgramParameters.push_back("-D LUXRAYS_OS_LINUX");
#endif

	cudaProgramParameters.insert(cudaProgramParameters.end(),
			additionalCompileOpts.begin(), additionalCompileOpts.end());

	return cudaProgramParameters;
}

string CUDADevice::GetKernelSource(const string &kernelSource) {
	return
		luxrays::ocl::KernelSource_cudadevice_oclemul_types +
		luxrays::ocl::KernelSource_cudadevice_math +
		luxrays::ocl::KernelSource_cudadevice_oclemul_funcs +
		kernelSource;
}

void CUDADevice::CompileProgram(HardwareDeviceProgram **program,
		const vector<string> &programParameters, const string &programSource,	
		const string &programName) {
	vector<string> cudaProgramParameters = AddKernelOpts(programParameters);

	LR_LOG(deviceContext, "[" << programName << "] Compiler options: " << oclKernelPersistentCache::ToOptsString(cudaProgramParameters));
	LR_LOG(deviceContext, "[" << programName << "] Compiling kernels");

	const string cudaProgramSource = GetKernelSource(programSource);

	bool cached;
	string error;
	CUmodule module = kernelCache->Compile(cudaProgramParameters, cudaProgramSource, programName, &cached, &error);
	if (!module) {
		LR_LOG(deviceContext, "[" << programName << "] CUDA program compilation error: " << endl << error);
		
		throw runtime_error(programName + " CUDA program compilation error");
	} else {
		if (error.length() > 0) {
			LR_LOG(deviceContext, "[" << programName << "] CUDA program compilation warnings: " << endl << error);
		}
	}

	if (cached) {
		LR_LOG(deviceContext, "[" << programName << "] Program cached");
	} else {
		LR_LOG(deviceContext, "[" << programName << "] Program not cached");
	}
	
	if (!*program)
		*program = new CUDADeviceProgram();
	
	CUDADeviceProgram *cudaDeviceProgram = dynamic_cast<CUDADeviceProgram *>(*program);
	assert (cudaDeviceProgram);

	cudaDeviceProgram->Set(module);
	
	loadedModules.push_back(module);
}

void CUDADevice::GetKernel(HardwareDeviceProgram *program,
		HardwareDeviceKernel **kernel, const string &kernelName) {
	if (!*kernel)
		*kernel = new CUDADeviceKernel();

	CUDADeviceKernel *cudaDeviceKernel = dynamic_cast<CUDADeviceKernel *>(*kernel);
	assert (cudaDeviceKernel);

	CUDADeviceProgram *cudaDeviceProgram = dynamic_cast<CUDADeviceProgram *>(program);
	assert (cudaDeviceProgram);

	CUfunction function;
	CHECK_CUDA_ERROR(cuModuleGetFunction(&function, cudaDeviceProgram->GetModule(), kernelName.c_str()));
	
	cudaDeviceKernel->Set(function);
}

u_int CUDADevice::GetKernelWorkGroupSize(HardwareDeviceKernel *kernel) {
	assert (kernel);
	assert (!kernel->IsNull());

	return 32;
}

void CUDADevice::SetKernelArg(HardwareDeviceKernel *kernel,
		const u_int index, const size_t size, const void *arg) {
	assert (kernel);
	assert (!kernel->IsNull());

	CUDADeviceKernel *cudaDeviceKernel = dynamic_cast<CUDADeviceKernel *>(kernel);
	assert (cudaDeviceKernel);

	if (index >= cudaDeviceKernel->args.size())
		cudaDeviceKernel->args.resize(index + 1, nullptr);

	void *argCpy;
	if (arg) {
		// Copy the argument
		argCpy = new char[size];
		memcpy(argCpy, arg, size);
	} else {
		CUdeviceptr p = 0;
		argCpy = new char[sizeof(CUdeviceptr)];
		memcpy(argCpy, &p, sizeof(CUdeviceptr));
	}

	if (cudaDeviceKernel->args[index]) {
		delete[] (char *)cudaDeviceKernel->args[index];
		cudaDeviceKernel->args[index] = nullptr;
	}

	cudaDeviceKernel->args[index] = argCpy;
}

void CUDADevice::SetKernelArgBuffer(HardwareDeviceKernel *kernel,
		const u_int index, const HardwareDeviceBuffer *buff) {
	assert (kernel);
	assert (!kernel->IsNull());

	if (buff) {
		const CUDADeviceBuffer *cudaDeviceBuff = dynamic_cast<const CUDADeviceBuffer *>(buff);
		assert (cudaDeviceBuff);

		SetKernelArg(kernel, index, sizeof(CUdeviceptr), &cudaDeviceBuff->cudaBuff);
	} else
		SetKernelArg(kernel, index, sizeof(CUdeviceptr), nullptr);
}

static void ConvertHardwareRange(const u_int globalSize, const u_int workGroupSize,
		u_int &block, u_int &thread) {
	if (workGroupSize == 0) {
		block = globalSize / 32;
		thread = 32;
	} else {
		block = globalSize / workGroupSize;
		thread = workGroupSize;
	}
}

static void ConvertHardwareRange(const HardwareDeviceRange &globalSize,
		const HardwareDeviceRange &workGroupSize,
		u_int &blockX, u_int &blockY, u_int &blockZ,
		u_int &threadX, u_int &threadY, u_int &threadZ) {
	if (globalSize.dimensions == 1) {
		ConvertHardwareRange(globalSize.sizes[0], workGroupSize.sizes[0], blockX, threadX);
		blockY = 1;
		threadY = 1;
		blockZ = 1;
		threadZ = 1;
	} else if (globalSize.dimensions == 2) {
		ConvertHardwareRange(globalSize.sizes[0], workGroupSize.sizes[0], blockX, threadX);
		ConvertHardwareRange(globalSize.sizes[1], workGroupSize.sizes[1], blockY, threadY);
		blockZ = 1;
		threadZ = 1;
	} else {
		ConvertHardwareRange(globalSize.sizes[0], workGroupSize.sizes[0], blockX, threadX);
		ConvertHardwareRange(globalSize.sizes[1], workGroupSize.sizes[1], blockY, threadY);
		ConvertHardwareRange(globalSize.sizes[2], workGroupSize.sizes[2], blockZ, threadZ);
	}
}

void CUDADevice::EnqueueKernel(HardwareDeviceKernel *kernel,
			const HardwareDeviceRange &globalSize,
			const HardwareDeviceRange &workGroupSize) {
	assert (kernel);
	assert (!kernel->IsNull());

	CUDADeviceKernel *cudaDeviceKernel = dynamic_cast<CUDADeviceKernel *>(kernel);
	assert (cudaDeviceKernel);

	u_int blockX, blockY, blockZ;
	u_int threadX, threadY, threadZ;
	ConvertHardwareRange(globalSize, workGroupSize,
			blockX, blockY, blockZ,
			threadX, threadY, threadZ);

	CHECK_CUDA_ERROR(cuLaunchKernel(cudaDeviceKernel->cudaKernel,
			blockX, blockY, blockZ,  // blocks
			threadX, threadY, threadZ,  // threads
			0, 0,
			&cudaDeviceKernel->args[0],
			nullptr));
}

void CUDADevice::EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
		const bool blocking, const size_t size, void *ptr) {
	assert (buff);
	assert (!buff->IsNull());

	const CUDADeviceBuffer *cudaDeviceBuff = dynamic_cast<const CUDADeviceBuffer *>(buff);
	assert (cudaDeviceBuff);

	if (blocking) {
		CHECK_CUDA_ERROR(cuMemcpyDtoH(ptr, cudaDeviceBuff->cudaBuff, size));
	} else {
		CHECK_CUDA_ERROR(cuMemcpyDtoHAsync(ptr, cudaDeviceBuff->cudaBuff, size, 0));
	}
}

void CUDADevice::EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
		const bool blocking, const size_t size, const void *ptr) {
	assert (buff);
	assert (!buff->IsNull());

	const CUDADeviceBuffer *cudaDeviceBuff = dynamic_cast<const CUDADeviceBuffer *>(buff);
	assert (cudaDeviceBuff);

	if (blocking) {
		CHECK_CUDA_ERROR(cuMemcpyHtoD(cudaDeviceBuff->cudaBuff, ptr, size));
	} else {
		CHECK_CUDA_ERROR(cuMemcpyHtoDAsync(cudaDeviceBuff->cudaBuff, ptr, size, 0));
	}
}

void CUDADevice::FlushQueue() {
}

void CUDADevice::FinishQueue() {
	CHECK_CUDA_ERROR(cuStreamSynchronize(0));
}

//------------------------------------------------------------------------------
// Memory management for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

void CUDADevice::AllocBuffer(HardwareDeviceBuffer **hdBuff, const BufferType type,
		void *src, const size_t size, const string &desc) {
	if (!*hdBuff)
		*hdBuff = new CUDADeviceBuffer();

	CUDADeviceBuffer *cudaDeviceBuff = dynamic_cast<CUDADeviceBuffer *>(*hdBuff);
	assert (cudaDeviceBuff);

	CUdeviceptr *buff = &cudaDeviceBuff->cudaBuff;
	
	// Handle the case of an empty buffer
	if (!size) {
		if (*buff) {
			// Free the buffer
			size_t cudaSize;
			CHECK_CUDA_ERROR(cuMemGetAddressRange(0, &cudaSize, *buff));
			FreeMemory(cudaSize);

			CHECK_CUDA_ERROR(cuMemFree(*buff));
		}

		*buff = 0;

		return;
	}

	if (*buff) {
		// Check the size of the already allocated buffer

		size_t cudaSize;
		CHECK_CUDA_ERROR(cuMemGetAddressRange(0, &cudaSize, *buff));

		if (size == cudaSize) {
			// I can reuse the buffer; just update the content

			//LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes");
			if (src) {
				CHECK_CUDA_ERROR(cuMemcpyHtoDAsync(*buff, src, size, 0));
			}

			return;
		} else {
			// Free the buffer
			size_t cudaSize;
			CHECK_CUDA_ERROR(cuMemGetAddressRange(0, &cudaSize, *buff));
			FreeMemory(cudaSize);

			CHECK_CUDA_ERROR(cuMemFree(*buff));
			*buff = 0;
		}
	}

	if (desc != "")
		LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc <<
				" buffer size: " << ToMemString(size) << ((type & BUFFER_TYPE_OUT_OF_CORE) ? " (OUT OF CORE)" : ""));

	// Check if I was asked for out of core support
	if ((type & BUFFER_TYPE_OUT_OF_CORE) && !deviceDesc->HasOutOfCoreMemorySupport()) {
		LR_LOG(deviceContext, "WARNING: CUDA device " << deviceDesc->GetName() << " doesn't support out of core memory buffers: " << desc);
	}
	
	if (type & BUFFER_TYPE_OUT_OF_CORE) {
		CHECK_CUDA_ERROR(cuMemAllocManaged(buff, size, CU_MEM_ATTACH_GLOBAL));

		if (type & BUFFER_TYPE_READ_ONLY) {
			CHECK_CUDA_ERROR(cuMemAdvise(*buff, size, CU_MEM_ADVISE_SET_READ_MOSTLY, deviceDesc->cudaDevice));
		}
	} else {
		CHECK_CUDA_ERROR(cuMemAlloc(buff, size));
	}

	if (src) {
		CHECK_CUDA_ERROR(cuMemcpyHtoDAsync(*buff, src, size, 0));
	}
	
	AllocMemory(size);
}

void CUDADevice::FreeBuffer(HardwareDeviceBuffer **buff) {
	if (*buff) {
		if (!(*buff)->IsNull()) {
			CUDADeviceBuffer *cudaDeviceBuff = dynamic_cast<CUDADeviceBuffer *>(*buff);
			assert (cudaDeviceBuff);

			FreeMemory(cudaDeviceBuff->GetSize());

			CHECK_CUDA_ERROR(cuMemFree(cudaDeviceBuff->cudaBuff));
		}

		delete *buff;
		*buff = nullptr;
	}
}

//------------------------------------------------------------------------------
// Optional Image management for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

void CUDADevice::AllocImageMap(const luxrays::ocl::ImageMapDescription &imgMapDesc,
		void *data, luxrays::ocl::ImageMapObj *imgMapObjs) {
	u_int elementSizeBytes;
	CUarray_format format;
	switch (imgMapDesc.storageType) {
		case luxrays::ocl::BYTE:
			format = CU_AD_FORMAT_UNSIGNED_INT8;
			elementSizeBytes = 1;
			break;
		case luxrays::ocl::HALF:
			format = CU_AD_FORMAT_HALF;
			elementSizeBytes = 2;
			break;
		case luxrays::ocl::FLOAT:
			format = CU_AD_FORMAT_FLOAT;
			elementSizeBytes = 4;
			break;
		default:
			throw runtime_error("Unknown storage type in CUDADevice::AllocImageMap()");
	}

	CUDA_RESOURCE_DESC resDesc = {};
	switch (imgMapDesc.channelCount) {
		case 1: {
			// In this case, I don't need to create a copy of the memory with the appropriate layout
			
			CUDA_ARRAY_DESCRIPTOR arrayDesc;
			arrayDesc.Format = format;
			arrayDesc.NumChannels = imgMapDesc.channelCount;
			arrayDesc.Width = imgMapDesc.width;
			arrayDesc.Height = imgMapDesc.height;

			CUarray cuArray;
			CHECK_CUDA_ERROR(cuArrayCreate(&cuArray, &arrayDesc));

			CUDA_MEMCPY2D copyParam = {};
			copyParam.dstMemoryType = CU_MEMORYTYPE_ARRAY;
			copyParam.dstArray = cuArray;
			copyParam.srcMemoryType = CU_MEMORYTYPE_HOST;
			copyParam.srcHost = data;
			copyParam.srcPitch = imgMapDesc.width;
			copyParam.WidthInBytes = copyParam.srcPitch;//WORNG!!!
			copyParam.Height = imgMapDesc.height;
			CHECK_CUDA_ERROR(cuMemcpy2D(&copyParam));

			resDesc.resType = CU_RESOURCE_TYPE_ARRAY;
			resDesc.res.array.hArray = cuArray;			
			break;
		}
		case 2:
		case 3:
		case 4: {
			// In this case I need to create a copy of the memory with the appropriate layout

			resDesc.resType = CU_RESOURCE_TYPE_PITCH2D;

			size_t devicePitch;
			CHECK_CUDA_ERROR(cuMemAllocPitch(&resDesc.res.pitch2D.devPtr, &devicePitch,
					imgMapDesc.width * elementSizeBytes * imgMapDesc.channelCount,
					imgMapDesc.height, 16));

			resDesc.res.pitch2D.format = format;
			resDesc.res.pitch2D.numChannels = imgMapDesc.channelCount;
			resDesc.res.pitch2D.width = imgMapDesc.width;
			resDesc.res.pitch2D.height = imgMapDesc.height;
			resDesc.res.pitch2D.pitchInBytes = devicePitch;

			CUDA_MEMCPY2D copyParam = {};
			copyParam.WidthInBytes = imgMapDesc.width * elementSizeBytes * imgMapDesc.channelCount;
			copyParam.Height = imgMapDesc.height;

			copyParam.srcMemoryType = CU_MEMORYTYPE_HOST;
			copyParam.srcHost = data;
			copyParam.srcPitch = copyParam.WidthInBytes;

			copyParam.dstMemoryType = CU_MEMORYTYPE_DEVICE;
			copyParam.dstDevice = resDesc.res.pitch2D.devPtr;
			copyParam.dstPitch = devicePitch;
			CHECK_CUDA_ERROR(cuMemcpy2D(&copyParam));

//			CUDA_MEMCPY2D copyParam = {};
//			copyParam.WidthInBytes = 512 *4;
//			copyParam.Height = 512;
//
//			copyParam.srcMemoryType = CU_MEMORYTYPE_HOST;
//			copyParam.srcHost = data;
//			copyParam.srcPitch = 512 *4;
//			copyParam.srcPitch = devicePitch;
//
//			copyParam.dstMemoryType = CU_MEMORYTYPE_DEVICE;
//			copyParam.dstDevice = resDesc.res.pitch2D.devPtr;
//			copyParam.dstPitch = 512 *4;
//			//CHECK_CUDA_ERROR(cuMemcpy2DUnaligned(&copyParam));
//			CHECK_CUDA_ERROR(cuMemcpy2D(&copyParam));
			break;
		}
		default:
			throw runtime_error("Unknown channel count in CUDADevice::AllocImageMap()");
	}

	CUDA_TEXTURE_DESC texDesc = {};
	switch (imgMapDesc.wrapType) {
		case luxrays::ocl::WRAP_REPEAT:
			texDesc.addressMode[0] = CU_TR_ADDRESS_MODE_WRAP;
			texDesc.addressMode[1] = CU_TR_ADDRESS_MODE_WRAP;
			texDesc.addressMode[2] = CU_TR_ADDRESS_MODE_WRAP;
			break;
		case luxrays::ocl::WRAP_BLACK:
			texDesc.addressMode[0] = CU_TR_ADDRESS_MODE_BORDER;
			texDesc.addressMode[1] = CU_TR_ADDRESS_MODE_BORDER;
			texDesc.addressMode[2] = CU_TR_ADDRESS_MODE_BORDER;
			texDesc.borderColor[0] = 0.f;
			texDesc.borderColor[1] = 0.f;
			texDesc.borderColor[2] = 0.f;
			texDesc.borderColor[3] = 1.f;
			break;
		case luxrays::ocl::WRAP_WHITE:
			texDesc.addressMode[0] = CU_TR_ADDRESS_MODE_BORDER;
			texDesc.addressMode[1] = CU_TR_ADDRESS_MODE_BORDER;
			texDesc.addressMode[2] = CU_TR_ADDRESS_MODE_BORDER;
			texDesc.borderColor[0] = 1.f;
			texDesc.borderColor[1] = 1.f;
			texDesc.borderColor[2] = 1.f;
			texDesc.borderColor[3] = 1.f;
			break;
		case luxrays::ocl::WRAP_CLAMP:
			texDesc.addressMode[0] = CU_TR_ADDRESS_MODE_CLAMP;
			texDesc.addressMode[1] = CU_TR_ADDRESS_MODE_CLAMP;
			texDesc.addressMode[2] = CU_TR_ADDRESS_MODE_CLAMP;
			break;
		default:
			throw runtime_error("Unknown wrap type in CUDADevice::AllocImageMap()");
	}

	texDesc.filterMode = CU_TR_FILTER_MODE_LINEAR;
	texDesc.flags = CU_TRSF_NORMALIZED_COORDINATES;

	CUtexObject texObject;
	CHECK_CUDA_ERROR(cuTexObjectCreate(&texObject, &resDesc, &texDesc, nullptr));

	*imgMapObjs = texObject;
}

void CUDADevice::FreeImageMap(luxrays::ocl::ImageMapObj *imgMapObjs) {
	if (*imgMapObjs != IMAGEMAPOBJ_NULL) {
		CHECK_CUDA_ERROR(cuTexObjectDestroy(*imgMapObjs));

		*imgMapObjs = IMAGEMAPOBJ_NULL;
		
		// TODO: free cuArray or Pitch2D memory

	}
}

#endif
