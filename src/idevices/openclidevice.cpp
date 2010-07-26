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

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"
#include "luxrays/kernels/kernels.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/accelerators/qbvhaccel.h"
#include "luxrays/accelerators/mqbvhaccel.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

size_t OpenCLIntersectionDevice::RayBufferSize = OPENCL_RAYBUFFER_SIZE;

OpenCLIntersectionDevice::OpenCLIntersectionDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const unsigned int index,
		const unsigned int forceWGSize) :
		HardwareIntersectionDevice(context, DEVICE_TYPE_OPENCL, index) {
	forceWorkGroupSize = forceWGSize;
	deviceDesc = desc;
	deviceName = (desc->GetName() +"Intersect").c_str();
	reportedPermissionError = false;
	qbvhUseImage = false;
	qbvhDisableImageStorage = false;
	intersectionThread = NULL;

	bvhKernel = NULL;
	qbvhKernel = NULL;
	qbvhImageKernel = NULL;
	mqbvhKernel = NULL;
	oclQueue = NULL;

	bvhBuff = NULL;
	vertsBuff = NULL;
	trisBuff = NULL;
	bvhBuff = NULL;

	qbvhBuff = NULL;
	qbvhTrisBuff = NULL;

	qbvhImageBuff = NULL;
	qbvhTrisImageBuff = NULL;

	mqbvhBuff = NULL;
	mqbvhMemMapBuff = NULL;
	mqbvhLeafBuff = NULL;
	mqbvhLeafQuadTrisBuff = NULL;
	mqbvhInvTransBuff = NULL;
	mqbvhTrisOffsetBuff = NULL;

	externalRayBufferQueue = NULL;

	// Allocate the queue for this device
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	cl::Device &oclDevice = deviceDesc->GetOCLDevice();
	oclQueue = new cl::CommandQueue(oclContext, oclDevice);
}

OpenCLIntersectionDevice::~OpenCLIntersectionDevice() {
	if (started)
		Stop();

	FreeDataSetBuffers();

	delete bvhKernel;
	delete qbvhKernel;
	delete qbvhImageKernel;
	delete mqbvhKernel;
	delete oclQueue;
}

void OpenCLIntersectionDevice::SetExternalRayBufferQueue(RayBufferQueue *queue) {
	assert (!started);

	externalRayBufferQueue = queue;
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void OpenCLIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	assert (started);
	assert (!externalRayBufferQueue);

	rayBufferQueue.PushToDo(rayBuffer, 0);
}

RayBuffer *OpenCLIntersectionDevice::PopRayBuffer() {
	assert (started);
	assert (!externalRayBufferQueue);

	return rayBufferQueue.PopDone(0);
}

void OpenCLIntersectionDevice::FreeDataSetBuffers() {
	// Check if I have to free something from previous DataSet
	if (dataSet) {
		deviceDesc->usedMemory -= raysBuff->getInfo<CL_MEM_SIZE>();
		delete raysBuff;
		deviceDesc->usedMemory -= hitsBuff->getInfo<CL_MEM_SIZE>();
		delete hitsBuff;

		if (bvhBuff) {
			deviceDesc->usedMemory -= vertsBuff->getInfo<CL_MEM_SIZE>();
			delete vertsBuff;
			deviceDesc->usedMemory -= trisBuff->getInfo<CL_MEM_SIZE>();
			delete trisBuff;
			deviceDesc->usedMemory -= bvhBuff->getInfo<CL_MEM_SIZE>();
			delete bvhBuff;
		}

		if (qbvhBuff) {
			if (qbvhUseImage) {
				deviceDesc->usedMemory -= qbvhImageBuff->getInfo<CL_MEM_SIZE>();
				delete qbvhImageBuff;
				deviceDesc->usedMemory -= qbvhTrisImageBuff->getInfo<CL_MEM_SIZE>();
				delete qbvhTrisImageBuff;
			} else {
				deviceDesc->usedMemory -= qbvhBuff->getInfo<CL_MEM_SIZE>();
				delete qbvhBuff;
				deviceDesc->usedMemory -= qbvhTrisBuff->getInfo<CL_MEM_SIZE>();
				delete qbvhTrisBuff;
			}
		}

		if (mqbvhBuff) {
			deviceDesc->usedMemory -= mqbvhBuff->getInfo<CL_MEM_SIZE>();
			delete mqbvhBuff;
			deviceDesc->usedMemory -= mqbvhMemMapBuff->getInfo<CL_MEM_SIZE>();
			delete mqbvhMemMapBuff;
			deviceDesc->usedMemory -= mqbvhLeafBuff->getInfo<CL_MEM_SIZE>();
			delete mqbvhLeafBuff;
			deviceDesc->usedMemory -= mqbvhLeafQuadTrisBuff->getInfo<CL_MEM_SIZE>();
			delete mqbvhLeafQuadTrisBuff;
			deviceDesc->usedMemory -= mqbvhInvTransBuff->getInfo<CL_MEM_SIZE>();
			delete mqbvhInvTransBuff;
			deviceDesc->usedMemory -= mqbvhTrisOffsetBuff->getInfo<CL_MEM_SIZE>();
			delete mqbvhTrisOffsetBuff;
		}
	}
}

void OpenCLIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	FreeDataSetBuffers();

	IntersectionDevice::SetDataSet(newDataSet);

	cl::Context &oclContext = deviceDesc->GetOCLContext();

	// Allocate OpenCL buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Ray buffer size: " << (sizeof(Ray) * RayBufferSize / 1024) << "Kbytes");
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY,
			sizeof(Ray) * RayBufferSize);
	deviceDesc->usedMemory += raysBuff->getInfo<CL_MEM_SIZE>();

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Ray hits buffer size: " << (sizeof(RayHit) * RayBufferSize / 1024) << "Kbytes");
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_WRITE_ONLY,
			sizeof(RayHit) * RayBufferSize);
	deviceDesc->usedMemory += hitsBuff->getInfo<CL_MEM_SIZE>();

	cl::Device &oclDevice = deviceDesc->GetOCLDevice();
	switch (dataSet->GetAcceleratorType()) {
		case ACCEL_BVH: {
			//--------------------------------------------------------------------------
			// BVH kernel
			//--------------------------------------------------------------------------

			{
				// Compile sources
				cl::Program::Sources source(1, std::make_pair(KernelSource_BVH.c_str(), KernelSource_BVH.length()));
				cl::Program program = cl::Program(oclContext, source);
				try {
					VECTOR_CLASS<cl::Device> buildDevice;
					buildDevice.push_back(oclDevice);
					program.build(buildDevice, "-I.");
				} catch (cl::Error err) {
					cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compilation error:\n" << strError.c_str());

					throw err;
				}

				bvhKernel = new cl::Kernel(program, "Intersect");
				bvhKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &bvhWorkGroupSize);
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH kernel work group size: " << bvhWorkGroupSize);
				cl_ulong memSize;
				bvhKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH kernel memory footprint: " << memSize);

				bvhKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &bvhWorkGroupSize);
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Suggested work group size: " << bvhWorkGroupSize);

				if (forceWorkGroupSize > 0) {
					bvhWorkGroupSize = forceWorkGroupSize;
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Forced work group size: " << bvhWorkGroupSize);
				}
			}

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Vertices buffer size: " << (sizeof(Point) * dataSet->GetTotalVertexCount() / 1024) << "Kbytes");
			const BVHAccel *bvh = (BVHAccel *)dataSet->accel;
			vertsBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(Point) * dataSet->GetTotalVertexCount(),
					bvh->preprocessedMesh->GetVertices());
			deviceDesc->usedMemory += vertsBuff->getInfo<CL_MEM_SIZE>();

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Triangle indices buffer size: " << (sizeof(Triangle) * dataSet->GetTotalTriangleCount() / 1024) << "Kbytes");
			trisBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(Triangle) * dataSet->GetTotalTriangleCount(),
					bvh->preprocessedMesh->GetTriangles());
			deviceDesc->usedMemory += trisBuff->getInfo<CL_MEM_SIZE>();

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH buffer size: " << (sizeof(BVHAccelArrayNode) * bvh->nNodes / 1024) << "Kbytes");
			bvhBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(BVHAccelArrayNode) * bvh->nNodes,
					bvh->bvhTree);
			deviceDesc->usedMemory += bvhBuff->getInfo<CL_MEM_SIZE>();

			// Set arguments
			bvhKernel->setArg(0, *raysBuff);
			bvhKernel->setArg(1, *hitsBuff);
			bvhKernel->setArg(2, *vertsBuff);
			bvhKernel->setArg(3, *trisBuff);
			bvhKernel->setArg(4, dataSet->GetTotalTriangleCount());
			bvhKernel->setArg(5, bvh->nNodes);
			bvhKernel->setArg(6, *bvhBuff);
			break;
		}
		case ACCEL_QBVH: {
			//--------------------------------------------------------------------------
			// QBVH kernel
			//--------------------------------------------------------------------------

			{
				// Compile sources
				{
					cl::Program::Sources source(1, std::make_pair(KernelSource_QBVH.c_str(), KernelSource_QBVH.length()));
					cl::Program program = cl::Program(oclContext, source);
					try {
						VECTOR_CLASS<cl::Device> buildDevice;
						buildDevice.push_back(oclDevice);
						program.build(buildDevice, "-I.");
					} catch (cl::Error err) {
						cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH compilation error:\n" << strError.c_str());

						throw err;
					}

					qbvhKernel = new cl::Kernel(program, "Intersect");
					qbvhKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &qbvhWorkGroupSize);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH kernel work group size: " << qbvhWorkGroupSize);
					cl_ulong memSize;
					qbvhKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH kernel memory footprint: " << memSize);

					qbvhKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &qbvhWorkGroupSize);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Suggested work group size: " << qbvhWorkGroupSize);

					if (forceWorkGroupSize > 0) {
						qbvhWorkGroupSize = forceWorkGroupSize;
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Forced work group size: " << qbvhWorkGroupSize);
					} else if (qbvhWorkGroupSize > 256) {
						// Otherwise I will probably run out of local memory
						qbvhWorkGroupSize = 256;
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Cap work group size to: " << qbvhWorkGroupSize);
					}
				}

				// Compile QBVH+image storage kernel only if image support is available
				if (deviceDesc->HasImageSupport()) {
					cl::Program::Sources source(1, std::make_pair(KernelSource_QBVH.c_str(), KernelSource_QBVH.length()));
					cl::Program program = cl::Program(oclContext, source);
					try {
						VECTOR_CLASS<cl::Device> buildDevice;
						buildDevice.push_back(oclDevice);
						program.build(buildDevice, "-I. -DUSE_IMAGE_STORAGE");
					} catch (cl::Error err) {
						cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH Image Storage compilation error:\n" << strError.c_str());

						throw err;
					}

					qbvhImageKernel = new cl::Kernel(program, "Intersect");
					qbvhImageKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &qbvhImageWorkGroupSize);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH Image Storage kernel work group size: " << qbvhImageWorkGroupSize);
					cl_ulong memSize;
					qbvhImageKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH Image Storage kernel memory footprint: " << memSize);

					qbvhImageKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &qbvhImageWorkGroupSize);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Suggested work group size: " << qbvhImageWorkGroupSize);

					if (forceWorkGroupSize > 0) {
						qbvhImageWorkGroupSize = forceWorkGroupSize;
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Forced work group size: " << qbvhImageWorkGroupSize);
					} else if (qbvhWorkGroupSize > 256) {
						// Otherwise I will probably run out of local memory
						qbvhWorkGroupSize = 256;
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Cap work group size to: " << qbvhWorkGroupSize);
					}
				}
			}

			const QBVHAccel *qbvh = (QBVHAccel *)dataSet->accel;

			// Calculate the required image size for the storage

			// 7 pixels required for the storage of a QBVH node
			const size_t nodeImagePixelRequired = qbvh->nNodes * 7;
			const size_t nodeImageWidth = Min<size_t>(RoundUp<size_t>(sqrtf(nodeImagePixelRequired), 7),  0x7fff);
			const size_t nodeImageHeight = nodeImagePixelRequired / nodeImageWidth + (((nodeImagePixelRequired % nodeImageWidth) == 0) ? 0 : 1);

			// 10 pixels required for the storage of QBVH Triangles
			const size_t leafPixelRequired = qbvh->nQuads * 10;
			const size_t leafImageWidth = Min<size_t>(RoundUp<size_t>(sqrtf(leafPixelRequired), 10), 32760);
			const size_t leafImageHeight = leafPixelRequired / leafImageWidth + (((leafPixelRequired % leafImageWidth) == 0) ? 0 : 1);

			// Check if I can use image to store the data set
			if (qbvhDisableImageStorage) {
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Disable forced for QBVH scene storage inside image");
				qbvhUseImage = false;
			} else {
				if (deviceDesc->HasImageSupport()) {
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] OpenCL image support is available");

					// Check if the scene is small enough to be stored inside an image
					const size_t maxWidth = deviceDesc->GetImage2DMaxWidth();
					const size_t maxHeight = deviceDesc->GetImage2DMaxHeight();
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] OpenCL max. image buffer size: " << maxWidth << "x" << maxHeight);

					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH node image buffer size: " << nodeImageWidth << "x" << nodeImageHeight);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH triangle image buffer size: " << leafImageWidth << "x" << leafImageHeight);

					if ((nodeImageWidth > maxWidth) ||
							(nodeImageHeight > maxHeight) ||
							(leafImageWidth > maxWidth) ||
							(leafImageHeight > maxHeight)) {
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] OpenCL image max. image size supported is too small");
						qbvhUseImage = false;
					} else {
						LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Enabled QBVH scene storage inside image");
						qbvhUseImage = true;
					}
				} else {
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] OpenCL image support is not available");
					qbvhUseImage = false;
				}
			}

			if (qbvhUseImage) {
				assert (nodeImageWidth < 0x7fff * 7);
				assert (nodeImageHeight < 0xffff);

				assert (leafImageWidth < 0x7ff * 10);
				assert (leafImageHeight < 0xffff);

				{
					// Convert node indices to image coordinates
					unsigned int *inodes = new unsigned int[nodeImageWidth * nodeImageHeight * 7 * 4];
					for (size_t i = 0; i < qbvh->nNodes; ++i) {
						unsigned int *pnodes = (unsigned int *)&qbvh->nodes[i];
						const size_t offset = i * 7 * 4;

						for (size_t j = 0; j < 6 * 4; ++j)
							inodes[offset + j] = pnodes[j];

						for (size_t j = 0; j < 4; ++j) {
							int index = qbvh->nodes[i].children[j];

							 if (QBVHNode::IsEmpty(index)) {
								inodes[offset + 6 * 4 + j] = index;
							} else if (QBVHNode::IsLeaf(index)) {
								int32_t count = QBVHNode::FirstQuadIndex(index) * 10;
								// "/ 10" in order to not waste bits
								const unsigned short x = (count % leafImageWidth) / 10;
								const unsigned short y = count / leafImageWidth;
								((int32_t *)inodes)[offset + 6 * 4 + j] =  0x80000000 |
										(((static_cast<int32_t>(QBVHNode::NbQuadPrimitives(index)) - 1) & 0xf) << 27) |
										(static_cast<int32_t>((x << 16) | y) & 0x07ffffff);
							} else {
								index *= 7;
								// "/ 7" in order to not waste bits
								const unsigned short x = (index % nodeImageWidth) / 7;
								const unsigned short y = index / nodeImageWidth;
								inodes[offset + 6 * 4 + j] = (x << 16) | y;
							}
						}
					}

					qbvhImageBuff = new cl::Image2D(oclContext,
							CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
							cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT32), nodeImageWidth, nodeImageHeight, 0, inodes);
					deviceDesc->usedMemory += qbvhImageBuff->getInfo<CL_MEM_SIZE>();

					delete inodes;
				}

				{
					unsigned int *iprims = new unsigned int[leafImageWidth * leafImageHeight * 10 * 4];
					memcpy(iprims, qbvh->prims, sizeof(QuadTriangle) * qbvh->nQuads);

					qbvhTrisImageBuff = new cl::Image2D(oclContext,
							CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
							cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT32), leafImageWidth, leafImageHeight, 0, iprims);
					deviceDesc->usedMemory += qbvhTrisImageBuff->getInfo<CL_MEM_SIZE>();

					delete iprims;
				}

				// Set arguments
				qbvhImageKernel->setArg(0, *raysBuff);
				qbvhImageKernel->setArg(1, *hitsBuff);
				qbvhImageKernel->setArg(2, *qbvhImageBuff);
				qbvhImageKernel->setArg(3, *qbvhTrisImageBuff);
				qbvhImageKernel->setArg(5, 24 * qbvhImageWorkGroupSize * sizeof(cl_int), NULL);
			} else {
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH buffer size: " << (sizeof(QBVHNode) * qbvh->nNodes / 1024) << "Kbytes");
				qbvhBuff = new cl::Buffer(oclContext,
						CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
						sizeof(QBVHNode) * qbvh->nNodes,
						qbvh->nodes);
				deviceDesc->usedMemory += qbvhBuff->getInfo<CL_MEM_SIZE>();

				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QuadTriangle buffer size: " << (sizeof(QuadTriangle) * qbvh->nQuads / 1024) << "Kbytes");
				qbvhTrisBuff = new cl::Buffer(oclContext,
						CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
						sizeof(QuadTriangle) * qbvh->nQuads,
						qbvh->prims);
				deviceDesc->usedMemory += qbvhTrisBuff->getInfo<CL_MEM_SIZE>();

				// Set arguments
				qbvhKernel->setArg(0, *raysBuff);
				qbvhKernel->setArg(1, *hitsBuff);
				qbvhKernel->setArg(2, *qbvhBuff);
				qbvhKernel->setArg(3, *qbvhTrisBuff);
				qbvhKernel->setArg(5, 24 * qbvhWorkGroupSize * sizeof(cl_int), NULL);
			}
			break;
		}
		case ACCEL_MQBVH: {
			//--------------------------------------------------------------------------
			// MQBVH kernel
			//--------------------------------------------------------------------------

			// Compile sources
			{
				cl::Program::Sources source(1, std::make_pair(KernelSource_MQBVH.c_str(), KernelSource_MQBVH.length()));
				cl::Program program = cl::Program(oclContext, source);
				try {
					VECTOR_CLASS<cl::Device> buildDevice;
					buildDevice.push_back(oclDevice);
					program.build(buildDevice, "-I.");
				} catch (cl::Error err) {
					cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH compilation error:\n" << strError.c_str());

					throw err;
				}

				mqbvhKernel = new cl::Kernel(program, "Intersect");
				mqbvhKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &mqbvhWorkGroupSize);
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH kernel work group size: " << mqbvhWorkGroupSize);
				cl_ulong memSize;
				mqbvhKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH kernel memory footprint: " << memSize);

				mqbvhKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &mqbvhWorkGroupSize);
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Suggested work group size: " << mqbvhWorkGroupSize);

				if (forceWorkGroupSize > 0) {
					mqbvhWorkGroupSize = forceWorkGroupSize;
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Forced work group size: " << mqbvhWorkGroupSize);
				} else if (qbvhWorkGroupSize > 256) {
					// Otherwise I will probably run out of local memory
					qbvhWorkGroupSize = 256;
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Cap work group size to: " << qbvhWorkGroupSize);
				}
			}

			// TODO: remove the following limitation
			// NOTE: this code is somewhat limited to 32bit address space of the OpenCL device
			const MQBVHAccel *mqbvh = (MQBVHAccel *)dataSet->accel;

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH buffer size: " << (sizeof(QBVHNode) * mqbvh->nNodes / 1024) << "Kbytes");
			mqbvhBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(QBVHNode) * mqbvh->nNodes,
					mqbvh->nodes);
			deviceDesc->usedMemory += mqbvhBuff->getInfo<CL_MEM_SIZE>();

			// Calculate the size of memory to allocate
			unsigned int totalNodesCount = 0;
			unsigned int totalQuadTrisCount = 0;

			std::map<const QBVHAccel *, unsigned int> indexNodesMap;
			std::map<const QBVHAccel *, unsigned int> indexQuadTrisMap;

			for (std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::const_iterator it = mqbvh->accels.begin(); it != mqbvh->accels.end(); it++) {
				const QBVHAccel *qbvh = it->second;

				indexNodesMap[qbvh] = totalNodesCount;
				totalNodesCount += qbvh->nNodes;
				indexQuadTrisMap[qbvh] = totalQuadTrisCount;
				totalQuadTrisCount += qbvh->nQuads;
			}

			unsigned int *memMap = new unsigned int[mqbvh->nLeafs * 2];
			for (unsigned int i = 0; i < mqbvh->nLeafs; ++i) {
				memMap[i * 2] = indexNodesMap[mqbvh->leafs[i]];
				memMap[i * 2 + 1] = indexQuadTrisMap[mqbvh->leafs[i]];
			}

			// Allocate memory for QBVH Leafs
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH leaf Nodes buffer size: " << (totalNodesCount * sizeof(QBVHNode) / 1024) << "Kbytes");
			mqbvhLeafBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY,
					totalNodesCount * sizeof(QBVHNode));
			deviceDesc->usedMemory += mqbvhLeafBuff->getInfo<CL_MEM_SIZE>();

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH QuadTriangle buffer size: " << (totalQuadTrisCount * sizeof(QuadTriangle) / 1024) << "Kbytes");
			mqbvhLeafQuadTrisBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY,
					totalQuadTrisCount * sizeof(QuadTriangle));
			deviceDesc->usedMemory += mqbvhLeafQuadTrisBuff->getInfo<CL_MEM_SIZE>();

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH memory map buffer size: " << (mqbvh->nLeafs * sizeof(unsigned int) * 2 / 1024) << "Kbytes");
			mqbvhMemMapBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					mqbvh->nLeafs * sizeof(unsigned int) * 2,
					memMap);
			deviceDesc->usedMemory += mqbvhMemMapBuff->getInfo<CL_MEM_SIZE>();
			delete memMap;

			// Upload QBVH leafs
			size_t nodesMemOffset = 0;
			size_t quadTrisMemOffset = 0;
			for (std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::const_iterator it = mqbvh->accels.begin(); it != mqbvh->accels.end(); it++) {
				const QBVHAccel *qbvh = it->second;

				const size_t nodesMemSize = sizeof(QBVHNode) * qbvh->nNodes;
				oclQueue->enqueueWriteBuffer(
						*mqbvhLeafBuff,
						CL_FALSE,
						nodesMemOffset,
						nodesMemSize,
						qbvh->nodes);
				nodesMemOffset += nodesMemSize;

				const size_t quadTrisMemSize = sizeof(QuadTriangle) * qbvh->nQuads;
				oclQueue->enqueueWriteBuffer(
						*mqbvhLeafQuadTrisBuff,
						CL_FALSE,
						quadTrisMemOffset,
						quadTrisMemSize,
						qbvh->prims);
				quadTrisMemOffset += quadTrisMemSize;
			}

			// Upload QBVH leafs transformations
			Matrix4x4 *invTrans = new Matrix4x4[mqbvh->nLeafs];
			for (unsigned int i = 0; i < mqbvh->nLeafs; ++i) {
				if (mqbvh->leafsInvTransform[i])
					invTrans[i] = mqbvh->leafsInvTransform[i]->GetMatrix();
				else
					invTrans[i] = Matrix4x4();
			}

			const size_t invTransMemSize = mqbvh->nLeafs * sizeof(Matrix4x4);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH inverse transformations buffer size: " << (invTransMemSize / 1024) << "Kbytes");
			mqbvhInvTransBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					invTransMemSize,
					invTrans);
			deviceDesc->usedMemory += mqbvhInvTransBuff->getInfo<CL_MEM_SIZE>();
			delete invTrans;

			// Upload primitive offsets
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MQBVH primitive offsets buffer size: " << (sizeof(unsigned int) * mqbvh->nLeafs / 1024) << "Kbytes");
			mqbvhTrisOffsetBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(unsigned int) * mqbvh->nLeafs,
					mqbvh->leafsOffset);
			deviceDesc->usedMemory += mqbvhTrisOffsetBuff->getInfo<CL_MEM_SIZE>();

			// Set arguments
			mqbvhKernel->setArg(0, *raysBuff);
			mqbvhKernel->setArg(1, *hitsBuff);
			mqbvhKernel->setArg(2, *mqbvhBuff);
			mqbvhKernel->setArg(4, *mqbvhMemMapBuff);
			mqbvhKernel->setArg(5, *mqbvhLeafBuff);
			mqbvhKernel->setArg(6, *mqbvhLeafQuadTrisBuff);
			mqbvhKernel->setArg(7, *mqbvhInvTransBuff);
			mqbvhKernel->setArg(8, *mqbvhTrisOffsetBuff);
			break;
		}
		default:
			assert (false);
	}
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	// Create the thread for the rendering
	intersectionThread = new boost::thread(boost::bind(OpenCLIntersectionDevice::IntersectionThread, this));

	// Set intersectionThread priority
	bool res = SetThreadRRPriority(intersectionThread);
	if (res && !reportedPermissionError) {
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)");
		reportedPermissionError = true;
	}
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

	if (!externalRayBufferQueue)
		rayBufferQueue.Clear();
}

void OpenCLIntersectionDevice::TraceRayBuffer(RayBuffer *rayBuffer, cl::Event *event) {
	// Upload the rays to the GPU
	oclQueue->enqueueWriteBuffer(
			*raysBuff,
			CL_FALSE,
			0,
			sizeof(Ray) * rayBuffer->GetRayCount(),
			rayBuffer->GetRayBuffer());

	switch (dataSet->GetAcceleratorType()) {
		case ACCEL_BVH: {
			bvhKernel->setArg(7, (unsigned int)rayBuffer->GetRayCount());
			oclQueue->enqueueNDRangeKernel(*bvhKernel, cl::NullRange,
				cl::NDRange(rayBuffer->GetSize()), cl::NDRange(bvhWorkGroupSize));
			break;
		}
		case ACCEL_QBVH: {
			if (qbvhUseImage) {
				qbvhImageKernel->setArg(4, (unsigned int)rayBuffer->GetRayCount());
				oclQueue->enqueueNDRangeKernel(*qbvhImageKernel, cl::NullRange,
					cl::NDRange(rayBuffer->GetSize()), cl::NDRange(qbvhImageWorkGroupSize));
			} else {
				qbvhKernel->setArg(4, (unsigned int)rayBuffer->GetRayCount());
				oclQueue->enqueueNDRangeKernel(*qbvhKernel, cl::NullRange,
					cl::NDRange(rayBuffer->GetSize()), cl::NDRange(qbvhWorkGroupSize));
			}
			break;
		}
		case ACCEL_MQBVH: {
			mqbvhKernel->setArg(3, (unsigned int)rayBuffer->GetRayCount());
			oclQueue->enqueueNDRangeKernel(*mqbvhKernel, cl::NullRange,
				cl::NDRange(rayBuffer->GetSize()), cl::NDRange(mqbvhWorkGroupSize));
			break;
		}
		default:
			assert (false);
	}

	// Download the results
	oclQueue->enqueueReadBuffer(
			*hitsBuff,
			CL_FALSE,
			0,
			sizeof(RayHit) * rayBuffer->GetRayCount(),
			rayBuffer->GetHitBuffer(), NULL, event);
}

void OpenCLIntersectionDevice::IntersectionThread(OpenCLIntersectionDevice *renderDevice) {
	LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread started");

	try {
		RayBufferQueue *queue = renderDevice->externalRayBufferQueue ?
			renderDevice->externalRayBufferQueue : &(renderDevice->rayBufferQueue);

		RayBuffer *rayBuffer0, *rayBuffer1, *rayBuffer2;
		const double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			queue->Pop3xToDo(&rayBuffer0, &rayBuffer1, &rayBuffer2);
			renderDevice->statsDeviceIdleTime += WallClockTime() - t1;
			const unsigned int count = (rayBuffer0 ? 1 : 0) + (rayBuffer1 ? 1 : 0) + (rayBuffer2 ? 1 : 0);

			switch(count) {
				case 1: {
					// Only one ray buffer to trace available
					cl::Event event;
					renderDevice->TraceRayBuffer(rayBuffer0, &event);

					event.wait();
					renderDevice->statsTotalRayCount += rayBuffer0->GetRayCount();
					queue->PushDone(rayBuffer0);

					renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
					break;
				}
				case 2: {
					// At least 2 ray buffers to trace

					// Trace 0 ray buffer
					cl::Event event0;
					renderDevice->TraceRayBuffer(rayBuffer0, &event0);

					// Trace 1 ray buffer
					cl::Event event1;
					renderDevice->TraceRayBuffer(rayBuffer1, &event1);

					// Pop 0 ray buffer
					event0.wait();
					renderDevice->statsTotalRayCount += rayBuffer0->GetRayCount();
					queue->PushDone(rayBuffer0);

					// Pop 1 ray buffer
					event1.wait();
					renderDevice->statsTotalRayCount += rayBuffer1->GetRayCount();
					queue->PushDone(rayBuffer1);

					renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
					break;
				}
				case 3: {
					// At least 3 ray buffers to trace

					// Trace 0 ray buffer
					cl::Event event0;
					renderDevice->TraceRayBuffer(rayBuffer0, &event0);

					// Trace 1 ray buffer
					cl::Event event1;
					renderDevice->TraceRayBuffer(rayBuffer1, &event1);

					// Trace 2 ray buffer
					cl::Event event2;
					renderDevice->TraceRayBuffer(rayBuffer2, &event2);

					// Pop 0 ray buffer
					event0.wait();
					renderDevice->statsTotalRayCount += rayBuffer0->GetRayCount();
					queue->PushDone(rayBuffer0);

					// Pop 1 ray buffer
					event1.wait();
					renderDevice->statsTotalRayCount += rayBuffer1->GetRayCount();
					queue->PushDone(rayBuffer1);

					// Pop 2 ray buffer
					event2.wait();
					renderDevice->statsTotalRayCount += rayBuffer2->GetRayCount();
					queue->PushDone(rayBuffer2);

					renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
					break;
				}
				default:
					assert (false);
			}
		}

		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (cl::Error err) {
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
}

#endif
