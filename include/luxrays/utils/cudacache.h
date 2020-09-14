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
#include <boost/filesystem.hpp>
#include <boost/unordered_map.hpp>

#include "luxrays/utils/cuda.h"

#if !defined(LUXRAYS_DISABLE_CUDA)

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
		char **ptx, size_t *ptxSize, std::string *error);
};

// WARNING: this class is not thread safe !
class cudaKernelPersistentCache : public cudaKernelCache {
public:
	cudaKernelPersistentCache(const std::string &applicationName);
	~cudaKernelPersistentCache();

	bool CompilePTX(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		char **ptx, size_t *ptxSize, bool *cached, std::string *error);

	virtual CUmodule Compile(const std::vector<std::string> &kernelsParameters,
		const std::string &kernelSource, const std::string &programName,
		bool *cached, std::string *error);

	static boost::filesystem::path GetCacheDir(const std::string &applicationName);

private:
	std::string appName;
};

}

#endif

#endif	/* _LUXRAYS_CUDACACHE_H */

