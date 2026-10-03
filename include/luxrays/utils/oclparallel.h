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

#ifndef _LUXRAYS_OCLPARALLEL_H
#define	_LUXRAYS_OCLPARALLEL_H

#include "luxrays/utils/oclcache.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays {

//------------------------------------------------------------------------------
// Parallel OpenCL kernel compilation helper
//------------------------------------------------------------------------------

/**
 * RAII class to manage parallel OpenCL kernel compilation lifecycle
 */
class OCLParallelCompilationManager {
public:
	/**
	 * Enable parallel OpenCL kernel compilation
	 * TBB will automatically use the optimal number of threads
	 */
	OCLParallelCompilationManager() {
		oclKernelPersistentCache::SetParallelCompilation(true);
	}
	
	~OCLParallelCompilationManager() {
		oclKernelPersistentCache::SetParallelCompilation(false);
	}
	
	/**
	 * Check if parallel compilation is enabled
	 */
	bool IsEnabled() const {
		return oclKernelPersistentCache::IsParallelCompilationEnabled();
	}
	
	/**
	 * Enable or disable parallel compilation
	 */
	void SetEnabled(bool enable) {
		oclKernelPersistentCache::SetParallelCompilation(enable);
	}
};

/**
 * Helper function to compile multiple OpenCL kernels in parallel
 * 
 * @param cache The kernel cache instance
 * @param context OpenCL context
 * @param device OpenCL device
 * @param kernelSources Vector of tuples containing (parameters, source)
 * @param cached Optional vector to receive cache hit information
 * @param errors Optional vector to receive compilation errors
 * @return Vector of compiled OpenCL programs
 */
inline std::vector<cl_program> CompileOCLKernelsParallel(
	oclKernelPersistentCache &cache,
	cl_context context, cl_device_id device,
	const std::vector<std::tuple<std::vector<std::string>, std::string>> &kernelSources,
	std::vector<bool> *cached = nullptr,
	std::vector<std::string> *errors = nullptr) {
	
	return cache.CompileMultiple(context, device, kernelSources, cached, errors);
}

} // namespace luxrays

#endif // !defined(LUXRAYS_DISABLE_OPENCL)

#endif /* _LUXRAYS_OCLPARALLEL_H */

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4