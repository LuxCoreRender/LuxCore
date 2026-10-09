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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iostream>
#include <string.h>
#include <fstream>
#include <mutex>
#include <atomic>
#include <vector>
#include <oneapi/tbb.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/oclerror.h"
#include "luxrays/utils/oclcache.h"
#include "luxrays/utils/embeddedspirv.h"
#include "luxrays/utils/config.h"

namespace luxrays {



// Static members for parallel compilation
std::atomic<bool> oclKernelPersistentCache::parallelCompilationEnabled(false);

// OpenCL vendor detection
std::string GetOpenCLVendor(cl_device_id device) {
	cl_platform_id platform;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr));
	
	std::size_t platformNameSize;
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 0, nullptr, &platformNameSize));
	char *platformNameChar = (char *)alloca(platformNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, platformNameSize, platformNameChar, nullptr));
	
	return std::string(platformNameChar);
}

// OpenCL compiler information
std::string GetOpenCLCompilerInfo(cl_device_id device) {
	cl_platform_id platform;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr));

	std::size_t platformVersionSize;
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 0, nullptr, &platformVersionSize));
	char *platformVersionChar = (char *)alloca(platformVersionSize * sizeof(char));
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, platformVersionSize, platformVersionChar, nullptr));
	const std::string platformVersion = boost::trim_copy(std::string(platformVersionChar));

	std::size_t driverVersionSize;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DRIVER_VERSION, 0, nullptr, &driverVersionSize));
	char *driverVersionChar = (char *)alloca(driverVersionSize * sizeof(char));
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DRIVER_VERSION, driverVersionSize, driverVersionChar, nullptr));
	const std::string driverVersion = boost::trim_copy(std::string(driverVersionChar));

	cl_bool compilerAvailable;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_COMPILER_AVAILABLE, sizeof(cl_bool), &compilerAvailable, nullptr));

	return GetOpenCLVendor(device) + " " + platformVersion +
		" (driver: " + driverVersion +
		", compiler " + (compilerAvailable ? "available" : "not available") + ")";
}

// Helper function to get error std::string
std::string oclErrorString(cl_int error) {
	switch (error) {
		case CL_SUCCESS:
			return "CL_SUCCESS";
		case CL_DEVICE_NOT_FOUND:
			return "CL_DEVICE_NOT_FOUND";
		case CL_DEVICE_NOT_AVAILABLE:
			return "CL_DEVICE_NOT_AVAILABLE";
		case CL_COMPILER_NOT_AVAILABLE:
			return "CL_COMPILER_NOT_AVAILABLE";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:
			return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case CL_OUT_OF_RESOURCES:
			return "CL_OUT_OF_RESOURCES";
		case CL_OUT_OF_HOST_MEMORY:
			return "CL_OUT_OF_HOST_MEMORY";
		case CL_PROFILING_INFO_NOT_AVAILABLE:
			return "CL_PROFILING_INFO_NOT_AVAILABLE";
		case CL_MEM_COPY_OVERLAP:
			return "CL_MEM_COPY_OVERLAP";
		case CL_IMAGE_FORMAT_MISMATCH:
			return "CL_IMAGE_FORMAT_MISMATCH";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:
			return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
		case CL_BUILD_PROGRAM_FAILURE:
			return "CL_BUILD_PROGRAM_FAILURE";
		case CL_MAP_FAILURE:
			return "CL_MAP_FAILURE";
#ifdef CL_VERSION_1_1
		case CL_MISALIGNED_SUB_BUFFER_OFFSET:
			return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
			return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
#endif
		case CL_INVALID_VALUE:
			return "CL_INVALID_VALUE";
		case CL_INVALID_DEVICE_TYPE:
			return "CL_INVALID_DEVICE_TYPE";
		case CL_INVALID_PLATFORM:
			return "CL_INVALID_PLATFORM";
		case CL_INVALID_DEVICE:
			return "CL_INVALID_DEVICE";
		case CL_INVALID_CONTEXT:
			return "CL_INVALID_CONTEXT";
		case CL_INVALID_QUEUE_PROPERTIES:
			return "CL_INVALID_QUEUE_PROPERTIES";
		case CL_INVALID_COMMAND_QUEUE:
			return "CL_INVALID_COMMAND_QUEUE";
		case CL_INVALID_HOST_PTR:
			return "CL_INVALID_HOST_PTR";
		case CL_INVALID_MEM_OBJECT:
			return "CL_INVALID_MEM_OBJECT";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
			return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case CL_INVALID_IMAGE_SIZE:
			return "CL_INVALID_IMAGE_SIZE";
		case CL_INVALID_SAMPLER:
			return "CL_INVALID_SAMPLER";
		case CL_INVALID_BINARY:
			return "CL_INVALID_BINARY";
		case CL_INVALID_BUILD_OPTIONS:
			return "CL_INVALID_BUILD_OPTIONS";
		case CL_INVALID_PROGRAM:
			return "CL_INVALID_PROGRAM";
		case CL_INVALID_PROGRAM_EXECUTABLE:
			return "CL_INVALID_PROGRAM_EXECUTABLE";
		case CL_INVALID_KERNEL_NAME:
			return "CL_INVALID_KERNEL_NAME";
		case CL_INVALID_KERNEL_DEFINITION:
			return "CL_INVALID_KERNEL_DEFINITION";
		case CL_INVALID_KERNEL:
			return "CL_INVALID_KERNEL";
		case CL_INVALID_ARG_INDEX:
			return "CL_INVALID_ARG_INDEX";
		case CL_INVALID_ARG_VALUE:
			return "CL_INVALID_ARG_VALUE";
		case CL_INVALID_ARG_SIZE:
			return "CL_INVALID_ARG_SIZE";
		case CL_INVALID_KERNEL_ARGS:
			return "CL_INVALID_KERNEL_ARGS";
		case CL_INVALID_WORK_DIMENSION:
			return "CL_INVALID_WORK_DIMENSION";
		case CL_INVALID_WORK_GROUP_SIZE:
			return "CL_INVALID_WORK_GROUP_SIZE";
		case CL_INVALID_WORK_ITEM_SIZE:
			return "CL_INVALID_WORK_ITEM_SIZE";
		case CL_INVALID_GLOBAL_OFFSET:
			return "CL_INVALID_GLOBAL_OFFSET";
		case CL_INVALID_EVENT_WAIT_LIST:
			return "CL_INVALID_EVENT_WAIT_LIST";
		case CL_INVALID_EVENT:
			return "CL_INVALID_EVENT";
		case CL_INVALID_OPERATION:
			return "CL_INVALID_OPERATION";
		case CL_INVALID_GL_OBJECT:
			return "CL_INVALID_GL_OBJECT";
		case CL_INVALID_BUFFER_SIZE:
			return "CL_INVALID_BUFFER_SIZE";
		case CL_INVALID_MIP_LEVEL:
			return "CL_INVALID_MIP_LEVEL";
		case CL_INVALID_GLOBAL_WORK_SIZE:
			return "CL_INVALID_GLOBAL_WORK_SIZE";
		default:
			return ToString(error);
	}
}

//------------------------------------------------------------------------------
// oclKernelCache
//------------------------------------------------------------------------------

std::string oclKernelCache::ToOptsString(const std::vector<std::string> &kernelsParameters) {
	std::string result;
	
	for	(auto const &p : kernelsParameters) {
		if (result.length() != 0)
			result += " ";
		result += p;
	}

	return result;
}

cl_program oclKernelCache::ForcedCompile(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
		std::string *errorStr) {
	if (errorStr)
		*errorStr = "";

	const char *kernelSources[1] = { kernelSource.c_str() };
	const std::size_t sourceSizes[1] = { kernelSource.length() };
	cl_int error;
	cl_program program = clCreateProgramWithSource(context, 1, kernelSources, sourceSizes, &error);
	CHECK_OCL_ERROR(error);

	const std::string optsStr = ToOptsString(kernelsParameters);
	error = clBuildProgram(program, 1, &device, optsStr.c_str(), nullptr, nullptr);
	if (error != CL_SUCCESS) {
		if (errorStr) {
			std::string logStr;
			if (program) {
				std::size_t valueSize;
				CHECK_OCL_ERROR(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &valueSize));
				char *value = (char *)alloca(valueSize * sizeof(char));
				CHECK_OCL_ERROR(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, valueSize, value, nullptr));

				logStr = std::string(value);
			} else
				logStr = "Build info not available";

			*errorStr = "ERROR " + ToString(error) + "[" + oclErrorString(error) + "]:" +
				"\n" + logStr + "\n";
		}

		if (program)
			CHECK_OCL_ERROR(clReleaseProgram(program));
		program = nullptr;
	}

	return program;
}

//------------------------------------------------------------------------------
// Embedded pre-compiled SPIR-V module translation
//------------------------------------------------------------------------------

typedef cl_program (CL_API_CALL *PFN_clCreateProgramWithIL)(cl_context,
	const void *, std::size_t, cl_int *);

// Translate the embedded pre-compiled SPIR-V module (if any) matching the
// kernel. It returns nullptr if there is no embedded module, if the device
// doesn't support the SPIR-V ingestion or if the translation fails; the caller
// falls back to the compilation of the source.
static cl_program CompileFromEmbeddedSPIRV(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource) {
	// Check if the device supports the ingestion of SPIR-V modules
	cl_int error;
	std::size_t extSize;
	error = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &extSize);
	if (error != CL_SUCCESS)
		return nullptr;
	std::vector<char> extStr(extSize);
	error = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, extSize, &extStr[0], nullptr);
	if (error != CL_SUCCESS || !strstr(&extStr[0], "cl_khr_il_program"))
		return nullptr;

	// Check if there is an embedded pre-compiled SPIR-V module for this kernel
	const std::string hash = oclKernelPersistentCache::HashString(
			oclKernelPersistentCache::ToOptsString(kernelsParameters)) +
		"-" + oclKernelPersistentCache::HashString(kernelSource);
	std::size_t spvSize;
	const unsigned char *spvData = GetEmbeddedSPIRVModule(hash, &spvSize);
	if (!spvData)
		return nullptr;

	// clCreateProgramWithIL is provided by the cl_khr_il_program extension
	// with OpenCL < 2.1
	cl_platform_id platform;
	error = clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr);
	if (error != CL_SUCCESS)
		return nullptr;
	auto clCreateProgramWithILFn = (PFN_clCreateProgramWithIL)
		clGetExtensionFunctionAddressForPlatform(platform, "clCreateProgramWithIL");
	if (!clCreateProgramWithILFn)
		return nullptr;

	cl_program program = clCreateProgramWithILFn(context, spvData, spvSize, &error);
	if (error != CL_SUCCESS)
		return nullptr;

	// The -D parameters are already baked in the SPIR-V module
	error = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
	if (error != CL_SUCCESS) {
		clReleaseProgram(program);
		return nullptr;
	}

	return program;
}

//------------------------------------------------------------------------------
// oclKernelPersistentCache
//------------------------------------------------------------------------------

std::filesystem::path oclKernelPersistentCache::GetCacheDir(const std::string &applicationName) {
	return luxrays::GetCacheDir() / "ocl_kernel_cache" / SanitizeFileName(applicationName);
}

oclKernelPersistentCache::oclKernelPersistentCache(const std::string &applicationName) {
	// Enable parallel compilation by default
	SetParallelCompilation(true);
	appName = applicationName;

	// Crate the cache directory
	std::filesystem::create_directories(GetCacheDir(appName));
}

oclKernelPersistentCache::~oclKernelPersistentCache() {
}

// Bob Jenkins's One-at-a-Time hash
// From: http://eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx

std::string oclKernelPersistentCache::HashString(const std::string &ss) {
	const u_int hash = HashBin(ss.c_str(), ss.length());

	char buf[9];
	sprintf(buf, "%08x", hash);

	return std::string(buf);
}

u_int oclKernelPersistentCache::HashBin(const char *s, const std::size_t size) {
	u_int hash = 0;

	for (u_int i = 0; i < size; ++i) {
		hash += *s++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

cl_program oclKernelPersistentCache::Compile(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
		bool *cached, std::string *errorStr, bool *fromSPIRV) {
	if (errorStr)
		*errorStr = "";
	if (fromSPIRV)
		*fromSPIRV = false;

	// Get device info (thread-safe, no shared state)
	cl_platform_id platform;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr));
	
	std::size_t platformNameSize;
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 0, nullptr, &platformNameSize));
	char *platformNameChar = (char *)alloca(platformNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, platformNameSize, platformNameChar, nullptr));
	std::string platformName = boost::trim_copy(std::string(platformNameChar));

	std::size_t deviceNameSize;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &deviceNameSize));
	char *deviceNameChar = (char *)alloca(deviceNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_NAME, deviceNameSize, deviceNameChar, nullptr));
	std::string deviceName = boost::trim_copy(std::string(deviceNameChar));
	
	cl_uint deviceUnitsUInt;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &deviceUnitsUInt, nullptr));
	std::string deviceUnits = ToString(deviceUnitsUInt);

	const std::string kernelName = HashString(ToOptsString(kernelsParameters)) + "-" + HashString(kernelSource) + ".ocl";
	const std::filesystem::path dirPath = GetCacheDir(appName) / SanitizeFileName(platformName) /
		SanitizeFileName(deviceName) / SanitizeFileName(deviceUnits);
	const std::filesystem::path filePath = dirPath / kernelName;
	const std::string fileName = filePath.generic_string();
	
	// Thread-safe cache access
	bool useCache = false;
	cl_program cachedProgram = nullptr;
	
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		
		if (std::filesystem::exists(filePath)) {
			useCache = true;
			const std::size_t fileSize = std::filesystem::file_size(filePath);

			if (fileSize > 4) {
				const std::size_t kernelSize = fileSize - 4;

				std::vector<char> kernelBin(kernelSize);

				// The use of std::filesystem::path is required for UNICODE support: fileName
				// is supposed to be UTF-8 encoded.
				std::ifstream file(std::filesystem::path(fileName),
					std::ifstream::in | std::ifstream::binary);

				// Read the binary hash
				u_int hashBin;
				file.read((char *)&hashBin, sizeof(int));

				file.read(&kernelBin[0], kernelSize);

				// Check for errors
				char buf[512];
				if (file.fail()) {
					sprintf(buf, "Unable to read kernel file cache %s", fileName.c_str());
					throw std::runtime_error(buf);
				}

				file.close();

				// Check the binary hash
				if (hashBin != HashBin(&kernelBin[0], kernelSize)) {
					// Something wrong in the file, remove it
					std::filesystem::remove(filePath);
				} else {
					// Cache is valid, compile from binaries
					std::vector<const unsigned char *> bins(1);
					bins[0] = (unsigned char *)&kernelBin[0];
					cl_int error;
					cachedProgram = clCreateProgramWithBinary(context, 1, &device, &kernelSize, 
							&bins[0], nullptr, &error);
					CHECK_OCL_ERROR(error);
					
					error = clBuildProgram(cachedProgram, 1, &device, nullptr, nullptr, nullptr);
					CHECK_OCL_ERROR(error);
					
					if (cached)
						*cached = true;
				}
			}
		}
	}
	
	if (useCache && cachedProgram) {
		return cachedProgram;
	}

	// Try to translate the embedded pre-compiled SPIR-V module (if any)
	// before falling back to the compilation of the source
	cl_program program = CompileFromEmbeddedSPIRV(context, device,
			kernelsParameters, kernelSource);

	// It isn't available or the translation failed, compile the source
	if (program) {
		if (fromSPIRV)
			*fromSPIRV = true;
	} else
		program = ForcedCompile(context, device,
				kernelsParameters, kernelSource, errorStr);
	if (!program)
		return nullptr;

	// Obtain the binaries of the sources
	std::size_t binsCount;
	CHECK_OCL_ERROR(clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, 0, nullptr, &binsCount));

	std::size_t *binsSizes = (std::size_t *)alloca(binsCount * sizeof(std::size_t));
	CHECK_OCL_ERROR(clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, binsCount, binsSizes, nullptr));

	// Create the file only if the binaries include something
	if (binsSizes[0] > 0) {
		// Using here alloca() can trigger a stack overflow on Windows for
		// large kernel binaries
		std::unique_ptr<char[]> bin(new char[binsSizes[0]]);
		char *bins = bin.get();
		CHECK_OCL_ERROR(clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char *), &bins, nullptr));

		// Add the kernel to the cache (thread-safe)
		std::lock_guard<std::mutex> lock(cacheMutex);
		
		// Recheck if file exists (another thread might have created it)
		if (!std::filesystem::exists(filePath)) {
			std::filesystem::create_directories(dirPath);

			// The use of std::filesystem::path is required for UNICODE support: fileName
			// is supposed to be UTF-8 encoded.
			std::ofstream file(std::filesystem::path(fileName),
					std::ofstream::out |
					std::ofstream::binary |
					std::ofstream::trunc);

			// Write the binary hash
			const u_int hashBin = HashBin(bins, binsSizes[0]);
			file.write((char *)&hashBin, sizeof(int));

			file.write(bins, binsSizes[0]);
			// Check for errors
			char buf[512];
			if (file.fail()) {
				sprintf(buf, "Unable to write kernel file cache %s", fileName.c_str());
				throw std::runtime_error(buf);
			}

			file.close();
		}
	}

	if (cached)
		*cached = false;

	return program;
}

//------------------------------------------------------------------------------
// Parallel compilation system using TBB
//------------------------------------------------------------------------------

void oclKernelPersistentCache::SetParallelCompilation(bool enable) {
	parallelCompilationEnabled = enable;
}

bool oclKernelPersistentCache::IsParallelCompilationEnabled() {
	return parallelCompilationEnabled;
}

std::vector<cl_program> oclKernelPersistentCache::CompileMultiple(
		cl_context context, cl_device_id device,
		const std::vector<std::tuple<std::vector<std::string>, std::string>> &kernels,
		std::vector<bool> *cached, std::vector<std::string> *errors,
		std::vector<bool> *fromSPIRV) {

	std::vector<cl_program> programs(kernels.size(), nullptr);

	if (!IsParallelCompilationEnabled() || kernels.empty()) {
		// Fallback to sequential compilation
		if (cached) cached->resize(kernels.size());
		if (errors) errors->resize(kernels.size());
		if (fromSPIRV) fromSPIRV->resize(kernels.size());

		for (std::size_t i = 0; i < kernels.size(); ++i) {
			const auto &[params, source] = kernels[i];
			bool isCached = false;
			bool isFromSPIRV = false;
			std::string error;

			programs[i] = Compile(context, device, params, source, &isCached, &error, &isFromSPIRV);

			if (cached) (*cached)[i] = isCached;
			if (errors) (*errors)[i] = error;
			if (fromSPIRV) (*fromSPIRV)[i] = isFromSPIRV;
		}

		return programs;
	}

	// Parallel compilation using TBB
	if (cached) cached->resize(kernels.size());
	if (errors) errors->resize(kernels.size());
	if (fromSPIRV) fromSPIRV->resize(kernels.size());

	tbb::parallel_for(tbb::blocked_range<std::size_t>(0, kernels.size()),
		[&](const tbb::blocked_range<std::size_t> &range) {
			for (std::size_t i = range.begin(); i < range.end(); ++i) {
				const auto &[params, source] = kernels[i];
				bool isCached = false;
				bool isFromSPIRV = false;
				std::string error;

				programs[i] = Compile(context, device, params, source, &isCached, &error, &isFromSPIRV);

				if (cached) (*cached)[i] = isCached;
				if (errors) (*errors)[i] = error;
				if (fromSPIRV) (*fromSPIRV)[i] = isFromSPIRV;
			}
		});

	return programs;
}

//------------------------------------------------------------------------------
// Cache management
//------------------------------------------------------------------------------

void oclKernelPersistentCache::ClearCache() {
	const std::filesystem::path cacheDir = GetCacheDir(appName);
	
	if (std::filesystem::exists(cacheDir)) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		std::filesystem::remove_all(cacheDir);
		std::filesystem::create_directories(cacheDir); // Recreate the directory
	}
}

void oclKernelPersistentCache::ClearKernelCache(cl_context context, cl_device_id device, const std::string &kernelName) {
	// Get device info to build the cache path
	cl_platform_id platform;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr));
	
	std::size_t platformNameSize;
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 0, nullptr, &platformNameSize));
	char *platformNameChar = (char *)alloca(platformNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, platformNameSize, platformNameChar, nullptr));
	std::string platformName = boost::trim_copy(std::string(platformNameChar));

	std::size_t deviceNameSize;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &deviceNameSize));
	char *deviceNameChar = (char *)alloca(deviceNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_NAME, deviceNameSize, deviceNameChar, nullptr));
	std::string deviceName = boost::trim_copy(std::string(deviceNameChar));
	
	cl_uint deviceUnitsUInt;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &deviceUnitsUInt, nullptr));
	std::string deviceUnits = ToString(deviceUnitsUInt);

	const std::filesystem::path dirPath = GetCacheDir(appName) / SanitizeFileName(platformName) /
		SanitizeFileName(deviceName) / SanitizeFileName(deviceUnits);
	const std::filesystem::path kernelPath = dirPath / kernelName;
	
	if (std::filesystem::exists(kernelPath)) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		std::filesystem::remove(kernelPath);
	}
}

void oclKernelPersistentCache::ClearAllCaches() {
	const std::filesystem::path baseCacheDir = luxrays::GetCacheDir() / "ocl_kernel_cache";
	
	if (std::filesystem::exists(baseCacheDir)) {
		// Use a separate std::mutex for global operations
		static std::mutex globalCacheMutex;
		std::lock_guard<std::mutex> lock(globalCacheMutex);
		std::filesystem::remove_all(baseCacheDir);
		std::filesystem::create_directories(baseCacheDir); // Recreate the directory
	}
}

void CheckOpenCLError(const cl_int err, const char *file, const int line) {
  	if (err != CL_SUCCESS) {
		std::string msg = std::string("OpenCL driver API error ")
			+ std::string("(code: ") + ToString(err)
			+ std::string(", file:") + std::string(file)
			+ std::string(", line: ") + ToString(line) + std::string(")")
			+ std::string(": ") + oclErrorString(err) + "\n";
		throw std::runtime_error(msg);
	}
}

}  // namespace luxrays

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
