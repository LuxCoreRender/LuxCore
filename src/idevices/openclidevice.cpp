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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"
#include "luxrays/kernels/kernels.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/accelerators/qbvhaccel.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/utils/ocl/utils.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL intersection kernels
//------------------------------------------------------------------------------

void OpenCLBVHKernel::FreeBuffers()
{
	delete kernel;
	kernel = NULL;
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	deviceDesc->FreeMemory(vertsBuff->getInfo<CL_MEM_SIZE>());
	delete vertsBuff;
	vertsBuff = NULL;
	deviceDesc->FreeMemory(trisBuff->getInfo<CL_MEM_SIZE>());
	delete trisBuff;
	trisBuff = NULL;
	deviceDesc->FreeMemory(bvhBuff->getInfo<CL_MEM_SIZE>());
	delete bvhBuff;
	bvhBuff = NULL;
}

void OpenCLBVHKernel::SetDataSet(const DataSet *newDataSet)
{
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	cl::Device &oclDevice = device->GetOpenCLDevice();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	// Compile sources
	std::string code(
		_LUXRAYS_POINT_OCLDEFINE
		_LUXRAYS_VECTOR_OCLDEFINE
		_LUXRAYS_RAY_OCLDEFINE
		_LUXRAYS_RAYHIT_OCLDEFINE
		_LUXRAYS_TRIANGLE_OCLDEFINE
		_LUXRAYS_BBOX_OCLDEFINE);
	code += KernelSource_BVH;
	cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
	cl::Program program = cl::Program(oclContext, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(oclDevice);
		program.build(buildDevice);
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compilation error:\n" << strError.c_str());

		throw err;
	}

	delete kernel;
	kernel = new cl::Kernel(program, "Intersect");
	kernel->getWorkGroupInfo<size_t>(oclDevice,
		CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] BVH kernel work group size: " << workGroupSize);

	kernel->getWorkGroupInfo<size_t>(oclDevice,
		CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Suggested work group size: " << workGroupSize);

	if (device->GetForceWorkGroupSize() > 0) {
		workGroupSize = device->GetForceWorkGroupSize();
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Forced work group size: " << workGroupSize);
	}

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Vertices buffer size: " <<
		(sizeof(Point) * newDataSet->GetTotalVertexCount() / 1024) <<
		"Kbytes");
	const BVHAccel *bvh = (const BVHAccel *)newDataSet->GetAccelerator();
	vertsBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(Point) * newDataSet->GetTotalVertexCount(),
		bvh->GetPreprocessedMesh()->GetVertices());
	deviceDesc->AllocMemory(vertsBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Triangle indices buffer size: " <<
		(sizeof(Triangle) * newDataSet->GetTotalTriangleCount() / 1024) <<
		"Kbytes");
	trisBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(Triangle) * newDataSet->GetTotalTriangleCount(),
		bvh->GetPreprocessedMesh()->GetTriangles());
	deviceDesc->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] BVH buffer size: " <<
		(sizeof(BVHAccelArrayNode) * bvh->GetNNodes() / 1024) <<
		"Kbytes");
	bvhBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(BVHAccelArrayNode) * bvh->GetNNodes(),
		(void*)(bvh->GetTree()));
	deviceDesc->AllocMemory(bvhBuff->getInfo<CL_MEM_SIZE>());

	// Set arguments
	kernel->setArg(2, *vertsBuff);
	kernel->setArg(3, *trisBuff);
	kernel->setArg(4, newDataSet->GetTotalTriangleCount());
	kernel->setArg(5, bvh->GetNNodes());
	kernel->setArg(6, *bvhBuff);
}

void OpenCLBVHKernel::EnqueueRayBuffer(cl::Buffer &rBuff,
	cl::Buffer &hBuff, const unsigned int rayCount)
{
	kernel->setArg(0, rBuff);
	kernel->setArg(1, hBuff);
	kernel->setArg(7, rayCount);
	device->GetOpenCLQueue().enqueueNDRangeKernel(*kernel, cl::NullRange,
		cl::NDRange(rayCount), cl::NDRange(workGroupSize));
}

void OpenCLQBVHKernel::FreeBuffers()
{
	delete kernel;
	kernel = NULL;
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	deviceDesc->FreeMemory(trisBuff->getInfo<CL_MEM_SIZE>());
	delete trisBuff;
	trisBuff = NULL;
	deviceDesc->FreeMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());
	delete qbvhBuff;
	qbvhBuff = NULL;
}

void OpenCLQBVHKernel::SetDataSet(const DataSet *newDataSet)
{
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	cl::Device &oclDevice = device->GetOpenCLDevice();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QBVH max. stack size: " << stackSize);
	// Compile sources
	std::stringstream params;
	params << "-D QBVH_STACK_SIZE=" << stackSize;

	std::string code(
		_LUXRAYS_POINT_OCLDEFINE
		_LUXRAYS_VECTOR_OCLDEFINE
		_LUXRAYS_RAY_OCLDEFINE
		_LUXRAYS_RAYHIT_OCLDEFINE
		_LUXRAYS_BBOX_OCLDEFINE);
	code += KernelSource_QBVH;
	cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
	cl::Program program = cl::Program(oclContext, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(oclDevice);
		program.build(buildDevice, params.str().c_str());
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] QBVH compilation error:\n" << strError.c_str());

		throw err;
	}

	delete kernel;
	kernel = new cl::Kernel(program, "Intersect");
	kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
		&workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QBVH kernel work group size: " << workGroupSize);

	kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
		&workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Suggested work group size: " << workGroupSize);

	if (device->GetForceWorkGroupSize() > 0) {
		workGroupSize = device->GetForceWorkGroupSize();
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Forced work group size: " << workGroupSize);
	} else if (workGroupSize > 256) {
		// Otherwise I will probably run out of local memory
		workGroupSize = 256;
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Cap work group size to: " << workGroupSize);
	}

	const QBVHAccel *qbvh = (QBVHAccel *)newDataSet->GetAccelerator();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QBVH buffer size: " <<
		(sizeof(QBVHNode) * qbvh->GetNNodes() / 1024) << "Kbytes");
	qbvhBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * qbvh->GetNNodes(),
		(void*)(qbvh->GetTree()));
	deviceDesc->AllocMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QuadTriangle buffer size: " <<
		(sizeof(QuadTriangle) * qbvh->GetNQuads() / 1024) << "Kbytes");
	trisBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QuadTriangle) * qbvh->GetNQuads(),
		(void*)(qbvh->GetQuads()));
	deviceDesc->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());

	// Set arguments
	kernel->setArg(2, *qbvhBuff);
	kernel->setArg(3, *trisBuff);
	// Check if we have enough local memory
	if (stackSize * workGroupSize * sizeof(cl_int) >
		device->GetOpenCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>())
		throw std::runtime_error("Not enough OpenCL device local memory available for the required work group size"
			" and QBVH stack depth (try to reduce the work group size and/or the stack depth)");

	kernel->setArg(5, stackSize * workGroupSize * sizeof(cl_int), NULL);
}

void OpenCLQBVHKernel::EnqueueRayBuffer(cl::Buffer &rBuff,
	cl::Buffer &hBuff, const unsigned int rayCount)
{
	kernel->setArg(0, rBuff);
	kernel->setArg(1, hBuff);
	kernel->setArg(4, rayCount);
	device->GetOpenCLQueue().enqueueNDRangeKernel(*kernel, cl::NullRange,
		cl::NDRange(rayCount), cl::NDRange(workGroupSize));
}

void OpenCLQBVHImageKernel::FreeBuffers()
{
	delete kernel;
	kernel = NULL;
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	deviceDesc->FreeMemory(trisBuff->getInfo<CL_MEM_SIZE>());
	delete trisBuff;
	trisBuff = NULL;
	deviceDesc->FreeMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());
	delete qbvhBuff;
	qbvhBuff = NULL;
}

void OpenCLQBVHImageKernel::SetDataSet(const DataSet *newDataSet)
{
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	cl::Device &oclDevice = device->GetOpenCLDevice();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QBVH max. stack size: " << stackSize);
	// Compile sources
	std::stringstream params;
	params << "-D USE_IMAGE_STORAGE -D QBVH_STACK_SIZE=" << stackSize;

	std::string code(
		_LUXRAYS_POINT_OCLDEFINE
		_LUXRAYS_VECTOR_OCLDEFINE
		_LUXRAYS_RAY_OCLDEFINE
		_LUXRAYS_RAYHIT_OCLDEFINE
		_LUXRAYS_BBOX_OCLDEFINE);
	code += KernelSource_QBVH;
	cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
	cl::Program program = cl::Program(oclContext, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(oclDevice);
		program.build(buildDevice, params.str().c_str());
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] QBVH Image Storage compilation error:\n" <<
			strError.c_str());

		throw err;
	}

	delete kernel;
	kernel = new cl::Kernel(program, "Intersect");
	kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
		&workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QBVH Image Storage kernel work group size: " <<
		workGroupSize);

	kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
		&workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Suggested work group size: " << workGroupSize);

	if (device->GetForceWorkGroupSize() > 0) {
		workGroupSize = device->GetForceWorkGroupSize();
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Forced work group size: " << workGroupSize);
	} else if (workGroupSize > 256) {
		// Otherwise I will probably run out of local memory
		workGroupSize = 256;
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Cap work group size to: " << workGroupSize);
	}

	const QBVHAccel *qbvh = (QBVHAccel *)newDataSet->GetAccelerator();

	// Convert node indices to image coordinates
	// Calculate the required image size for the storage
	// 7 pixels required for the storage of a QBVH node
	const size_t nodePixelRequired = qbvh->GetNNodes() * 7;
	const size_t nodeWidth = Min(RoundUp(static_cast<unsigned int>(sqrtf(nodePixelRequired)), 7u),  0x7fffu);
	const size_t nodeHeight = nodePixelRequired / nodeWidth +
		(((nodePixelRequired % nodeWidth) == 0) ? 0 : 1);
	// 10 pixels required for the storage of QBVH Triangles
	const size_t leafPixelRequired = qbvh->GetNQuads() * 10;
	const size_t leafWidth = Min(RoundUp(static_cast<unsigned int>(sqrtf(leafPixelRequired)), 10u), 32760u);
	const size_t leafHeight = leafPixelRequired / leafWidth +
		(((leafPixelRequired % leafWidth) == 0) ? 0 : 1);

	unsigned int *inodes = new unsigned int[nodeWidth * nodeHeight * 4];
	const QBVHNode *qnodes = qbvh->GetTree();
	for (size_t i = 0; i < qbvh->GetNNodes(); ++i) {
		unsigned int *pnodes = (unsigned int *)(qnodes + i);
		const size_t offset = i * 7 * 4;

		for (size_t j = 0; j < 6 * 4; ++j)
			inodes[offset + j] = pnodes[j];

		for (size_t j = 0; j < 4; ++j) {
			int index = qnodes[i].children[j];

			if (QBVHNode::IsEmpty(index)) {
				inodes[offset + 6 * 4 + j] = index;
			} else if (QBVHNode::IsLeaf(index)) {
				int32_t count = QBVHNode::FirstQuadIndex(index) * 10;
				// "/ 10" in order to not waste bits
				const unsigned short x = static_cast<unsigned short>((count % leafWidth) / 10);
				const unsigned short y = static_cast<unsigned short>(count / leafWidth);
				((int32_t *)inodes)[offset + 6 * 4 + j] =  0x80000000 |
					(((static_cast<int32_t>(QBVHNode::NbQuadPrimitives(index)) - 1) & 0xf) << 27) |
					(static_cast<int32_t>((x << 16) | y) & 0x07ffffff);
			} else {
				index *= 7;
				// "/ 7" in order to not waste bits
				const unsigned short x = static_cast<unsigned short>((index % nodeWidth) / 7);
				const unsigned short y = static_cast<unsigned short>(index / nodeWidth);
				inodes[offset + 6 * 4 + j] = (x << 16) | y;
			}
		}
	}
	qbvhBuff = new cl::Image2D(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT32),
		nodeWidth, nodeHeight, 0, inodes);
	deviceDesc->AllocMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());
	delete[] inodes;

	unsigned int *iprims = new unsigned int[leafWidth * leafHeight * 4];
	memcpy(iprims, qbvh->GetQuads(), sizeof(QuadTriangle) * qbvh->GetNQuads());
	trisBuff = new cl::Image2D(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT32),
		leafWidth, leafHeight, 0, iprims);
	deviceDesc->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());
	delete[] iprims;

	// Set arguments
	kernel->setArg(2, *qbvhBuff);
	kernel->setArg(3, *trisBuff);
	// Check if we have enough local memory
	if (stackSize * workGroupSize * sizeof(cl_int) >
		device->GetOpenCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>())
		throw std::runtime_error("Not enough OpenCL device local memory available for the required work group size"
			" and QBVH stack depth (try to reduce the work group size and/or the stack depth)");
	kernel->setArg(5, stackSize * workGroupSize * sizeof(cl_int), NULL);
}

void OpenCLQBVHImageKernel::EnqueueRayBuffer(cl::Buffer &rBuff,
	cl::Buffer &hBuff, const unsigned int rayCount)
{
	kernel->setArg(0, rBuff);
	kernel->setArg(1, hBuff);
	kernel->setArg(4, rayCount);
	device->GetOpenCLQueue().enqueueNDRangeKernel(*kernel, cl::NullRange,
		cl::NDRange(rayCount), cl::NDRange(workGroupSize));
}

void OpenCLMQBVHKernel::FreeBuffers()
{
	delete kernel;
	kernel = NULL;
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	deviceDesc->FreeMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());
	delete mqbvhBuff;
	mqbvhBuff = NULL;
	deviceDesc->FreeMemory(memMapBuff->getInfo<CL_MEM_SIZE>());
	delete memMapBuff;
	memMapBuff = NULL;
	deviceDesc->FreeMemory(leafBuff->getInfo<CL_MEM_SIZE>());
	delete leafBuff;
	leafBuff = NULL;
	deviceDesc->FreeMemory(leafQuadTrisBuff->getInfo<CL_MEM_SIZE>());
	delete leafQuadTrisBuff;
	leafQuadTrisBuff = NULL;
	deviceDesc->FreeMemory(invTransBuff->getInfo<CL_MEM_SIZE>());
	delete invTransBuff;
	invTransBuff = NULL;
	deviceDesc->FreeMemory(trisOffsetBuff->getInfo<CL_MEM_SIZE>());
	delete trisOffsetBuff;
	trisOffsetBuff = NULL;
}

void OpenCLMQBVHKernel::SetDataSet(const DataSet *newDataSet)
{
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	cl::Device &oclDevice = device->GetOpenCLDevice();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	// Compile sources
	std::string code(
		_LUXRAYS_POINT_OCLDEFINE
		_LUXRAYS_VECTOR_OCLDEFINE
		_LUXRAYS_RAY_OCLDEFINE
		_LUXRAYS_RAYHIT_OCLDEFINE
		_LUXRAYS_BBOX_OCLDEFINE
		_LUXRAYS_MATRIX4X4_OCLDEFINE);
	code += KernelSource_MQBVH;
	cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
	cl::Program program = cl::Program(oclContext, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(oclDevice);
		program.build(buildDevice);
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] MQBVH compilation error:\n" << strError.c_str());
		throw err;
	}

	kernel = new cl::Kernel(program, "Intersect");
	kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
		&workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH kernel work group size: " << workGroupSize);

	kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
		&workGroupSize);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Suggested work group size: " << workGroupSize);

	if (device->GetForceWorkGroupSize() > 0) {
		workGroupSize = device->GetForceWorkGroupSize();
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Forced work group size: " << workGroupSize);
	} else if (workGroupSize > 256) {
		// Otherwise I will probably run out of local memory
		workGroupSize = 256;
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Cap work group size to: " << workGroupSize);
	}

	// TODO: remove the following limitation
	// NOTE: this code is somewhat limited to 32bit address space of the OpenCL device
	const MQBVHAccel *mqbvh = (MQBVHAccel *)newDataSet->GetAccelerator();

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH buffer size: " <<
		(sizeof(QBVHNode) * mqbvh->GetNNodes() / 1024) << "Kbytes");
	mqbvhBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * mqbvh->GetNNodes(), mqbvh->GetTree());
	deviceDesc->AllocMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());

	// Calculate the size of memory to allocate
	unsigned int totalNodesCount = 0;
	unsigned int totalQuadTrisCount = 0;

	std::map<const QBVHAccel *, unsigned int> indexNodesMap;
	std::map<const QBVHAccel *, unsigned int> indexQuadTrisMap;

	for (std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::const_iterator it = mqbvh->GetAccels().begin(); it != mqbvh->GetAccels().end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		indexNodesMap[qbvh] = totalNodesCount;
		totalNodesCount += qbvh->GetNNodes();
		indexQuadTrisMap[qbvh] = totalQuadTrisCount;
		totalQuadTrisCount += qbvh->GetNQuads();
	}

	// Allocate memory for QBVH Leafs
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH leaf Nodes buffer size: " <<
		(totalNodesCount * sizeof(QBVHNode) / 1024) << "Kbytes");
	leafBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY,
		totalNodesCount * sizeof(QBVHNode));
	deviceDesc->AllocMemory(leafBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH QuadTriangle buffer size: " <<
		(totalQuadTrisCount * sizeof(QuadTriangle) / 1024) << "Kbytes");
	leafQuadTrisBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY,
		totalQuadTrisCount * sizeof(QuadTriangle));
	deviceDesc->AllocMemory(leafQuadTrisBuff->getInfo<CL_MEM_SIZE>());

	unsigned int *memMap = new unsigned int[mqbvh->GetNLeafs() * 2];
	for (unsigned int i = 0; i < mqbvh->GetNLeafs(); ++i) {
		memMap[i * 2] = indexNodesMap[mqbvh->GetLeafs()[i]];
		memMap[i * 2 + 1] = indexQuadTrisMap[mqbvh->GetLeafs()[i]];
	}
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH memory map buffer size: " <<
		(mqbvh->GetNLeafs() * sizeof(unsigned int) * 2 / 1024) <<
		"Kbytes");
	memMapBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		mqbvh->GetNLeafs() * sizeof(unsigned int) * 2, memMap);
	deviceDesc->AllocMemory(memMapBuff->getInfo<CL_MEM_SIZE>());
	delete memMap;

	// Upload QBVH leafs
	size_t nodesMemOffset = 0;
	size_t quadTrisMemOffset = 0;
	for (std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::const_iterator it = mqbvh->GetAccels().begin(); it != mqbvh->GetAccels().end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		const size_t nodesMemSize = sizeof(QBVHNode) * qbvh->GetNNodes();
		device->GetOpenCLQueue().enqueueWriteBuffer(
			*leafBuff,
			CL_FALSE,
			nodesMemOffset,
			nodesMemSize,
			qbvh->GetTree());
		nodesMemOffset += nodesMemSize;

		const size_t quadTrisMemSize = sizeof(QuadTriangle) * qbvh->GetNQuads();
		device->GetOpenCLQueue().enqueueWriteBuffer(
			*leafQuadTrisBuff,
			CL_FALSE,
			quadTrisMemOffset,
			quadTrisMemSize,
			qbvh->GetQuads());
		quadTrisMemOffset += quadTrisMemSize;
	}

	// Upload QBVH leafs transformations
	Matrix4x4 *invTrans = new Matrix4x4[mqbvh->GetNLeafs()];
	for (unsigned int i = 0; i < mqbvh->GetNLeafs(); ++i) {
		if (mqbvh->GetTransforms()[i])
			invTrans[i] = mqbvh->GetTransforms()[i]->mInv;
		else
			invTrans[i] = Matrix4x4();
	}
	const size_t invTransMemSize = mqbvh->GetNLeafs() * sizeof(Matrix4x4);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH inverse transformations buffer size: " <<
		(invTransMemSize / 1024) << "Kbytes");
	invTransBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		invTransMemSize, invTrans);
	deviceDesc->AllocMemory(invTransBuff->getInfo<CL_MEM_SIZE>());
	delete invTrans;

	// Upload primitive offsets
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH primitive offsets buffer size: " <<
		(sizeof(unsigned int) * mqbvh->GetNLeafs() / 1024) << "Kbytes");
	trisOffsetBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(unsigned int) * mqbvh->GetNLeafs(),
		(unsigned int *)(mqbvh->GetLeafsOffsets()));
	deviceDesc->AllocMemory(trisOffsetBuff->getInfo<CL_MEM_SIZE>());

	// Set arguments
	kernel->setArg(2, *mqbvhBuff);
	kernel->setArg(4, *memMapBuff);
	kernel->setArg(5, *leafBuff);
	kernel->setArg(6, *leafQuadTrisBuff);
	kernel->setArg(7, *invTransBuff);
	kernel->setArg(8, *trisOffsetBuff);
}

void OpenCLMQBVHKernel::UpdateDataSet(const DataSet *newDataSet) {
	const Context *deviceContext = device->GetContext();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Updating DataSet");

	const MQBVHAccel *mqbvh = (MQBVHAccel *)newDataSet->GetAccelerator();

	// Upload QBVH leafs transformations
	Matrix4x4 *invTrans = new Matrix4x4[mqbvh->GetNLeafs()];
	for (unsigned int i = 0; i < mqbvh->GetNLeafs(); ++i) {
		if (mqbvh->GetTransforms()[i])
			invTrans[i] = mqbvh->GetTransforms()[i]->mInv;
		else
			invTrans[i] = Matrix4x4();
	}

	device->GetOpenCLQueue().enqueueWriteBuffer(
		*invTransBuff,
		CL_TRUE,
		0,
		mqbvh->GetNLeafs() * sizeof(Matrix4x4),
		invTrans);
	delete invTrans;

	// Update MQBVH nodes
	deviceDesc->FreeMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());
	delete mqbvhBuff;

	mqbvhBuff = new cl::Buffer(deviceDesc->GetOCLContext(),
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * mqbvh->GetNNodes(), mqbvh->GetTree());
	deviceDesc->AllocMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());

	kernel->setArg(2, *mqbvhBuff);
}

void OpenCLMQBVHKernel::EnqueueRayBuffer(cl::Buffer &rBuff,
	cl::Buffer &hBuff, const unsigned int rayCount)
{
	kernel->setArg(0, rBuff);
	kernel->setArg(1, hBuff);
	kernel->setArg(3, rayCount);
	device->GetOpenCLQueue().enqueueNDRangeKernel(*kernel, cl::NullRange,
		cl::NDRange(rayCount), cl::NDRange(workGroupSize));
}

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

size_t OpenCLIntersectionDevice::RayBufferSize = OPENCL_RAYBUFFER_SIZE;

OpenCLIntersectionDevice::OpenCLIntersectionDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const size_t index,
		const unsigned int forceWGSize) :
		HardwareIntersectionDevice(context, DEVICE_TYPE_OPENCL, index),
		oclQueue(new cl::CommandQueue(desc->GetOCLContext(),
		desc->GetOCLDevice())), kernel(NULL)
{
	forceWorkGroupSize = forceWGSize;
	stackSize = 24;
	deviceDesc = desc;
	deviceName = (desc->GetName() +"Intersect").c_str();
	reportedPermissionError = false;
	qbvhUseImage = false;
	qbvhDisableImageStorage = false;
	hybridRenderingSupport = true;
	intersectionThread = NULL;

	externalRayBufferQueue = NULL;
}

OpenCLIntersectionDevice::~OpenCLIntersectionDevice() {
	if (started)
		Stop();

	FreeDataSetBuffers();

	delete oclQueue;
}

void OpenCLIntersectionDevice::SetExternalRayBufferQueue(RayBufferQueue *queue) {
	assert (!started);

	externalRayBufferQueue = queue;
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer() {
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer(const size_t size) {
	return new RayBuffer(RoundUpPow2<size_t>(size));
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
		deviceDesc->FreeMemory(raysBuff->getInfo<CL_MEM_SIZE>());
		delete raysBuff;
		raysBuff = NULL;
		deviceDesc->FreeMemory(hitsBuff->getInfo<CL_MEM_SIZE>());
		delete hitsBuff;
		hitsBuff = NULL;
	}
	delete kernel;
	kernel = NULL;
}

void OpenCLIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	FreeDataSetBuffers();

	IntersectionDevice::SetDataSet(newDataSet);

	if (!newDataSet)
		return;

	cl::Context &oclContext = GetOpenCLContext();

	// Allocate OpenCL buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Ray buffer size: " <<
		(sizeof(Ray) * RayBufferSize / 1024) << "Kbytes");
	raysBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY,
		sizeof(Ray) * RayBufferSize);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Ray hits buffer size: " <<
		(sizeof(RayHit) * RayBufferSize / 1024) << "Kbytes");
	hitsBuff = new cl::Buffer(oclContext, CL_MEM_WRITE_ONLY,
		sizeof(RayHit) * RayBufferSize);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	delete kernel;
	switch (dataSet->GetAcceleratorType()) {
		case ACCEL_BVH:
			//------------------------------------------------------------------
			// BVH kernel
			//------------------------------------------------------------------
			kernel = new OpenCLBVHKernel(this);
			break;
		case ACCEL_QBVH:
			//------------------------------------------------------------------
			// QBVH kernel
			//------------------------------------------------------------------
			// Check if I can use image to store the data set
			if (qbvhDisableImageStorage) {
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Disable forced for QBVH scene storage inside image");
				qbvhUseImage = false;
			} else {
				if (!deviceDesc->HasImageSupport() ||
					(deviceDesc->GetOpenCLType() != OCL_DEVICE_TYPE_GPU)) {
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] OpenCL image support is not available");
					qbvhUseImage = false;
				} else {
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] OpenCL image support is available");

					// Check if the scene is small enough to be stored inside an image
					const size_t maxWidth = deviceDesc->GetImage2DMaxWidth();
					const size_t maxHeight = deviceDesc->GetImage2DMaxHeight();

					const QBVHAccel *qbvh = (QBVHAccel *)newDataSet->GetAccelerator();
					// Calculate the required image size for the storage

					// 7 pixels required for the storage of a QBVH node
					const size_t nodeImagePixelRequired = qbvh->nNodes * 7;
					const size_t nodeImageWidth = Min(RoundUp(static_cast<unsigned int>(sqrtf(nodeImagePixelRequired)), 7u),  0x7fffu);
					const size_t nodeImageHeight = nodeImagePixelRequired / nodeImageWidth + (((nodeImagePixelRequired % nodeImageWidth) == 0) ? 0 : 1);

					// 10 pixels required for the storage of QBVH Triangles
					const size_t leafPixelRequired = qbvh->nQuads * 10;
					const size_t leafImageWidth = Min(RoundUp(static_cast<unsigned int>(sqrtf(leafPixelRequired)), 10u), 32760u);
					const size_t leafImageHeight = leafPixelRequired / leafImageWidth + (((leafPixelRequired % leafImageWidth) == 0) ? 0 : 1);

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
				}
			}
			if (qbvhUseImage)
				kernel = new OpenCLQBVHImageKernel(this);
			else
				kernel = new OpenCLQBVHKernel(this);
			break;
		case ACCEL_MQBVH:
			//------------------------------------------------------------------
			// MQBVH kernel
			//------------------------------------------------------------------
			kernel = new OpenCLMQBVHKernel(this);
			break;
		default:
			kernel = NULL;
			assert (false);
	}
	if (kernel) {
		kernel->SetMaxStackSize(stackSize);
		kernel->SetDataSet(newDataSet);
	}
}

void OpenCLIntersectionDevice::UpdateDataSet() {
	if (kernel)
		kernel->UpdateDataSet(dataSet);
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	if (hybridRenderingSupport) {
		// Create the thread for the rendering
		intersectionThread = new boost::thread(boost::bind(OpenCLIntersectionDevice::IntersectionThread, this));

		// Set intersectionThread priority
		bool res = SetThreadRRPriority(intersectionThread);
		if (res && !reportedPermissionError) {
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)");
			reportedPermissionError = true;
		}
	}
}

void OpenCLIntersectionDevice::Interrupt() {
	assert (started);

	if (hybridRenderingSupport)
		intersectionThread->interrupt();
}

void OpenCLIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	if (hybridRenderingSupport) {
		intersectionThread->interrupt();
		intersectionThread->join();
		delete intersectionThread;
		intersectionThread = NULL;

		if (!externalRayBufferQueue)
			rayBufferQueue.Clear();
	}
}

void OpenCLIntersectionDevice::TraceRayBuffer(RayBuffer *rayBuffer, cl::Event *event) {
	// Upload the rays to the GPU
	oclQueue->enqueueWriteBuffer(*raysBuff, CL_FALSE, 0,
		sizeof(Ray) * rayBuffer->GetRayCount(),
		rayBuffer->GetRayBuffer());

	EnqueueTraceRayBuffer(*raysBuff, *hitsBuff, rayBuffer->GetSize());

	// Download the results
	oclQueue->enqueueReadBuffer(*hitsBuff, CL_FALSE, 0,
		sizeof(RayHit) * rayBuffer->GetRayCount(),
		rayBuffer->GetHitBuffer(), NULL, event);
}

void OpenCLIntersectionDevice::EnqueueTraceRayBuffer(cl::Buffer &rBuff,
	cl::Buffer &hBuff, const unsigned int rayCount)
{
	if (kernel) {
		kernel->EnqueueRayBuffer(rBuff, hBuff, rayCount);
		statsTotalRayCount += rayCount;
	}
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
					queue->PushDone(rayBuffer0);

					// Pop 1 ray buffer
					event1.wait();
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
					queue->PushDone(rayBuffer0);

					// Pop 1 ray buffer
					event1.wait();
					queue->PushDone(rayBuffer1);

					// Pop 2 ray buffer
					event2.wait();
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
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread ERROR: " << err.what() << "(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}
}

#endif
