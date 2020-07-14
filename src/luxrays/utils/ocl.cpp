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
#include "luxrays/utils/config.h"

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

string oclKernelCache::ToOptsString(const vector<string> &kernelsParameters) {
	string result;
	
	for	(auto const &p : kernelsParameters) {
		if (result.length() != 0)
			result += " ";
		result += p;
	}

	return result;
}

cl_program oclKernelCache::ForcedCompile(cl_context context, cl_device_id device,
		const vector<string> &kernelsParameters, const string &kernelSource,
		string *errorStr) {
	const char *kernelSources[1] = { kernelSource.c_str() };
	const size_t sourceSizes[1] = { kernelSource.length() };
	cl_int error;
	cl_program program = clCreateProgramWithSource(context, 1, kernelSources, sourceSizes, &error);
	CHECK_OCL_ERROR(error);

	const string optsStr = ToOptsString(kernelsParameters);
	error = clBuildProgram(program, 1, &device, optsStr.c_str(), nullptr, nullptr);
	if (error != CL_SUCCESS) {
		if (errorStr) {
			string logStr;
			if (program) {
				size_t valueSize;
				CHECK_OCL_ERROR(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, &valueSize, nullptr));
				char *value = (char *)alloca(valueSize * sizeof(char));
				CHECK_OCL_ERROR(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, valueSize, value, nullptr));

				logStr = string(value);
			} else
				logStr = "Build info not available";

			*errorStr = "ERROR " + ToString(error) + "[" + oclErrorString(error) + "]:" +
				"\n" + logStr + "\n";
		}

		if (program)
			CHECK_OCL_ERROR(clReleaseProgram(program));
		program = nullptr;
	}

	return program;
}

//------------------------------------------------------------------------------
// oclKernelPersistentCache
//------------------------------------------------------------------------------

boost::filesystem::path oclKernelPersistentCache::GetCacheDir(const string &applicationName) {
	return GetConfigDir() / "ocl_kernel_cache" / SanitizeFileName(applicationName);
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

cl_program oclKernelPersistentCache::Compile(cl_context context, cl_device_id device,
		const vector<string> &kernelsParameters, const string &kernelSource,
		bool *cached, string *errorStr) {
	// Check if the kernel is available inside the cache

	cl_platform_id platform;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr));
	
	size_t platformNameSize;
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 0, nullptr, &platformNameSize));
	char *platformNameChar = (char *)alloca(platformNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, platformNameSize, platformNameChar, nullptr));
	string platformName = boost::trim_copy(string(platformNameChar));

	size_t deviceNameSize;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &deviceNameSize));
	char *deviceNameChar = (char *)alloca(deviceNameSize * sizeof(char));
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_NAME, deviceNameSize, deviceNameChar, nullptr));
	string deviceName = boost::trim_copy(string(deviceNameChar));
	
	cl_uint deviceUnitsUInt;
	CHECK_OCL_ERROR(clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &deviceUnitsUInt, nullptr));
	string deviceUnits = ToString(deviceUnitsUInt);

	const string kernelName = HashString(ToOptsString(kernelsParameters)) + "-" + HashString(kernelSource) + ".ocl";
	const boost::filesystem::path dirPath = GetCacheDir(appName) / SanitizeFileName(platformName) /
		SanitizeFileName(deviceName) / SanitizeFileName(deviceUnits);
	const boost::filesystem::path filePath = dirPath / kernelName;
	const string fileName = filePath.generic_string();
	
	if (!boost::filesystem::exists(filePath)) {
		// It isn't available, compile the source
		cl_program program = ForcedCompile(context, device,
				kernelsParameters, kernelSource, errorStr);
		if (!program)
			return nullptr;

		// Obtain the binaries of the sources
		size_t binsCount;
		CHECK_OCL_ERROR(clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, 0, nullptr, &binsCount));

		size_t *binsSizes = (size_t *)alloca(binsCount * sizeof(size_t));
		CHECK_OCL_ERROR(clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, binsCount, binsSizes, nullptr));

		// Create the file only if the binaries include something
		if (binsSizes[0] > 0) {
			// Using here alloca() can trigger a stack overflow on Windows for
			// large kernel binaries
			unique_ptr<char> bin(new char[binsSizes[0]]);
			char *bins = bin.get();
			CHECK_OCL_ERROR(clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char *), &bins, nullptr));

			// Add the kernel to the cache
			boost::filesystem::create_directories(dirPath);

			// The use of boost::filesystem::path is required for UNICODE support: fileName
			// is supposed to be UTF-8 encoded.
			boost::filesystem::ofstream file(boost::filesystem::path(fileName),
					boost::filesystem::ofstream::out |
					boost::filesystem::ofstream::binary |
					boost::filesystem::ofstream::trunc);

			// Write the binary hash
			const u_int hashBin = HashBin(bins, binsSizes[0]);
			file.write((char *)&hashBin, sizeof(int));

			file.write(bins, binsSizes[0]);
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

			vector<char> kernelBin(kernelSize);

			// The use of boost::filesystem::path is required for UNICODE support: fileName
			// is supposed to be UTF-8 encoded.
			boost::filesystem::ifstream file(boost::filesystem::path(fileName),
					boost::filesystem::ifstream::in | boost::filesystem::ifstream::binary);

			// Read the binary hash
			u_int hashBin;
			file.read((char *)&hashBin, sizeof(int));

			file.read(&kernelBin[0], kernelSize);

			// Check for errors
			char buf[512];
			if (file.fail()) {
				sprintf(buf, "Unable to read kernel file cache %s", fileName.c_str());
				throw runtime_error(buf);
			}

			file.close();

			// Check the binary hash
			if (hashBin != HashBin(&kernelBin[0], kernelSize)) {
				// Something wrong in the file, remove the file and retry
				boost::filesystem::remove(filePath);
				return Compile(context, device, kernelsParameters, kernelSource, cached, errorStr);
			} else {
				// Compile from the binaries
				vector<const unsigned char *> bins(1);
				bins[0] = (unsigned char *)&kernelBin[0];
				cl_int error;
				cl_program program = clCreateProgramWithBinary(context, 1, &device, &kernelSize, 
						&bins[0],
						nullptr, &error);
				CHECK_OCL_ERROR(error);
				
				error = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
				CHECK_OCL_ERROR(error);

				if (cached)
					*cached = true;

				return program;
			}
		} else {
			// Something wrong in the file, remove the file and retry
			boost::filesystem::remove(filePath);
			return Compile(context, device, kernelsParameters, kernelSource, cached, errorStr);
		}
	}
}

#endif
