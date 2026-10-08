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

#ifndef _LUXRAYS_OPENCLCACHE_H
#define	_LUXRAYS_OPENCLCACHE_H

#include <string>
#include <filesystem>

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <mutex>
#include <atomic>
#include <oneapi/tbb.h>
#include "luxrays/utils/ocl.h"

namespace luxrays {

// OpenCL vendor detection
std::string GetOpenCLVendor(cl_device_id device);

// OpenCL compiler information (platform vendor/version, driver, compiler availability)
std::string GetOpenCLCompilerInfo(cl_device_id device);

class oclKernelCache {
public:
	oclKernelCache() { }
	virtual ~oclKernelCache() { }

	virtual cl_program Compile(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
		bool *cached, std::string *errorStr) = 0;

	// Compile multiple kernel programs. The default implementation compiles
	// them sequentially; derived classes may compile them in parallel.
	virtual std::vector<cl_program> CompileMultiple(
		cl_context context, cl_device_id device,
		const std::vector<std::tuple<std::vector<std::string>, std::string>> &kernels,
		std::vector<bool> *cached = nullptr,
		std::vector<std::string> *errors = nullptr
	) {
		std::vector<cl_program> programs(kernels.size(), nullptr);
		if (cached)
			cached->resize(kernels.size());
		if (errors)
			errors->resize(kernels.size());

		for (std::size_t i = 0; i < kernels.size(); ++i) {
			const auto &[params, source] = kernels[i];
			bool isCached = false;
			std::string error;

			programs[i] = Compile(context, device, params, source, &isCached, &error);

			if (cached)
				(*cached)[i] = isCached;
			if (errors)
				(*errors)[i] = error;
		}

		return programs;
	}

	static std::string ToOptsString(const std::vector<std::string> &kernelsParameters);
	static cl_program ForcedCompile(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
		std::string *errorStr);
};

class oclKernelDummyCache : public oclKernelCache {
public:
	oclKernelDummyCache() { }
	~oclKernelDummyCache() { }

	virtual cl_program Compile(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
		bool *cached, std::string *errorStr) {
		if (cached)
			*cached = false;

		return ForcedCompile(context, device, kernelsParameters, kernelSource, errorStr);
	}
};

// Thread-safe class for OpenCL kernel compilation with parallel support
class oclKernelPersistentCache : public oclKernelCache {
public:
	oclKernelPersistentCache(const std::string &applicationName);
	~oclKernelPersistentCache();

	virtual cl_program Compile(cl_context context, cl_device_id device,
		const std::vector<std::string> &kernelsParameters, const std::string &kernelSource,
		bool *cached, std::string *errorStr);

	// Parallel compilation support
	//------------------------------------------------------------------------------

	// Enable/disable parallel compilation
	static void SetParallelCompilation(bool enable);
	static bool IsParallelCompilationEnabled();

	// Compile multiple kernels in parallel using TBB
	virtual std::vector<cl_program> CompileMultiple(
		cl_context context, cl_device_id device,
		const std::vector<std::tuple<std::vector<std::string>, std::string>> &kernels,
		std::vector<bool> *cached = nullptr,
		std::vector<std::string> *errors = nullptr
	);

	static std::string HashString(const std::string &ss);
	static u_int HashBin(const char *s, const size_t size);

	static std::filesystem::path GetCacheDir(const std::string &applicationName);

	std::string GetApplicationName() {
		return appName;
	}

	//------------------------------------------------------------------------------
	// Cache management
	//------------------------------------------------------------------------------

	// Clear all cached kernels for this application
	void ClearCache();

	// Clear a specific kernel from cache
	void ClearKernelCache(cl_context context, cl_device_id device, const std::string &kernelName);

	// Clear all OpenCL kernel caches (all applications)
	static void ClearAllCaches();

private:
	std::string appName;
	
	// Thread safety
	std::mutex cacheMutex;
	
	// Parallel compilation state
	static std::atomic<bool> parallelCompilationEnabled;
};

}

#endif

#endif	/* _LUXRAYS_OPENCLCACHE_H */

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
