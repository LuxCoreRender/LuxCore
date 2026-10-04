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

#ifndef _LUXRAYS_CUDACACHE_H
#define	_LUXRAYS_CUDACACHE_H

#include <string>
#include <filesystem>

#include "luxrays/utils/cuda.h"

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <mutex>
#include <atomic>
#include <oneapi/tbb.h>

namespace luxrays {

class cudaKernelCache {
public:
	cudaKernelCache() { }
	virtual ~cudaKernelCache() { }

	virtual CUmodule Compile(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		bool *cached, std::string *error) = 0;
	
	static bool ForcedCompilePTX(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		std::unique_ptr<char[]> * ptx, size_t *ptxSize, std::string *error);
};

// Thread-safe class for CUDA kernel compilation with parallel support
class cudaKernelPersistentCache : public cudaKernelCache {
public:
	cudaKernelPersistentCache(const std::string &applicationName);
	~cudaKernelPersistentCache();

	bool CompilePTX(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		std::unique_ptr<char[]> * ptx, size_t *ptxSize, bool *cached, std::string *error);

	virtual CUmodule Compile(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		bool *cached, std::string *error);

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
	void ClearKernelCache(const std::string &kernelName);

	// Clear all CUDA kernel caches (all applications)
	static void ClearAllCaches();

	//------------------------------------------------------------------------------
	// Parallel compilation support
	//------------------------------------------------------------------------------

	// Enable/disable parallel compilation
	static void SetParallelCompilation(bool enable);
	static bool IsParallelCompilationEnabled();

	// Compile multiple kernels in parallel using TBB
	std::vector<CUmodule> CompileMultiple(
		const std::vector<std::tuple<std::vector<std::string>, std::string, std::string>> &kernels,
		std::vector<bool> *cached = nullptr,
		std::vector<std::string> *errors = nullptr
	);

private:
	std::string appName;
	
	// Thread safety
	std::mutex cacheMutex;
	
	// Parallel compilation state
	static std::atomic<bool> parallelCompilationEnabled;
	
	// Global system initialization
	static void InitializeParallelCompilationSystem();
	static void ShutdownParallelCompilationSystem();
};

}

#endif

#endif	/* _LUXRAYS_CUDACACHE_H */

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
