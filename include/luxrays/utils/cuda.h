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

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <cuew.h>
#define OPTIX_DONT_INCLUDE_CUDA
#include <optix_stubs.h>

namespace luxrays {

#define CHECK_CUDA_ERROR(err) CheckCUDAError(err, __FILE__, __LINE__)

inline void CheckCUDAError(const CUresult err, const char *file, const int line) {
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

#define CHECK_NVRTC_ERROR(err) CheckNVRTCError(err, __FILE__, __LINE__)

inline void CheckNVRTCError(const nvrtcResult err, const char *file, const int line) {
  if (err != NVRTC_SUCCESS) {
	  throw std::runtime_error("CUDA NVRTC error "
			  "(code: " + ToString(err) + ", file:" + std::string(file) + ", line: " + ToString(line) + ")"
			  ": " + std::string(nvrtcGetErrorString(err)) + "\n");
	}
}

}

#endif

#endif	/* _LUXRAYS_CUDA_H */
