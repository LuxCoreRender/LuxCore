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

#ifndef _LUXRAYS_CUDACPP_H
#define	_LUXRAYS_CUDACPP_H

#if !defined(LUXCORE_DISABLE_CUDA_CPLUSPLUS)

#include <stdexcept>

#include "luxrays/utils/strutils.h"

namespace luxrays {

#define CHECK_CUDACPP_ERROR() luxrays::CheckCUDACPPError(cudaGetLastError(), __FILE__, __LINE__)

inline void CheckCUDACPPError(const cudaError_t error, const char *file, const int line) {
	if (error != cudaSuccess)
		throw std::runtime_error("CUDA C++ error: " + std::string(cudaGetErrorString(error)) + " "
				"(file:" + std::string(file) + ", line: " + ToString(line) + ")\n");
}

}

#endif

#endif	/* _LUXRAYS_CUDACPP_H */
