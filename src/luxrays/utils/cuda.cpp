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

#include <memory>
#if !defined(LUXRAYS_DISABLE_CUDA)

#include <iostream>
#include <fstream>
#include <string.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <oneapi/tbb.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "luxrays/core/context.h"
#include "luxrays/luxrays.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/config.h"
#include "luxrays/utils/cudacache.h"
#include "luxrays/utils/oclcache.h"

#include <optix_function_table_definition.h>

namespace luxrays {

// Static members for parallel compilation
std::atomic<bool> cudaKernelPersistentCache::parallelCompilationEnabled(false);

static std::string GetCuda10Architecture() {
	CUdevice device;
	int major, minor;
	CHECK_CUDA_ERROR(cuCtxGetDevice(&device));
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device));
	CHECK_CUDA_ERROR(cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device));

        // v2.10: as nvrtc has been updated, the below code should not still be necessary...
        // we keep it "just-in-case"
        //
	//if ((major >= 7) && (minor >= 5)) {
		//// NVIDIA driver doesn't include NVIDA RTC (Run Time Compiler) so we ship
		//// CUDA 10 NRTC with LuxCore however it supports only up to Turing architecture
		//// (Ampere 8.0 architecture is not supported). So I have to bound the required
		//// architecture in order to not get an error.

		//major = 7;
		//minor = 5;
	//}

	return std::to_string(major) + std::to_string(minor);
}

//------------------------------------------------------------------------------
// cudaKernelCache
//------------------------------------------------------------------------------

bool cudaKernelCache::ForcedCompilePTX(
	const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
	const std::string &programName, std::unique_ptr<char[]> * ptx, std::size_t *ptxSize, std::string *error
) {
	if (error)
		*error = "";

	nvrtcProgram prog;
	CHECK_NVRTC_ERROR(nvrtcCreateProgram(&prog, kernelSource.c_str(), programName.c_str(), 0, nullptr, nullptr));

	std::vector<const char *> cudaOpts;
	cudaOpts.push_back("--device-as-default-execution-space");
	//cudaOpts.push_back("--disable-warnings");

        // Set target architecture, based on current device's capability
        std::string targetArch = "--gpu-architecture=compute_" + GetCuda10Architecture();
        cudaOpts.push_back(targetArch.c_str());

	// To display warning numbers
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--display_error_number");

	// To suppress warning: warning #550-D: variable "xyz" was set but never used
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--diag_suppress=550");

	// To suppress warning: warning #1055-D: types cannot be declared in anonymous unions
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--diag_suppress=1055");

	// To suppress warning: warning #68-D: integer conversion resulted in a change of sign
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--diag_suppress=68");

	// Accelerate compilation
	//cudaOpts.push_back("--Ofast-compile=min"); # Only 12.9+
	cudaOpts.push_back("--split-compile=0");

	// Enable debug info
	//cudaOpts.push_back("-G");
	// Enable only debug line info
	//cudaOpts.push_back("--generate-line-info");

	for	(auto const &p : kernelsParameters)
		cudaOpts.push_back(p.c_str());

	// For some debug
	//for (uint i = 0; i < cudaOpts.size(); ++i)
	//	cout << "Opt #" << i <<" : [" << cudaOpts[i] << "]\n";

	const nvrtcResult compilationResult = nvrtcCompileProgram(prog,
			cudaOpts.size(),
			(cudaOpts.size() > 0) ? &cudaOpts[0] : nullptr);

	std::size_t logSize;
	CHECK_NVRTC_ERROR(nvrtcGetProgramLogSize(prog, &logSize));
	auto log = std::make_unique<char[]>(logSize);
	CHECK_NVRTC_ERROR(nvrtcGetProgramLog(prog, log.get()));

	*error = std::string(log.get());

	if (compilationResult != NVRTC_SUCCESS)
		return false;

	// Obtain PTX from the program.
	CHECK_NVRTC_ERROR(nvrtcGetPTXSize(prog, ptxSize));
	*ptx = std::make_unique<char[]>(*ptxSize);
	CHECK_NVRTC_ERROR(nvrtcGetPTX(prog, ptx->get()));

	CHECK_NVRTC_ERROR(nvrtcDestroyProgram(&prog));

	return true;
}

//------------------------------------------------------------------------------
// Parallel compilation system using TBB
//------------------------------------------------------------------------------

void cudaKernelPersistentCache::SetParallelCompilation(bool enable) {
	parallelCompilationEnabled = enable;
}

bool cudaKernelPersistentCache::IsParallelCompilationEnabled() {
	return parallelCompilationEnabled;
}

//------------------------------------------------------------------------------
// cudaKernelPersistentCache
//------------------------------------------------------------------------------

std::filesystem::path cudaKernelPersistentCache::GetCacheDir(const std::string &applicationName) {
	return luxrays::GetCacheDir() / "cuda_kernel_cache" / SanitizeFileName(applicationName);
}

cudaKernelPersistentCache::cudaKernelPersistentCache(const std::string &applicationName) {
	appName = applicationName;

	// Crate the cache directory
	std::filesystem::create_directories(GetCacheDir(appName));
}

cudaKernelPersistentCache::~cudaKernelPersistentCache() {
}

bool cudaKernelPersistentCache::CompilePTX(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		std::unique_ptr<char[]> *ptx, std::size_t *ptxSize, bool *cached, std::string *error) {
	if (error)
		*error = "";

	// Check if the kernel is available in the cache

	const std::string kernelName =
			oclKernelPersistentCache::HashString(oclKernelPersistentCache::ToOptsString(kernelsParameters))
			+ "-" +
			oclKernelPersistentCache::HashString(kernelSource) +
                        "_compute_" + GetCuda10Architecture() + ".ptx";
	const std::filesystem::path dirPath = GetCacheDir(appName);
	const std::filesystem::path filePath = dirPath / kernelName;
	const std::string fileName = filePath.generic_string();

	*cached = false;

	// Thread-safe cache access
	{
		std::lock_guard<std::mutex> lock(cacheMutex);

		if (std::filesystem::exists(filePath)) {
			const std::size_t fileSize = std::filesystem::file_size(filePath);

			if (fileSize > 4) {
				*ptxSize = fileSize - 4;

				*ptx = std::make_unique<char[]>(*ptxSize);

				// The use of std::filesystem::path is required for UNICODE support: fileName
				// is supposed to be UTF-8 encoded.
				std::ifstream file(std::filesystem::path(fileName),
						std::ifstream::in | std::ifstream::binary);

				// Read the binary hash
				u_int hashBin;
				file.read((char *)&hashBin, sizeof(int));

				file.read(ptx->get(), *ptxSize);
				// Check for errors
				char buf[512];
				if (file.fail()) {
					sprintf(buf, "Unable to read kernel file cache %s", fileName.c_str());
					throw std::runtime_error(buf);
				}

				file.close();

				// Check the binary hash
				if (hashBin != oclKernelPersistentCache::HashBin(ptx->get(), *ptxSize)) {
					// Something wrong in the file, remove the file and retry
					std::filesystem::remove(filePath);
					return CompilePTX(kernelsParameters, kernelSource, programName, ptx, ptxSize, cached, error);
				} else {
					*cached = true;

					return true;
				}
			} else {
				// Something wrong in the file, remove the file and retry
				std::filesystem::remove(filePath);
				return CompilePTX(kernelsParameters, kernelSource, programName, ptx, ptxSize, cached, error);
			}
		}
	}

	// It isn't available, compile the source
	// Create the file only if the binaries include something
	if (ForcedCompilePTX(kernelsParameters, kernelSource, programName, ptx, ptxSize, error)) {
		// Add the kernel to the cache (thread-safe)
		std::lock_guard<std::mutex> lock(cacheMutex);

		std::filesystem::create_directories(dirPath);

		// The use of std::filesystem::path is required for UNICODE support: fileName
		// is supposed to be UTF-8 encoded.
		std::ofstream file(std::filesystem::path(fileName),
					std::ofstream::out |
					std::ofstream::binary |
					std::ofstream::trunc);

		// Write the binary hash
		const u_int hashBin = oclKernelPersistentCache::HashBin(ptx->get(), *ptxSize);
		file.write((char *)&hashBin, sizeof(int));

		file.write(ptx->get(), *ptxSize);
		// Check for errors
		char buf[512];
		if (file.fail()) {
			sprintf(buf, "Unable to write kernel file cache %s", fileName.c_str());
			throw std::runtime_error(buf);
		}

		file.close();

		return true;
	} else
		return false;
}

CUmodule cudaKernelPersistentCache::Compile(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		bool *cached, std::string *error) {
	std::unique_ptr<char[]> ptx;
	std::size_t ptxSize;
	if (CompilePTX(kernelsParameters, kernelSource, programName, &ptx, &ptxSize, cached, error)) {
		CUmodule module;
		CHECK_CUDA_ERROR(cuModuleLoadDataEx(&module, ptx.get(), 0, 0, 0));

		return module;
	} else
		return nullptr;
}

std::vector<CUmodule> cudaKernelPersistentCache::CompileMultiple(
		const std::vector<std::tuple<std::vector<std::string>, std::string, std::string>> &kernels,
		std::vector<bool> *cached, std::vector<std::string> *errors) {

	std::vector<CUmodule> modules(kernels.size(), nullptr);

	if (!IsParallelCompilationEnabled() || kernels.empty()) {
		// Fallback to sequential compilation
		if (cached) cached->resize(kernels.size());
		if (errors) errors->resize(kernels.size());

		for (std::size_t i = 0; i < kernels.size(); ++i) {
			const auto &[params, source, name] = kernels[i];
			bool isCached = false;
			std::string error;

			modules[i] = Compile(params, source, name, &isCached, &error);

			if (cached) (*cached)[i] = isCached;
			if (errors) (*errors)[i] = error;
		}

		return modules;
	}

	// Parallel compilation using TBB
	if (cached) cached->resize(kernels.size());
	if (errors) errors->resize(kernels.size());

	tbb::parallel_for(tbb::blocked_range<std::size_t>(0, kernels.size()),
		[&](const tbb::blocked_range<std::size_t> &range) {
			for (std::size_t i = range.begin(); i < range.end(); ++i) {
				const auto &[params, source, name] = kernels[i];
				bool isCached = false;
				std::string error;

				modules[i] = Compile(params, source, name, &isCached, &error);

				if (cached) (*cached)[i] = isCached;
				if (errors) (*errors)[i] = error;
			}
		});

	return modules;
}

//------------------------------------------------------------------------------
// Cache management
//------------------------------------------------------------------------------

void cudaKernelPersistentCache::ClearCache() {
	const std::filesystem::path cacheDir = GetCacheDir(appName);

	if (std::filesystem::exists(cacheDir)) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		std::filesystem::remove_all(cacheDir);
		std::filesystem::create_directories(cacheDir); // Recreate the directory
	}
}

void cudaKernelPersistentCache::ClearKernelCache(const std::string &kernelName) {
	const std::filesystem::path cacheDir = GetCacheDir(appName);
	const std::filesystem::path kernelPath = cacheDir / kernelName;

	if (std::filesystem::exists(kernelPath)) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		std::filesystem::remove(kernelPath);
	}
}

void cudaKernelPersistentCache::ClearAllCaches() {
	const std::filesystem::path baseCacheDir = luxrays::GetCacheDir() / "cuda_kernel_cache";

	if (std::filesystem::exists(baseCacheDir)) {
		// Use a separate std::mutex for global operations
		static std::mutex globalCacheMutex;
		std::lock_guard<std::mutex> lock(globalCacheMutex);
		std::filesystem::remove_all(baseCacheDir);
		std::filesystem::create_directories(baseCacheDir); // Recreate the directory
	}
}

// Global initialization for parallel compilation
void cudaKernelPersistentCache::InitializeParallelCompilationSystem() {
	SetParallelCompilation(true);
}

void cudaKernelPersistentCache::ShutdownParallelCompilationSystem() {
	SetParallelCompilation(false);
}

}  // namespace luxrays

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
