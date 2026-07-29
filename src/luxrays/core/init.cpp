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

#include <iostream>
#include <filesystem>

#include "luxrays/luxrays.h"
#if !defined(LUXRAYS_DISABLE_OPENCL)
#include "luxrays/utils/ocl.h"
#endif
#if !defined(LUXRAYS_DISABLE_CUDA)
#include "luxrays/utils/cuda.h"
#endif

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// Init()
//------------------------------------------------------------------------------

namespace luxrays {

void Init() {

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (clewInit() == CLEW_SUCCESS) {
		isOpenCLAvilable = true;
	}
#endif

#if !defined(LUXRAYS_DISABLE_CUDA)
	if (cuewInit(CUEW_INIT_CUDA | CUEW_INIT_NVRTC) == CUEW_SUCCESS) {
		// Was:
		//CHECK_CUDA_ERROR(cuInit(0));

		const CUresult err = cuInit(0);
		if (err != CUDA_SUCCESS) {
			// Handling multiple cases like:
			//
			// - when CUDA is installed but there are no NVIDIA GPUs available (CUDA_ERROR_NO_DEVICE)
			//
			// - when CUDA is installed but there an external GPU is unplugged (CUDA_ERROR_UNKNOWN)
			//
			// In all above cases, I just disable CUDA (but with this solution, at
			/// the moment I have no way to report the type of error).
			switch(err) {
				case CUDA_ERROR_INVALID_VALUE:
					std::cerr << "Cuda error: invalid value\n"; break;
				case CUDA_ERROR_INVALID_DEVICE:
					std::cerr << "Cuda error: invalid device\n"; break;
				case CUDA_ERROR_SYSTEM_DRIVER_MISMATCH:
					std::cerr << "Cuda error: system driver mismatch\n"; break;
				case CUDA_ERROR_COMPAT_NOT_SUPPORTED_ON_DEVICE:
					std::cerr << "Cuda error: compat not supported on device\n"; break;
				case CUDA_ERROR_OUT_OF_MEMORY:
					std::cerr << "Cuda error: out of memory\n"; break;
				default:
					std::cerr << "Cuda error: undefined\n"; break;
			}


		} else {
			isCudaAvilable = true;
			const OptixResult optixResult = optixInit();
			if (optixResult == OPTIX_SUCCESS)
				isOptixAvilable = true;
			else
				std::cerr << "Optix initialization failed: " <<
						optixGetErrorName(optixResult) << "\n";
		}
	}
#endif
}

}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
