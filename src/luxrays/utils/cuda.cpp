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

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <iostream>
#include <fstream>
#include <string.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/config.h"
#include "luxrays/utils/cudacache.h"
#include "luxrays/utils/oclcache.h"

#include <optix_function_table_definition.h>

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// cudaKernelCache
//------------------------------------------------------------------------------

bool cudaKernelCache::ForcedCompilePTX(const vector<string> &kernelsParameters, const string &kernelSource,
		 const string &programName, char **ptx, size_t *ptxSize, string *error) {
	if (error)
		*error = "";

	nvrtcProgram prog;
	CHECK_NVRTC_ERROR(nvrtcCreateProgram(&prog, kernelSource.c_str(), programName.c_str(), 0, nullptr, nullptr));
  
	vector<const char *> cudaOpts;
	cudaOpts.push_back("--device-as-default-execution-space");
	//cudaOpts.push_back("--disable-warnings");

	// To display warning numbers
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--display_error_number");
	
	// To suppress warning: warning #550-D: variable "xyz" was set but never used
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--diag_suppress=550");
	
	// To suppress warning: warning #1055-D: types cannot be declared in anonymous unions
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--diag_suppress=1055");

	// To suppress warning: warning #68-D: integer conversion resulted in a change of sign
	cudaOpts.push_back("-Xcudafe");
	cudaOpts.push_back("--diag_suppress=68");

	for	(auto const &p : kernelsParameters)
		cudaOpts.push_back(p.c_str());

	// For some debug
	//for (uint i = 0; i < cudaOpts.size(); ++i)
	//	cout << "Opt #" << i <<" : [" << cudaOpts[i] << "]\n";

	const nvrtcResult compilationResult = nvrtcCompileProgram(prog,
			cudaOpts.size(),
			(cudaOpts.size() > 0) ? &cudaOpts[0] : nullptr);

	size_t logSize;
	CHECK_NVRTC_ERROR(nvrtcGetProgramLogSize(prog, &logSize));
	unique_ptr<char> log(new char[logSize]);
	CHECK_NVRTC_ERROR(nvrtcGetProgramLog(prog, log.get()));

	*error = string(log.get());

	if (compilationResult != NVRTC_SUCCESS)
		return false;

	// Obtain PTX from the program.
	CHECK_NVRTC_ERROR(nvrtcGetPTXSize(prog, ptxSize));
	*ptx = new char[*ptxSize];
	CHECK_NVRTC_ERROR(nvrtcGetPTX(prog, *ptx));

	CHECK_NVRTC_ERROR(nvrtcDestroyProgram(&prog));

	return true;
}

//------------------------------------------------------------------------------
// cudaKernelPersistentCache
//------------------------------------------------------------------------------

boost::filesystem::path cudaKernelPersistentCache::GetCacheDir(const string &applicationName) {
	return GetConfigDir() / "cuda_kernel_cache" / SanitizeFileName(applicationName);
}

cudaKernelPersistentCache::cudaKernelPersistentCache(const string &applicationName) {
	appName = applicationName;

	// Crate the cache directory
	boost::filesystem::create_directories(GetCacheDir(appName));
}

cudaKernelPersistentCache::~cudaKernelPersistentCache() {
}

bool cudaKernelPersistentCache::CompilePTX(const vector<string> &kernelsParameters,
		const string &kernelSource, const string &programName,
		char **ptx, size_t *ptxSize, bool *cached, string *error) {
	if (error)
		*error = "";

	// Check if the kernel is available in the cache

	const string kernelName =
			oclKernelPersistentCache::HashString(oclKernelPersistentCache::ToOptsString(kernelsParameters))
			+ "-" +
			oclKernelPersistentCache::HashString(kernelSource) + ".ptx";
	const boost::filesystem::path dirPath = GetCacheDir(appName);
	const boost::filesystem::path filePath = dirPath / kernelName;
	const string fileName = filePath.generic_string();
	
	*cached = false;
	if (!boost::filesystem::exists(filePath)) {
		// It isn't available, compile the source

		// Create the file only if the binaries include something
		if (ForcedCompilePTX(kernelsParameters, kernelSource, programName, ptx, ptxSize, error)) {
			// Add the kernel to the cache
			boost::filesystem::create_directories(dirPath);

			// The use of boost::filesystem::path is required for UNICODE support: fileName
			// is supposed to be UTF-8 encoded.
			boost::filesystem::ofstream file(boost::filesystem::path(fileName),
					boost::filesystem::ofstream::out |
					boost::filesystem::ofstream::binary |
					boost::filesystem::ofstream::trunc);

			// Write the binary hash
			const u_int hashBin = oclKernelPersistentCache::HashBin(*ptx, *ptxSize);
			file.write((char *)&hashBin, sizeof(int));

			file.write(*ptx, *ptxSize);
			// Check for errors
			char buf[512];
			if (file.fail()) {
				sprintf(buf, "Unable to write kernel file cache %s", fileName.c_str());
				throw runtime_error(buf);
			}

			file.close();

			return true;
		} else
			return false;
	} else {
		const size_t fileSize = boost::filesystem::file_size(filePath);

		if (fileSize > 4) {
			*ptxSize = fileSize - 4;

			*ptx = new char[*ptxSize];

			// The use of boost::filesystem::path is required for UNICODE support: fileName
			// is supposed to be UTF-8 encoded.
			boost::filesystem::ifstream file(boost::filesystem::path(fileName),
					boost::filesystem::ifstream::in | boost::filesystem::ifstream::binary);

			// Read the binary hash
			u_int hashBin;
			file.read((char *)&hashBin, sizeof(int));

			file.read(*ptx, *ptxSize);
			// Check for errors
			char buf[512];
			if (file.fail()) {
				sprintf(buf, "Unable to read kernel file cache %s", fileName.c_str());
				throw runtime_error(buf);
			}

			file.close();

			// Check the binary hash
			if (hashBin != oclKernelPersistentCache::HashBin(*ptx, *ptxSize)) {
				// Something wrong in the file, remove the file and retry
				boost::filesystem::remove(filePath);
				return CompilePTX(kernelsParameters, kernelSource, programName, ptx, ptxSize, cached, error);
			} else {
				*cached = true;

				return true;
			}
		} else {
			// Something wrong in the file, remove the file and retry
			boost::filesystem::remove(filePath);
			return CompilePTX(kernelsParameters, kernelSource, programName, ptx, ptxSize, cached, error);
		}
	}
}

CUmodule cudaKernelPersistentCache::Compile(const vector<string> &kernelsParameters,
		const string &kernelSource, const string &programName,
		bool *cached, string *error) {
	char *ptx;
	size_t ptxSize;
	if (CompilePTX(kernelsParameters, kernelSource, programName, &ptx, &ptxSize, cached, error)) {
		CUmodule module;
		CHECK_CUDA_ERROR(cuModuleLoadDataEx(&module, ptx, 0, 0, 0));

		delete[] ptx;
		
		return module;
	} else
		return nullptr;
}

#endif
