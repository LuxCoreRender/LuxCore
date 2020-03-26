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

#ifndef _LUXRAYS_CUDA_H
#define	_LUXRAYS_CUDA_H

#if defined(LUXRAYS_ENABLE_CUDA)

#include <cuda.h>

namespace luxrays {

#define CHECK_CUDA_ERROR(err) CheckCudaError(err, __FILE__, __LINE__)

inline void CheckCudaError(const CUresult err, const char *file, const int line) {
  if (err != CUDA_SUCCESS) {
	  const char *errorNameStr;
	  if (cuGetErrorName(err, &errorNameStr) != CUDA_SUCCESS)
		  errorNameStr = "cuGetErrorName(ERROR)";

	  const char *errorStr;
	  if (cuGetErrorString(err, &errorStr) != CUDA_SUCCESS)
		  errorStr = "cuGetErrorString(ERROR)";

	  throw std::runtime_error("CUDA driver API error " + std::string(errorNameStr) + " "
			  "(code: " + ToString(err) + ", file:" + std::string(file) + ", line: " + ToString(line) + ")"
			  ": " + std::string(errorStr) + "\n");
	}
}

}

#endif

#endif	/* _LUXRAYS_CUDA_H */
