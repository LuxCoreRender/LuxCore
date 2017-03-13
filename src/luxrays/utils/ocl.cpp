/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iostream>
#include <fstream>
#include <string.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/oclerror.h"
#include "luxrays/utils/oclcache.h"

using namespace std;
using namespace luxrays;

// Helper function to get error string
string luxrays::oclErrorString(cl_int error) {
	switch (error) {
		case CL_SUCCESS:
			return "CL_SUCCESS";
		case CL_DEVICE_NOT_FOUND:
			return "CL_DEVICE_NOT_FOUND";
		case CL_DEVICE_NOT_AVAILABLE:
			return "CL_DEVICE_NOT_AVAILABLE";
		case CL_COMPILER_NOT_AVAILABLE:
			return "CL_COMPILER_NOT_AVAILABLE";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:
			return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case CL_OUT_OF_RESOURCES:
			return "CL_OUT_OF_RESOURCES";
		case CL_OUT_OF_HOST_MEMORY:
			return "CL_OUT_OF_HOST_MEMORY";
		case CL_PROFILING_INFO_NOT_AVAILABLE:
			return "CL_PROFILING_INFO_NOT_AVAILABLE";
		case CL_MEM_COPY_OVERLAP:
			return "CL_MEM_COPY_OVERLAP";
		case CL_IMAGE_FORMAT_MISMATCH:
			return "CL_IMAGE_FORMAT_MISMATCH";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:
			return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
		case CL_BUILD_PROGRAM_FAILURE:
			return "CL_BUILD_PROGRAM_FAILURE";
		case CL_MAP_FAILURE:
			return "CL_MAP_FAILURE";
#ifdef CL_VERSION_1_1
		case CL_MISALIGNED_SUB_BUFFER_OFFSET:
			return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
			return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
#endif
		case CL_INVALID_VALUE:
			return "CL_INVALID_VALUE";
		case CL_INVALID_DEVICE_TYPE:
			return "CL_INVALID_DEVICE_TYPE";
		case CL_INVALID_PLATFORM:
			return "CL_INVALID_PLATFORM";
		case CL_INVALID_DEVICE:
			return "CL_INVALID_DEVICE";
		case CL_INVALID_CONTEXT:
			return "CL_INVALID_CONTEXT";
		case CL_INVALID_QUEUE_PROPERTIES:
			return "CL_INVALID_QUEUE_PROPERTIES";
		case CL_INVALID_COMMAND_QUEUE:
			return "CL_INVALID_COMMAND_QUEUE";
		case CL_INVALID_HOST_PTR:
			return "CL_INVALID_HOST_PTR";
		case CL_INVALID_MEM_OBJECT:
			return "CL_INVALID_MEM_OBJECT";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
			return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case CL_INVALID_IMAGE_SIZE:
			return "CL_INVALID_IMAGE_SIZE";
		case CL_INVALID_SAMPLER:
			return "CL_INVALID_SAMPLER";
		case CL_INVALID_BINARY:
			return "CL_INVALID_BINARY";
		case CL_INVALID_BUILD_OPTIONS:
			return "CL_INVALID_BUILD_OPTIONS";
		case CL_INVALID_PROGRAM:
			return "CL_INVALID_PROGRAM";
		case CL_INVALID_PROGRAM_EXECUTABLE:
			return "CL_INVALID_PROGRAM_EXECUTABLE";
		case CL_INVALID_KERNEL_NAME:
			return "CL_INVALID_KERNEL_NAME";
		case CL_INVALID_KERNEL_DEFINITION:
			return "CL_INVALID_KERNEL_DEFINITION";
		case CL_INVALID_KERNEL:
			return "CL_INVALID_KERNEL";
		case CL_INVALID_ARG_INDEX:
			return "CL_INVALID_ARG_INDEX";
		case CL_INVALID_ARG_VALUE:
			return "CL_INVALID_ARG_VALUE";
		case CL_INVALID_ARG_SIZE:
			return "CL_INVALID_ARG_SIZE";
		case CL_INVALID_KERNEL_ARGS:
			return "CL_INVALID_KERNEL_ARGS";
		case CL_INVALID_WORK_DIMENSION:
			return "CL_INVALID_WORK_DIMENSION";
		case CL_INVALID_WORK_GROUP_SIZE:
			return "CL_INVALID_WORK_GROUP_SIZE";
		case CL_INVALID_WORK_ITEM_SIZE:
			return "CL_INVALID_WORK_ITEM_SIZE";
		case CL_INVALID_GLOBAL_OFFSET:
			return "CL_INVALID_GLOBAL_OFFSET";
		case CL_INVALID_EVENT_WAIT_LIST:
			return "CL_INVALID_EVENT_WAIT_LIST";
		case CL_INVALID_EVENT:
			return "CL_INVALID_EVENT";
		case CL_INVALID_OPERATION:
			return "CL_INVALID_OPERATION";
		case CL_INVALID_GL_OBJECT:
			return "CL_INVALID_GL_OBJECT";
		case CL_INVALID_BUFFER_SIZE:
			return "CL_INVALID_BUFFER_SIZE";
		case CL_INVALID_MIP_LEVEL:
			return "CL_INVALID_MIP_LEVEL";
		case CL_INVALID_GLOBAL_WORK_SIZE:
			return "CL_INVALID_GLOBAL_WORK_SIZE";
		default:
			return boost::lexical_cast<string > (error);
	}
}

//------------------------------------------------------------------------------
// oclKernelCache
//------------------------------------------------------------------------------

cl::Program *oclKernelCache::ForcedCompile(cl::Context &context, cl::Device &device,
		const string &kernelsParameters, const string &kernelSource,
		cl::STRING_CLASS *error) {
	cl::Program *program = NULL;

	try {
		cl::Program::Sources source(1, make_pair(kernelSource.c_str(), kernelSource.length()));
		program = new cl::Program(context, source);

		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(device);
		program->build(buildDevice, kernelsParameters.c_str());
	} catch (cl::Error &err) {
		const string clerr = program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);

		stringstream ss;
		ss << "ERROR " << err.what() << "[" << oclErrorString(err.err()) << "]:" <<
				endl << clerr << endl;
		*error = ss.str();

		if (program)
			delete program;
		program = NULL;
	}

	return program;
}

//------------------------------------------------------------------------------
// oclKernelVolatileCache
//------------------------------------------------------------------------------

oclKernelVolatileCache::oclKernelVolatileCache() {

}

oclKernelVolatileCache::~oclKernelVolatileCache() {
	for (vector<char *>::iterator it = kernels.begin(); it != kernels.end(); it++)
		delete[] (*it);
}

cl::Program *oclKernelVolatileCache::Compile(cl::Context &context, cl::Device& device,
		const string &kernelsParameters, const string &kernelSource,
		bool *cached, cl::STRING_CLASS *error) {
	// Check if the kernel is available in the cache
	boost::unordered_map<string, cl::Program::Binaries>::iterator it = kernelCache.find(kernelsParameters);

	if (it == kernelCache.end()) {
		// It isn't available, compile the source
		cl::Program *program = ForcedCompile(
				context, device, kernelsParameters, kernelSource, error);
		if (!program)
			return NULL;

		// Obtain the binaries of the sources
		VECTOR_CLASS<char *> bins = program->getInfo<CL_PROGRAM_BINARIES>();
		assert (bins.size() == 1);
		VECTOR_CLASS<size_t> sizes = program->getInfo<CL_PROGRAM_BINARY_SIZES>();
		assert (sizes.size() == 1);

		if (sizes[0] > 0) {
			// Add the kernel to the cache
			char *bin = new char[sizes[0]];
			memcpy(bin, bins[0], sizes[0]);
			kernels.push_back(bin);

			kernelCache[kernelsParameters] = cl::Program::Binaries(1, make_pair(bin, sizes[0]));
		}

		if (cached)
			*cached = false;

		return program;
	} else {
		// Compile from the binaries
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(device);
		cl::Program *program = new cl::Program(context, buildDevice, it->second);
		program->build(buildDevice);

		if (cached)
			*cached = true;

		return program;
	}
}

//------------------------------------------------------------------------------
// oclKernelPersistentCache
//------------------------------------------------------------------------------

static string SanitizeFileName(const string &name) {
	string sanitizedName(name.size(), '_');
	
	for (u_int i = 0; i < sanitizedName.size(); ++i) {
		if ((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z') ||
				(name[i] >= '0' && name[i] <= '9'))
			sanitizedName[i] = name[i];
	}

	return sanitizedName;
}

boost::filesystem::path oclKernelPersistentCache::GetCacheDir(const string &applicationName) const {
#if defined(__linux__)
	// boost::filesystem::temp_directory_path() is usually mapped to /tmp and
	// the content of the directory is often delete at each reboot
	boost::filesystem::path kernelCacheDir(getenv("HOME"));
	kernelCacheDir = kernelCacheDir / ".config" / "luxrender.net";
#else
	boost::filesystem::path kernelCacheDir= boost::filesystem::temp_directory_path();
#endif

	return kernelCacheDir / "kernel_cache" / SanitizeFileName(applicationName);
}

oclKernelPersistentCache::oclKernelPersistentCache(const string &applicationName) {
	appName = applicationName;

	// Crate the cache directory
	boost::filesystem::create_directories(GetCacheDir(appName));
}

oclKernelPersistentCache::~oclKernelPersistentCache() {
}

// Bob Jenkins's One-at-a-Time hash
// From: http://eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx

string oclKernelPersistentCache::HashString(const string &ss) {
	const u_int hash = HashBin(ss.c_str(), ss.length());

	char buf[9];
	sprintf(buf, "%08x", hash);

	return string(buf);
}

u_int oclKernelPersistentCache::HashBin(const char *s, const size_t size) {
	u_int hash = 0;

	for (u_int i = 0; i < size; ++i) {
		hash += *s++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

cl::Program *oclKernelPersistentCache::Compile(cl::Context &context, cl::Device& device,
		const string &kernelsParameters, const string &kernelSource,
		bool *cached, cl::STRING_CLASS *error) {
	// Check if the kernel is available in the cache

	const cl::Platform platform = device.getInfo<CL_DEVICE_PLATFORM>();
	const string platformName = boost::trim_copy(platform.getInfo<CL_PLATFORM_VENDOR>());
	const string deviceName = boost::trim_copy(device.getInfo<CL_DEVICE_NAME>());
	const string deviceUnits = ToString(device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>());
	const string kernelName = HashString(kernelsParameters) + "-" + HashString(kernelSource) + ".ocl";
	const boost::filesystem::path dirPath = GetCacheDir(appName) / SanitizeFileName(platformName) /
		SanitizeFileName(deviceName) / SanitizeFileName(deviceUnits);
	const boost::filesystem::path filePath = dirPath / kernelName;
	const string fileName = filePath.generic_string();
	
	if (!boost::filesystem::exists(filePath)) {
		// It isn't available, compile the source
		cl::Program *program = ForcedCompile(
				context, device, kernelsParameters, kernelSource, error);
		if (!program)
			return NULL;

		// Obtain the binaries of the sources
		VECTOR_CLASS<char *> bins = program->getInfo<CL_PROGRAM_BINARIES>();
		assert (bins.size() == 1);
		VECTOR_CLASS<size_t> sizes = program->getInfo<CL_PROGRAM_BINARY_SIZES>();
		assert (sizes.size() == 1);

		// Create the file only if the binaries include something
		if (sizes[0] > 0) {
			// Add the kernel to the cache
			boost::filesystem::create_directories(dirPath);
			BOOST_OFSTREAM file(fileName.c_str(), ios_base::out | ios_base::binary);

			// Write the binary hash
			const u_int hashBin = HashBin(bins[0], sizes[0]);
			file.write((char *)&hashBin, sizeof(int));

			file.write(bins[0], sizes[0]);
			// Check for errors
			char buf[512];
			if (file.fail()) {
				sprintf(buf, "Unable to write kernel file cache %s", fileName.c_str());
				throw runtime_error(buf);
			}

			file.close();
		}

		if (cached)
			*cached = false;

		return program;
	} else {
		const size_t fileSize = boost::filesystem::file_size(filePath);

		if (fileSize > 4) {
			const size_t kernelSize = fileSize - 4;

			char *kernelBin = new char[kernelSize];

			BOOST_IFSTREAM file(fileName.c_str(), ios_base::in | ios_base::binary);

			// Read the binary hash
			u_int hashBin;
			file.read((char *)&hashBin, sizeof(int));

			file.read(kernelBin, kernelSize);
			// Check for errors
			char buf[512];
			if (file.fail()) {
				sprintf(buf, "Unable to read kernel file cache %s", fileName.c_str());
				throw runtime_error(buf);
			}

			file.close();

			// Check the binary hash
			if (hashBin != HashBin(kernelBin, kernelSize)) {
				// Something wrong in the file, remove the file and retry
				boost::filesystem::remove(filePath);
				return Compile(context, device, kernelsParameters, kernelSource, cached, error);
			} else {
				// Compile from the binaries
				VECTOR_CLASS<cl::Device> buildDevice;
				buildDevice.push_back(device);
				cl::Program *program = new cl::Program(context, buildDevice,
						cl::Program::Binaries(1, make_pair(kernelBin, kernelSize)));
				program->build(buildDevice);

				if (cached)
					*cached = true;

				delete[] kernelBin;

				return program;
			}
		} else {
			// Something wrong in the file, remove the file and retry
			boost::filesystem::remove(filePath);
			return Compile(context, device, kernelsParameters, kernelSource, cached, error);
		}
	}
}

#endif
