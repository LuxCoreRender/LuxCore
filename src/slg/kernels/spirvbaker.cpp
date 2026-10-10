/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)
 *
 *   This file is part of LuxCoreRender.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

// Tool used at build time to pre-compile the OpenCL kernels in SPIR-V
// format. It writes, for each render engine and OpenCL platform vendor, the
// exact source and parameters used at run time so that the kernel cache hash
// matches the one computed at run time (see
// oclKernelPersistentCache::Compile()).
//
// The generated .cl files are compiled to SPIR-V by the host clang and
// embedded in LuxCore binaries (see src/slg/CMakeLists.txt).

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "luxrays/core/epsilon.h"
#include "luxrays/kernels/kernels.h"
#include "luxrays/utils/oclcache.h"

#include "slg/kernels/kernels.h"
#include "slg/kernels/kernelsources.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace slg::ocl;

// The same parameters added by OpenCLDevice::CompilePrograms()
static void AddDeviceParameters(vector<string> &params) {
	// Note: no LUXRAYS_OS_* related parameter: it is not used by the kernel
	// sources and the kernel cache hash must be platform independent
	// (see OpenCLDevice::GetCompleteProgramParameters())
	params.push_back("-D LUXRAYS_OPENCL_DEVICE");
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		cerr << "Usage: " << argv[0] << " <output dir>" << endl;
		return EXIT_FAILURE;
	}

	const string outDir = argv[1];

	// The exact source compiled at run time when opencl.splitkernels.enable
	// is disabled (i.e. the monolithic source)
	const string fullSource =
		luxrays::ocl::KernelSource_ocldevice_funcs +
		GetPathOCLBaseKernelSources() +
		KernelSource_pathoclbase_kernels_micro;

	const string renderEngineTypes[] = { "PATHOCL", "TILEPATHOCL", "RTPATHOCL" };
	// (parameter, short name used in the file names)
	const std::pair<const char *, const char *> platformDefines[] = {
		{ "-D LUXCORE_GENERIC_OPENCL", "generic" },
		{ "-D LUXCORE_AMD_OPENCL", "amd" },
		{ "-D LUXCORE_NVIDIA_OPENCL", "nvidia" }
	};

	ofstream manifest(outDir + "/spirvmanifest.txt");
	if (!manifest) {
		cerr << "Unable to write " << outDir << "/spirvmanifest.txt" << endl;
		return EXIT_FAILURE;
	}

	for (const string &renderEngineType : renderEngineTypes) {
		for (const auto &platformDefine : platformDefines) {
			vector<string> params;
			GetKernelParamters(params, renderEngineType,
				MachineEpsilon::GetMin(), MachineEpsilon::GetMax());
			params.push_back(platformDefine.first);
			AddDeviceParameters(params);

			// The file names must be stable (i.e. independent of the hash) in
			// order to be known at CMake configuration time
			string stableName = renderEngineType;
			transform(stableName.begin(), stableName.end(), stableName.begin(), ::tolower);
			stableName += string("_") + platformDefine.second;
			const string baseName = outDir + "/" + stableName;

			const string hash =
				oclKernelPersistentCache::HashString(oclKernelPersistentCache::ToOptsString(params)) +
				"-" + oclKernelPersistentCache::HashString(fullSource);

			// Write the OpenCL source
			const string srcFileName = baseName + ".cl";
			ofstream srcFile(srcFileName, ofstream::out | ofstream::trunc);
			if (!srcFile) {
				cerr << "Unable to write " << srcFileName << endl;
				return EXIT_FAILURE;
			}
			srcFile << fullSource;
			srcFile.close();

			// Write the parameters used to compile the source
			const string paramsFileName = baseName + ".params";
			ofstream paramsFile(paramsFileName, ofstream::out | ofstream::trunc);
			if (!paramsFile) {
				cerr << "Unable to write " << paramsFileName << endl;
				return EXIT_FAILURE;
			}
			for (const string &param : params)
				paramsFile << param << endl;
			paramsFile.close();

			manifest << stableName << " " << hash << endl;
		}
	}

	return EXIT_SUCCESS;
}
