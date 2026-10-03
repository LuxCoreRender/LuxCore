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

#ifndef _LUXRAYS_CUDAPARALLEL_H
#define	_LUXRAYS_CUDAPARALLEL_H

#include "luxrays/utils/cudacache.h"

#if !defined(LUXRAYS_DISABLE_CUDA)

namespace luxrays {

//------------------------------------------------------------------------------
// Parallel CUDA kernel compilation helper
//------------------------------------------------------------------------------

/**
 * RAII class to manage parallel CUDA kernel compilation lifecycle
 */
class CUDAParallelCompilationManager {
public:
	/**
	 * Enable parallel CUDA kernel compilation
	 * TBB will automatically use the optimal number of threads
	 */
	CUDAParallelCompilationManager() {
		cudaKernelPersistentCache::SetParallelCompilation(true);
	}
	
	~CUDAParallelCompilationManager() {
		cudaKernelPersistentCache::SetParallelCompilation(false);
	}
	
	/**
	 * Check if parallel compilation is enabled
	 */
	bool IsEnabled() const {
		return cudaKernelPersistentCache::IsParallelCompilationEnabled();
	}
	
	/**
	 * Enable or disable parallel compilation
	 */
	void SetEnabled(bool enable) {
		cudaKernelPersistentCache::SetParallelCompilation(enable);
	}
};

/**
 * Helper function to compile multiple CUDA kernels in parallel
 * 
 * @param cache The kernel cache instance
 * @param kernelSources Vector of tuples containing (parameters, source, name)
 * @param cached Optional vector to receive cache hit information
 * @param errors Optional vector to receive compilation errors
 * @return Vector of compiled CUDA modules
 */
inline std::vector<CUmodule> CompileCUDAKernelsParallel(
	cudaKernelPersistentCache &cache,
	const std::vector<std::tuple<std::vector<std::string>, std::string, std::string>> &kernelSources,
	std::vector<bool> *cached = nullptr,
	std::vector<std::string> *errors = nullptr) {
	
	return cache.CompileMultiple(kernelSources, cached, errors);
}

} // namespace luxrays

#endif // !defined(LUXRAYS_DISABLE_CUDA)

#endif /* _LUXRAYS_CUDAPARALLEL_H */

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4