/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <cstdio>

#include "luxrays/core/device.h"
#include "luxrays/core/context.h"
#include "luxrays/kernels/kernels.h"

#if defined (__linux__)
#include <pthread.h>
#endif

namespace luxrays {

IntersectionDevice::IntersectionDevice(const Context *context, const DeviceType type, const unsigned int index) :
	deviceContext(context), deviceType(type) {
	deviceIndex = index;
	started = false;
	dataSet = NULL;
}

IntersectionDevice::~IntersectionDevice() {
}

void IntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	assert (!started);

	dataSet = newDataSet;
}

void IntersectionDevice::Start() {
	assert (!started);

	statsTotalRayTime = 0.0;
	statsTotalRayCount = 0.0;
	statsDeviceIdleTime = 0.0;
	statsDeviceTotalTime = 0.0;
	started = true;
}

void IntersectionDevice::Stop() {
	assert (started);
	started = false;
}

void IntersectionDevice::Filter(const DeviceType type,
		std::vector<DeviceDescription *> &deviceDescriptions) {
	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		if ((type != DEVICE_TYPE_ALL) && (deviceDescriptions[i]->type != type)) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else
			++i;
	}
}

std::string IntersectionDevice::GetDeviceType(const DeviceType type) {
	switch (type) {
		case DEVICE_TYPE_ALL:
			return "ALL";
		case DEVICE_TYPE_NATIVE_THREAD:
			return "NATIVE_THREAD";
		case DEVICE_TYPE_OPENCL:
			return "OPENCL";
		case DEVICE_TYPE_VIRTUAL:
			return "VIRTUAL";
		default:
			return "UNKNOWN";
	}
}

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

size_t NativeThreadIntersectionDevice::RayBufferSize = 512;

NativeThreadIntersectionDevice::NativeThreadIntersectionDevice(const Context *context,
		const size_t threadIndex, const unsigned int devIndex) :
			IntersectionDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
	char buf[64];
	sprintf(buf, "NativeThread-%03d", (int)threadIndex);
	deviceName = std::string(buf);

	intersectionThread = NULL;
}

NativeThreadIntersectionDevice::~NativeThreadIntersectionDevice() {
	if (started)
		Stop();
}

void NativeThreadIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);
}

void NativeThreadIntersectionDevice::Start() {
	IntersectionDevice::Start();

	// Create the thread for the rendering
	intersectionThread = new boost::thread(boost::bind(NativeThreadIntersectionDevice::IntersectionThread, this));

	// Set thread priority
#if defined (__linux__) // || defined (__APPLE__) Note: SCHED_BATCH is not supported on MacOS
	{
		const pthread_t tid = (pthread_t) intersectionThread->native_handle();

		int policy = SCHED_BATCH;
		int sysMinPriority = sched_get_priority_min(policy);

		struct sched_param param;
		param.sched_priority = sysMinPriority;

		int ret = pthread_setschedparam(tid, policy, &param);
		if (ret)
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set thread priority, error: " << ret);
	}
#elif defined (WIN32)
	{
		const HANDLE tid = (HANDLE) renderThread->native_handle();
		if (!SetThreadPriority(tid, THREAD_PRIORITY_LOWEST))
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set thread priority, error: " << ret);
	}
#endif
}

void NativeThreadIntersectionDevice::Interrupt() {
	assert (started);
	intersectionThread->interrupt();
}

void NativeThreadIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	intersectionThread->interrupt();
	intersectionThread->join();
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void NativeThreadIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	todoRayBufferQueue.Push(rayBuffer);
}

RayBuffer *NativeThreadIntersectionDevice::PopRayBuffer() {
	return doneRayBufferQueue.Pop();
}

void NativeThreadIntersectionDevice::IntersectionThread(NativeThreadIntersectionDevice *renderDevice) {
	LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread started");

	try {
		double lastStatPrint = WallClockTime();

		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			RayBuffer *rayBuffer = renderDevice->todoRayBufferQueue.Pop();
			const double t2 = WallClockTime();

			// Trace rays
			const Ray *rb = rayBuffer->GetRayBuffer();
			RayHit *hb = rayBuffer->GetHitBuffer();
			const DataSet *dataSet = renderDevice->dataSet;
			for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i)
				dataSet->Intersect(&rb[i], &hb[i]);

			const double t3 = WallClockTime();
			const double rayCount = rayBuffer->GetRayCount();
			renderDevice->doneRayBufferQueue.Push(rayBuffer);
			const double t4 = WallClockTime();

			renderDevice->statsTotalRayTime += t3 - t2;
			renderDevice->statsTotalRayCount += rayCount;
			renderDevice->statsDeviceIdleTime += t2 - t1;
			renderDevice->statsDeviceTotalTime += t4 - t1;

			if (t4 - lastStatPrint > 5.0) {
				// Print some statistic
				char buf[128];
				sprintf(buf, "[Rays/sec %dK][Load %.1f%%]",
					int(renderDevice->statsTotalRayCount / (1000.0 * renderDevice->statsTotalRayTime)),
					100.0 * renderDevice->statsTotalRayTime / renderDevice->statsDeviceTotalTime);
				LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "]" << buf);
				lastStatPrint = t4;
			}
		}

		LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (cl::Error err) {
		LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
}

void NativeThreadIntersectionDevice::AddDevices(std::vector<DeviceDescription *> &descriptions) {
	unsigned int count = boost::thread::hardware_concurrency();

	// Build the descriptions
	char buf[64];
	for (size_t i = 0; i < count; ++i) {
		sprintf(buf, "NativeThread-%03d", (int)i);
		std::string deviceName = std::string(buf);

		descriptions.push_back(new NativeThreadDeviceDescription(deviceName, i));
	}
}

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

size_t OpenCLIntersectionDevice::RayBufferSize = 8 * 1024;

OpenCLIntersectionDevice::OpenCLIntersectionDevice(const Context *context, const cl::Device &device,
		const unsigned int index) :	IntersectionDevice(context, DEVICE_TYPE_OPENCL, index) {
	deviceName = device.getInfo<CL_DEVICE_NAME > ().c_str();
	reportedPermissionError = false;
	intersectionThread = NULL;
	oclType = GetOCLDeviceType(device.getInfo<CL_DEVICE_TYPE >());

	// Allocate a context with the selected device
	VECTOR_CLASS<cl::Device> devices;
	devices.push_back(device);
	cl_context_properties cps[3] = {
		CL_CONTEXT_PLATFORM, (cl_context_properties)context->oclPlatform(), 0
	};

	oclContext = new cl::Context(devices, cps);

	// Allocate the queue for this device
	oclQueue = new cl::CommandQueue(*oclContext, device);

	//--------------------------------------------------------------------------
	// BVH kernel
	//--------------------------------------------------------------------------

	// Compile sources
	cl::Program::Sources source(1, std::make_pair(KernelSource_BVH.c_str(), KernelSource_BVH.length()));
	cl::Program program = cl::Program(*oclContext, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(device);
		program.build(buildDevice, "-I.");
	} catch (cl::Error err) {
		cl::string strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Compilation error:\n" << strError.c_str());

		throw err;
	}

	bvhKernel = new cl::Kernel(program, "Intersect");
	bvhKernel->getWorkGroupInfo<size_t>(device, CL_KERNEL_WORK_GROUP_SIZE, &bvhWorkGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH kernel work group size: " << bvhWorkGroupSize);
	cl_ulong memSize;
	bvhKernel->getWorkGroupInfo<cl_ulong>(device, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH kernel memory footprint: " << memSize);

	bvhKernel->getWorkGroupInfo<size_t>(device, CL_KERNEL_WORK_GROUP_SIZE, &bvhWorkGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << bvhWorkGroupSize);
	// TODO: hard code the workgroup size to 64
	bvhWorkGroupSize = 64;
}

OpenCLIntersectionDevice::~OpenCLIntersectionDevice() {
	if (started)
		Stop();

	if (dataSet) {
		delete raysBuff;
		delete hitsBuff;
		delete vertsBuff;
		delete trisBuff;
		delete bvhBuff;
	}

	delete bvhKernel;
	delete oclQueue;
	delete oclContext;
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void OpenCLIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	todoRayBufferQueue.Push(rayBuffer);
}

RayBuffer *OpenCLIntersectionDevice::PopRayBuffer() {
	return doneRayBufferQueue.Pop();
}

void OpenCLIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	// Check if I have to free something from previous DataSet
	if (dataSet) {
		delete raysBuff;
		delete hitsBuff;
		delete vertsBuff;
		delete trisBuff;
		delete bvhBuff;
	}

	IntersectionDevice::SetDataSet(newDataSet);

	// Allocate OpenCL buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Rays buffer size: " << (sizeof(Ray) * RayBufferSize / 1024) << "Kbytes");
	raysBuff = new cl::Buffer(*oclContext,
			CL_MEM_READ_ONLY,
			sizeof(Ray) * RayBufferSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Ray hits buffer size: " << (sizeof(RayHit) * RayBufferSize / 1024) << "Kbytes");
	hitsBuff = new cl::Buffer(*oclContext,
			CL_MEM_WRITE_ONLY,
			sizeof(RayHit) * RayBufferSize);

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Vertices buffer size: " << (sizeof(Point) * dataSet->GetTotalVertexCount() / 1024) << "Kbytes");
	vertsBuff = new cl::Buffer(*oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(Point) * dataSet->GetTotalVertexCount(),
			dataSet->GetTriangleMesh()->GetVertices());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Triangle indices buffer size: " << (sizeof(Triangle) * dataSet->GetTotalTriangleCount() / 1024) << "Kbytes");
	trisBuff = new cl::Buffer(*oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(Triangle) * dataSet->GetTotalTriangleCount(),
			dataSet->GetTriangleMesh()->GetTriangles());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " BVH buffer size: " << (sizeof(BVHAccelArrayNode) * dataSet->GetAccelerator()->nNodes / 1024) << "Kbytes");
	bvhBuff = new cl::Buffer(*oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(BVHAccelArrayNode) * dataSet->GetAccelerator()->nNodes,
			dataSet->GetAccelerator()->bvhTree);

	// Set Arguments
	bvhKernel->setArg(0, *raysBuff);
	bvhKernel->setArg(1, *hitsBuff);
	bvhKernel->setArg(2, *vertsBuff);
	bvhKernel->setArg(3, *trisBuff);
	bvhKernel->setArg(4, dataSet->GetTotalTriangleCount());
	bvhKernel->setArg(5, dataSet->GetAccelerator()->nNodes);
	bvhKernel->setArg(6, *bvhBuff);
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	// Create the thread for the rendering
	intersectionThread = new boost::thread(boost::bind(OpenCLIntersectionDevice::IntersectionThread, this));

	// Set rayIntersectionThread priority
#if defined (__linux__) || defined (__APPLE__)
	{
		const pthread_t tid = (pthread_t) intersectionThread->native_handle();

		int policy = SCHED_FIFO;
		int sysMaxPriority = sched_get_priority_max(policy);

		struct sched_param param;
		param.sched_priority = sysMaxPriority;

		int ret = pthread_setschedparam(tid, policy, &param);
		if (ret && !reportedPermissionError) {
			/*LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set ray intersection thread priority, error: " <<
					ret << " (you probably need root/administrator permission to set thread SCHED_FIFO priority)");*/
		}
	}
#elif defined (WIN32)
	{
		const HANDLE tid = (HANDLE) renderThread->native_handle();
		if (!SetThreadPriority(tid, THREAD_PRIORITY_HIGHEST))
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to ray intersection thread priority" << endl;
	}
#endif
}

void OpenCLIntersectionDevice::Interrupt() {
	assert (started);
	intersectionThread->interrupt();
}

void OpenCLIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	intersectionThread->interrupt();
	intersectionThread->join();
	delete intersectionThread;
	intersectionThread = NULL;

	todoRayBufferQueue.Clear();
	doneRayBufferQueue.Clear();
}

void OpenCLIntersectionDevice::TraceRayBuffer(RayBuffer *rayBuffer) {
	// Download the rays to the GPU
	cl::Event writeBufferEvent;
	oclQueue->enqueueWriteBuffer(
			*raysBuff,
			CL_FALSE,
			0,
			sizeof(Ray) * rayBuffer->GetRayCount(),
			rayBuffer->GetRayBuffer(), NULL, &writeBufferEvent);

	bvhKernel->setArg(7, (unsigned int)rayBuffer->GetRayCount());
	cl::Event kernelEvent;
	VECTOR_CLASS<cl::Event> kernelWaitEvents(1, writeBufferEvent);
	oclQueue->enqueueNDRangeKernel(*bvhKernel, cl::NullRange,
		cl::NDRange(rayBuffer->GetSize()), cl::NDRange(bvhWorkGroupSize),
		&kernelWaitEvents, &kernelEvent);

	// Upload the results
	cl::Event readBufferEvent;
	VECTOR_CLASS<cl::Event> readBufferWaitEvents(1, kernelEvent);
	oclQueue->enqueueReadBuffer(
			*hitsBuff,
			CL_TRUE,
			0,
			sizeof(RayHit) * rayBuffer->GetRayCount(),
			rayBuffer->GetHitBuffer(),
			&readBufferWaitEvents, &readBufferEvent);
}

void OpenCLIntersectionDevice::IntersectionThread(OpenCLIntersectionDevice *renderDevice) {
	LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread started");

	try {
		double lastStatPrint = WallClockTime();

		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			RayBuffer *rayBuffer = renderDevice->todoRayBufferQueue.Pop();
			const double t2 = WallClockTime();
			renderDevice->TraceRayBuffer(rayBuffer);
			const double t3 = WallClockTime();

			const double rayCount = rayBuffer->GetRayCount();

			renderDevice->doneRayBufferQueue.Push(rayBuffer);
			const double t4 = WallClockTime();

			renderDevice->statsTotalRayTime += t3 - t2;
			renderDevice->statsTotalRayCount += rayCount;
			renderDevice->statsDeviceIdleTime += t2 - t1;
			renderDevice->statsDeviceTotalTime += t4 - t1;

			if (t4 - lastStatPrint > 5.0) {
				// Print some statistic
				char buf[128];
				sprintf(buf, "[Rays/sec %dK][Load %.1f%%]",
					int(renderDevice->statsTotalRayCount / (1000.0 * renderDevice->statsTotalRayTime)),
					100.0 * renderDevice->statsTotalRayTime / renderDevice->statsDeviceTotalTime);
				LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "]" << buf);
				lastStatPrint = t4;
			}
		}

		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (cl::Error err) {
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
}

std::string OpenCLIntersectionDevice::GetDeviceType(const cl_int type) {
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return "TYPE_ALL";
		case CL_DEVICE_TYPE_DEFAULT:
			return "TYPE_DEFAULT";
		case CL_DEVICE_TYPE_CPU:
			return "TYPE_CPU";
		case CL_DEVICE_TYPE_GPU:
			return "TYPE_GPU";
		default:
			return "TYPE_UNKNOWN";
	}
}

std::string OpenCLIntersectionDevice::GetDeviceType(const OpenCLDeviceType type) {
	switch (type) {
		case OCL_DEVICE_TYPE_ALL:
			return "ALL";
		case OCL_DEVICE_TYPE_DEFAULT:
			return "DEFAULT";
		case OCL_DEVICE_TYPE_CPU:
			return "CPU";
		case OCL_DEVICE_TYPE_GPU:
			return "GPU";
		default:
			return "UNKNOWN";
	}
}

OpenCLDeviceType OpenCLIntersectionDevice::GetOCLDeviceType(const cl_int type) {
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return OCL_DEVICE_TYPE_ALL;
		case CL_DEVICE_TYPE_DEFAULT:
			return OCL_DEVICE_TYPE_DEFAULT;
		case CL_DEVICE_TYPE_CPU:
			return OCL_DEVICE_TYPE_CPU;
		case CL_DEVICE_TYPE_GPU:
			return OCL_DEVICE_TYPE_GPU;
		default:
			return OCL_DEVICE_TYPE_UNKNOWN;
	}
}

void OpenCLIntersectionDevice::AddDevices(
	const cl::Platform &oclPlatform, const OpenCLDeviceType filter,
	std::vector<DeviceDescription *> &descriptions) {
	// Get the list of devices available on the platform
	VECTOR_CLASS<cl::Device> oclDevices;
	oclPlatform.getDevices(CL_DEVICE_TYPE_ALL, &oclDevices);

	// Build the descriptions
	for (size_t i = 0; i < oclDevices.size(); ++i) {
		OpenCLDeviceType type = GetOCLDeviceType(oclDevices[i].getInfo<CL_DEVICE_TYPE >());

		if ((filter == OCL_DEVICE_TYPE_ALL) || (filter == type)) {
			OpenCLDeviceDescription *desc = new OpenCLDeviceDescription(
				oclDevices[i].getInfo<CL_DEVICE_NAME >().c_str(), type, i,
				oclDevices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>(),
				oclDevices[i].getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>());
			descriptions.push_back(desc);
		}
	}
}

void OpenCLIntersectionDevice::Filter(const OpenCLDeviceType type,
		std::vector<DeviceDescription *> &deviceDescriptions) {
	if (type == OCL_DEVICE_TYPE_ALL)
		return;

	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		if ((deviceDescriptions[i]->type == DEVICE_TYPE_OPENCL) &&
				(((OpenCLDeviceDescription *)deviceDescriptions[i])->oclType != type)) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else
			++i;
	}
}

}
