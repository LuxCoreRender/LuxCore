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
#include <boost/filesystem.hpp>

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

bool isOpenCLAvilable = false;
bool isCudaAvilable = false;
bool isOptixAvilable = false;

std::locale cLocale("C");

void Init() {
#if defined(WIN32)
	// Set locale for conversion from UTF-16 to UTF-8 on Windows. LuxRays/LuxCore assume
	// all file names are UTF-8 encoded. This works fine on Linux/MacOS but
	// requires a conversion to UTF-16 on Windows.

	boost::filesystem::path::imbue(
			std::locale(std::locale(), new std::codecvt_utf8_utf16<wchar_t>()));
#endif

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
		} else {
			isCudaAvilable = true;

			// Try to initialize Optix too
			if (optixInit() == OPTIX_SUCCESS)
				isOptixAvilable = true;
		}
	}
#endif
}

}
