/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"
#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"
#endif

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLMQBVHKernels : public OpenCLKernels {
public:
	OpenCLMQBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const MQBVHAccel *accel) :
		OpenCLKernels(dev, kernelCount), mqbvh(accel), mqbvhBuff(NULL), memMapBuff(NULL), leafBuff(NULL),
		leafQuadTrisBuff(NULL), leafTransIndexBuff(NULL), uniqueInvTransBuff(NULL), leafsMotionSystemBuff(NULL),
		uniqueLeafsInterpolatedTransformBuff(NULL) {
		const Context *deviceContext = device->GetContext();
		const std::string &deviceName(device->GetName());
		cl::Context &oclContext = device->GetOpenCLContext();
		cl::Device &oclDevice = device->GetOpenCLDevice();

		//----------------------------------------------------------------------
		// Compile kernel sources
		//----------------------------------------------------------------------

		std::stringstream opts;
		opts << " -D LUXRAYS_OPENCL_KERNEL"
				" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
				" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH compile options: \n" << opts.str());

		intersectionKernelSource =
				luxrays::ocl::KernelSource_qbvh_types +
				luxrays::ocl::KernelSource_mqbvh;

		const std::string code(
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_epsilon_types +
			luxrays::ocl::KernelSource_epsilon_funcs +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_matrix4x4_types +
			luxrays::ocl::KernelSource_matrix4x4_funcs +
			luxrays::ocl::KernelSource_quaternion_types +
			luxrays::ocl::KernelSource_quaternion_funcs +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_ray_funcs +
			luxrays::ocl::KernelSource_bbox_types +
			luxrays::ocl::KernelSource_bbox_funcs +
			luxrays::ocl::KernelSource_transform_types +
			luxrays::ocl::KernelSource_transform_funcs +
			luxrays::ocl::KernelSource_motionsystem_types +
			luxrays::ocl::KernelSource_motionsystem_funcs +
			luxrays::ocl::KernelSource_qbvh_types +
			luxrays::ocl::KernelSource_mqbvh);
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error &err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] MQBVH compilation error:\n" << strError.c_str());
			throw err;
		}

		for (u_int i = 0; i < kernelCount; ++i) {
			kernels[i] = new cl::Kernel(program, "Accelerator_Intersect_RayBuffer");

			if (device->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
				workGroupSize = device->GetDeviceDesc()->GetForceWorkGroupSize();
			else {
				kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
					CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
				//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				//	"] MQBVH kernel work group size: " << workGroupSize);

				if (workGroupSize > 256) {
					// Otherwise I will probably run out of local memory
					workGroupSize = 256;
					//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
					//	"] Cap work group size to: " << workGroupSize);
				}
			}
		}
	}
	virtual ~OpenCLMQBVHKernels() {
		device->FreeMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());
		delete mqbvhBuff;
		mqbvhBuff = NULL;
		device->FreeMemory(memMapBuff->getInfo<CL_MEM_SIZE>());
		delete memMapBuff;
		memMapBuff = NULL;
		device->FreeMemory(leafBuff->getInfo<CL_MEM_SIZE>());
		delete leafBuff;
		leafBuff = NULL;
		device->FreeMemory(leafQuadTrisBuff->getInfo<CL_MEM_SIZE>());
		delete leafQuadTrisBuff;
		leafQuadTrisBuff = NULL;
		device->FreeMemory(leafTransIndexBuff->getInfo<CL_MEM_SIZE>());
		delete leafTransIndexBuff;
		leafTransIndexBuff = NULL;
		device->FreeMemory(uniqueInvTransBuff->getInfo<CL_MEM_SIZE>());
		delete uniqueInvTransBuff;
		uniqueInvTransBuff = NULL;
		device->FreeMemory(leafsMotionSystemBuff->getInfo<CL_MEM_SIZE>());
		delete leafsMotionSystemBuff;
		leafsMotionSystemBuff = NULL;
		device->FreeMemory(uniqueLeafsInterpolatedTransformBuff->getInfo<CL_MEM_SIZE>());
		delete uniqueLeafsInterpolatedTransformBuff;
		uniqueLeafsInterpolatedTransformBuff = NULL;
	}

	void SetBuffers(cl::Buffer *m, cl::Buffer *l, cl::Buffer *q,
		cl::Buffer *mm, cl::Buffer *ti, cl::Buffer *t, cl::Buffer *ms,
		cl::Buffer *it);
	virtual void Update(const DataSet *newDataSet);
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex);

protected:
	const MQBVHAccel *mqbvh;

	// MQBVH fields
	cl::Buffer *mqbvhBuff;
	cl::Buffer *memMapBuff;
	cl::Buffer *leafBuff;
	cl::Buffer *leafQuadTrisBuff;
	cl::Buffer *leafTransIndexBuff;
	cl::Buffer *uniqueInvTransBuff;
	cl::Buffer *leafsMotionSystemBuff;
	cl::Buffer *uniqueLeafsInterpolatedTransformBuff;

};

void OpenCLMQBVHKernels::SetBuffers(cl::Buffer *m, cl::Buffer *l, cl::Buffer *q,
	cl::Buffer *mm, cl::Buffer *ti, cl::Buffer *t, cl::Buffer *ms, cl::Buffer *it) {
	mqbvhBuff = m;
	leafBuff = l;
	leafQuadTrisBuff = q;
	memMapBuff = mm;
	leafTransIndexBuff = ti;
	uniqueInvTransBuff = t;
	leafsMotionSystemBuff = ms;
	uniqueLeafsInterpolatedTransformBuff = it;

	// Set arguments
	BOOST_FOREACH(cl::Kernel *kernel, kernels)
		SetIntersectionKernelArgs(*kernel, 3);
}

void OpenCLMQBVHKernels::Update(const DataSet *newDataSet) {
	const Context *deviceContext = device->GetContext();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Updating DataSet");

	// Upload QBVH leafs transformations
	vector<u_int> leafTransIndex(mqbvh->GetNLeafs());
	vector<Matrix4x4> uniqueInvTrans;
	for (u_int i = 0; i < mqbvh->GetNLeafs(); ++i) {
		if (mqbvh->GetTransforms()[i]) {
			leafTransIndex[i] = uniqueInvTrans.size();
			uniqueInvTrans.push_back(mqbvh->GetTransforms()[i]->mInv);
		} else
			leafTransIndex[i] = NULL_INDEX;
	}

	if (uniqueInvTrans.size() == 0) {
		// Just a place holder
		Matrix4x4 placeHolder;
		uniqueInvTrans.push_back(placeHolder);
	}

	device->GetOpenCLQueue().enqueueWriteBuffer(
		*leafTransIndexBuff,
		CL_TRUE,
		0,
		sizeof(u_int) * leafTransIndex.size(),
		&leafTransIndex[0]);
	device->GetOpenCLQueue().enqueueWriteBuffer(
		*uniqueInvTransBuff,
		CL_TRUE,
		0,
		sizeof(luxrays::ocl::Matrix4x4) * uniqueInvTrans.size(),
		&uniqueInvTrans[0]);

	// Update MQBVH nodes
	device->FreeMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());
	delete mqbvhBuff;

	mqbvhBuff = new cl::Buffer(deviceDesc->GetOCLContext(),
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * mqbvh->GetNNodes(), mqbvh->GetTree());
	device->AllocMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());

	BOOST_FOREACH(cl::Kernel *kernel, kernels)
		SetIntersectionKernelArgs(*kernel, 3);
}

void OpenCLMQBVHKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
	kernels[kernelIndex]->setArg(0, rBuff);
	kernels[kernelIndex]->setArg(1, hBuff);
	kernels[kernelIndex]->setArg(2, rayCount);

	const u_int globalRange = RoundUp<u_int>(rayCount, workGroupSize);
	oclQueue.enqueueNDRangeKernel(*kernels[kernelIndex], cl::NullRange,
		cl::NDRange(globalRange), cl::NDRange(workGroupSize), events,
		event);
}

u_int OpenCLMQBVHKernels::SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int index) {
	u_int argIndex = index;
	kernel.setArg(argIndex++, *mqbvhBuff);
	kernel.setArg(argIndex++, *memMapBuff);
	kernel.setArg(argIndex++, *leafBuff);
	kernel.setArg(argIndex++, *leafQuadTrisBuff);
	kernel.setArg(argIndex++, *leafTransIndexBuff);
	kernel.setArg(argIndex++, *uniqueInvTransBuff);
	kernel.setArg(argIndex++, *leafsMotionSystemBuff);
	kernel.setArg(argIndex++, *uniqueLeafsInterpolatedTransformBuff);

	return argIndex;
}

OpenCLKernels *MQBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	const std::string &deviceName(device->GetName());

	// TODO: remove the following limitation
	// NOTE: this code is somewhat limited to 32bit address space of the OpenCL device

	// Allocate buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH buffer size: " <<
		(sizeof(QBVHNode) * nNodes / 1024) << "Kbytes");
	cl::Buffer *mqbvhBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * nNodes, nodes);
	device->AllocMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());

	// Calculate the size of memory to allocate
	u_int totalNodesCount = 0;
	u_int totalQuadTrisCount = 0;

	std::map<const QBVHAccel *, u_int> indexNodesMap;
	std::map<const QBVHAccel *, u_int> indexQuadTrisMap;

	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		indexNodesMap[qbvh] = totalNodesCount;
		totalNodesCount += qbvh->nNodes;
		indexQuadTrisMap[qbvh] = totalQuadTrisCount;
		totalQuadTrisCount += qbvh->nQuads;
	}

	//--------------------------------------------------------------------------
	// Pack together all QBVH Leafs
	//--------------------------------------------------------------------------

	QBVHNode *allLeafs = new QBVHNode[totalNodesCount];
	u_int nodesOffset = 0;
	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		const size_t nodesMemSize = sizeof(QBVHNode) * qbvh->nNodes;
		memcpy(&allLeafs[nodesOffset], qbvh->nodes, nodesMemSize);
		nodesOffset += qbvh->nNodes;
	}

	//--------------------------------------------------------------------------
	// Allocate memory for QBVH Leafs
	//--------------------------------------------------------------------------

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH leaf Nodes buffer size: " <<
		(totalNodesCount * sizeof(QBVHNode) / 1024) << "Kbytes");
	cl::Buffer *leafBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * totalNodesCount, allLeafs);
	device->AllocMemory(leafBuff->getInfo<CL_MEM_SIZE>());
	delete[] allLeafs;

	// Pack together all QuadTriangle
	QuadTriangle *allQuadTris = new QuadTriangle[totalQuadTrisCount];

	u_int quadTrisOffset = 0;
	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		const size_t quadTrisMemSize = sizeof(QuadTriangle) * qbvh->nQuads;
		memcpy(&allQuadTris[quadTrisOffset], qbvh->prims, quadTrisMemSize);
		quadTrisOffset += qbvh->nQuads;
	}

	//--------------------------------------------------------------------------
	// Allocate memory for QBVH QuadTriangles
	//--------------------------------------------------------------------------

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH QuadTriangle buffer size: " <<
		(totalQuadTrisCount * sizeof(QuadTriangle) / 1024) << "Kbytes");
	cl::Buffer *leafQuadTrisBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QuadTriangle) * totalQuadTrisCount, allQuadTris);
	device->AllocMemory(leafQuadTrisBuff->getInfo<CL_MEM_SIZE>());
	delete[] allQuadTris;

	u_int *memMap = new u_int[nLeafs * 2];
	for (u_int i = 0; i < nLeafs; ++i) {
		memMap[i * 2] = indexNodesMap[leafs[i]];
		memMap[i * 2 + 1] = indexQuadTrisMap[leafs[i]];
	}
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH memory map buffer size: " <<
		(nLeafs * sizeof(u_int) * 2 / 1024) <<
		"Kbytes");
	cl::Buffer *memMapBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(u_int) * nLeafs * 2, memMap);
	device->AllocMemory(memMapBuff->getInfo<CL_MEM_SIZE>());
	delete[] memMap;

	//--------------------------------------------------------------------------
	// Upload QBVH leaf transformations
	//--------------------------------------------------------------------------

	vector<u_int> leafTransIndex(nLeafs);
	vector<Matrix4x4> uniqueInvTrans;
	for (u_int i = 0; i < nLeafs; ++i) {
		if (leafsTransform[i]) {
			leafTransIndex[i] = uniqueInvTrans.size();
			uniqueInvTrans.push_back(leafsTransform[i]->mInv);
		} else
			leafTransIndex[i] = NULL_INDEX;
	}

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH inverse transformations index buffer size: " <<
		(sizeof(u_int) * leafTransIndex.size() / 1024) << "Kbytes");
	cl::Buffer *leafTransIndexBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(u_int) * leafTransIndex.size(), &leafTransIndex[0]);
	device->AllocMemory(leafTransIndexBuff->getInfo<CL_MEM_SIZE>());
	
	cl::Buffer *uniqueInvTransBuff;
	if (uniqueInvTrans.size() > 0) {
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] MQBVH inverse transformations buffer size: " <<
			(sizeof(luxrays::ocl::Matrix4x4) * uniqueInvTrans.size() / 1024) <<
			"Kbytes");
		uniqueInvTransBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(luxrays::ocl::Matrix4x4) * uniqueInvTrans.size(),
			(void *)&uniqueInvTrans[0]);
	} else {
		// Just allocating a place holder
		luxrays::ocl::Matrix4x4 placeHolder;
		uniqueInvTransBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(luxrays::ocl::Matrix4x4),
			(void *)&placeHolder);
	}
	device->AllocMemory(uniqueInvTransBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Upload QBVH leaf motion systems
	//--------------------------------------------------------------------------

	// Transform CPU data structures in OpenCL data structures

	vector<luxrays::ocl::MotionSystem> motionSystems;
	vector<luxrays::ocl::InterpolatedTransform> interpolatedTransforms;
	BOOST_FOREACH(const MotionSystem *ms, leafsMotionSystem) {
		luxrays::ocl::MotionSystem oclMotionSystem;

		if (ms) {
			oclMotionSystem.interpolatedTransformFirstIndex = interpolatedTransforms.size();
			BOOST_FOREACH(const InterpolatedTransform &it, ms->interpolatedTransforms) {
				// Here, I assume that luxrays::ocl::InterpolatedTransform and
				// luxrays::InterpolatedTransform are the same
				interpolatedTransforms.push_back(*((const luxrays::ocl::InterpolatedTransform *)&it));
			}
			oclMotionSystem.interpolatedTransformLastIndex = interpolatedTransforms.size() - 1;
		} else {
			oclMotionSystem.interpolatedTransformFirstIndex = NULL_INDEX;
			oclMotionSystem.interpolatedTransformLastIndex = NULL_INDEX;
		}

		motionSystems.push_back(oclMotionSystem);
	}

	// Allocate the motion system buffer
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH Leaf motion systems buffer size: " <<
		(sizeof(luxrays::ocl::MotionSystem) * motionSystems.size() / 1024) <<
		"Kbytes");
	cl::Buffer *leafMotionSystemBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(luxrays::ocl::MotionSystem) * motionSystems.size(),
		(void *)&(motionSystems[0]));
	device->AllocMemory(leafMotionSystemBuff->getInfo<CL_MEM_SIZE>());

	// Allocate the InterpolatedTransform buffer
	cl::Buffer *leafInterpolatedTransformBuff;
	if (interpolatedTransforms.size() > 0) {
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] MQBVH Leaf interpolated transforms buffer size: " <<
			(sizeof(luxrays::ocl::InterpolatedTransform) * interpolatedTransforms.size() / 1024) <<
			"Kbytes");
		leafInterpolatedTransformBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(luxrays::ocl::InterpolatedTransform) * interpolatedTransforms.size(),
			(void *)&interpolatedTransforms[0]);
	} else {
		// Just allocating a place holder
		luxrays::ocl::InterpolatedTransform placeHolder;
		leafInterpolatedTransformBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(luxrays::ocl::InterpolatedTransform),
			(void *)&placeHolder);
	}
	device->AllocMemory(leafInterpolatedTransformBuff->getInfo<CL_MEM_SIZE>());

	// Setup kernels
	OpenCLMQBVHKernels *kernels = new OpenCLMQBVHKernels(device, kernelCount, this);
	kernels->SetBuffers(mqbvhBuff, leafBuff, leafQuadTrisBuff, memMapBuff,
				leafTransIndexBuff, uniqueInvTransBuff, leafMotionSystemBuff, leafInterpolatedTransformBuff);

	return kernels;
}

bool MQBVHAccel::CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const {
	const OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();

	u_int totalNodesCount = 0;
	u_int totalQuadTrisCount = 0;
	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;
		totalNodesCount += qbvh->nNodes;
		totalQuadTrisCount += qbvh->nQuads;
	}

	// Check if I can allocate enough space for the accelerator data
	if (sizeof(QBVHNode) * nNodes > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because root node buffer is too big: " <<
			(sizeof(QBVHNode) * nNodes / 1024) << "Kbytes");
		return false;
	}
	if (sizeof(QBVHNode) * totalNodesCount > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because leafs node buffer is too big: " <<
			(sizeof(QBVHNode) * totalNodesCount / 1024) << "Kbytes");
		return false;
	}
	if (sizeof(QuadTriangle) * totalQuadTrisCount > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because triangle buffer is too big: " <<
			(sizeof(QuadTriangle) * totalQuadTrisCount / 1024) << "Kbytes");
		return false;
	}
	return true;
}

#else

OpenCLKernels *MQBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	return NULL;
}

bool MQBVHAccel::CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const {
	return false;
}

#endif

MQBVHAccel::MQBVHAccel(const Context *context,
		u_int fst, u_int sf) : fullSweepThreshold(fst),
		skipFactor(sf), accels(MeshPtrCompare), ctx(context) {
	initialized = false;
}

MQBVHAccel::~MQBVHAccel() {
	if (initialized) {
		FreeAligned(nodes);

		delete[] leafs;

		for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::iterator it = accels.begin(); it != accels.end(); it++)
			delete it->second;
	}
}

bool MQBVHAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

void MQBVHAccel::Init(const std::deque<const Mesh *> &meshes, const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	assert (!initialized);

	meshList = meshes;

	// Build a QBVH for each mesh
	nLeafs = meshList.size();
	LR_LOG(ctx, "MQBVH leaf count: " << nLeafs);

	leafs = new QBVHAccel*[nLeafs];
	leafsTransform.resize(nLeafs, NULL);
	leafsMotionSystem.resize(nLeafs, NULL);
	u_int currentOffset = 0;
	double lastPrint = WallClockTime();
	for (u_int i = 0; i < nLeafs; ++i) {
		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			LR_LOG(ctx, "Building QBVH for MQBVH leaf: " << i << "/" << nLeafs);
			lastPrint = now;
		}

		switch (meshList[i]->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE: {
				leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
				leafs[i]->Init(std::deque<const Mesh *>(1, meshList[i]),
						meshList[i]->GetTotalVertexCount(), meshList[i]->GetTotalTriangleCount());
				accels[meshList[i]] = leafs[i];
				break;
			}
			case TYPE_TRIANGLE_INSTANCE:
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(meshList[i]);

				// Check if a QBVH has already been created
				std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::iterator it = accels.find(itm->GetTriangleMesh());

				if (it == accels.end()) {
					// Create a new QBVH
					leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
					leafs[i]->Init(std::deque<const Mesh *>(1, itm->GetTriangleMesh()),
							itm->GetTotalVertexCount(), itm->GetTotalTriangleCount());
					accels[itm->GetTriangleMesh()] = leafs[i];
				} else {
					//LR_LOG(ctx, "Cached QBVH leaf");
					leafs[i] = it->second;
				}

				leafsTransform[i] = &itm->GetTransformation();
				break;
			}
			case TYPE_TRIANGLE_MOTION:
			case TYPE_EXT_TRIANGLE_MOTION: {
				const MotionTriangleMesh *mtm = dynamic_cast<const MotionTriangleMesh *>(meshList[i]);

				// Check if a QBVH has already been created
				std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::iterator it = accels.find(mtm->GetTriangleMesh());

				if (it == accels.end()) {
					// Create a new QBVH
					leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
					leafs[i]->Init(std::deque<const Mesh *>(1, mtm->GetTriangleMesh()),
							mtm->GetTotalVertexCount(), mtm->GetTotalTriangleCount());
					accels[mtm->GetTriangleMesh()] = leafs[i];
				} else {
					//LR_LOG(ctx, "Cached QBVH leaf");
					leafs[i] = it->second;
				}

				leafsMotionSystem[i] = &mtm->GetMotionSystem();
				break;
			}
			default:
				throw std::runtime_error("Unknown Mesh type in MQBVHAccel::Init(): " + ToString(meshList[i]->GetType()));
		}

		currentOffset += meshList[i]->GetTotalTriangleCount();
	}

	maxNodes = 2 * nLeafs - 1;
	nodes = AllocAligned<QBVHNode>(maxNodes);

	Update();
	
	initialized = true;
}

void MQBVHAccel::Update() {
	// Temporary data for building
	u_int *primsIndexes = new u_int[nLeafs];

	nNodes = 0;
	for (u_int i = 0; i < maxNodes; ++i)
		nodes[i] = QBVHNode();

	// The arrays that will contain
	// - the bounding boxes for all leafs
	// - the centroids for all leafs
	BBox *primsBboxes = new BBox[nLeafs];
	Point *primsCentroids = new Point[nLeafs];
	// The bouding volume of all the centroids
	BBox centroidsBbox;

	// Fill each base array
	for (u_int i = 0; i < nLeafs; ++i) {
		// This array will be reorganized during construction.
		primsIndexes[i] = i;

		// Compute the bounding box for the triangle
		primsBboxes[i] = meshList[i]->GetBBox();
		primsBboxes[i].Expand(MachineEpsilon::E(primsBboxes[i]));
		primsCentroids[i] = (primsBboxes[i].pMin + primsBboxes[i].pMax) * .5f;

		// Update the global bounding boxes
		worldBound = Union(worldBound, primsBboxes[i]);
		centroidsBbox = Union(centroidsBbox, primsCentroids[i]);
	}

	// Recursively build the tree
	LR_LOG(ctx, "Building MQBVH, leafs: " << nLeafs << ", initial nodes: " << maxNodes);

	BuildTree(0, nLeafs, primsIndexes, primsBboxes, primsCentroids,
			worldBound, centroidsBbox, -1, 0, 0);

	LR_LOG(ctx, "MQBVH completed with " << nNodes << "/" << maxNodes << " nodes");
	LR_LOG(ctx, "Total MQBVH memory usage: " << nNodes * sizeof(QBVHNode) / 1024 << "Kbytes");

	// Release temporary memory
	delete[] primsBboxes;
	delete[] primsCentroids;
	delete[] primsIndexes;
}

void MQBVHAccel::BuildTree(u_int start, u_int end, u_int *primsIndexes,
		BBox *primsBboxes, Point *primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex, int32_t childIndex, int depth) {
	if (depth > 64)
		throw std::runtime_error("Maximum recursion depth reached while constructing MQBVH");

	// Create a leaf ?
	//********
	if (end - start == 1) {
		CreateLeaf(parentIndex, childIndex, primsIndexes[start], nodeBbox);
		return;
	}

	int32_t currentNode = parentIndex;
	int32_t leftChildIndex = childIndex;
	int32_t rightChildIndex = childIndex + 1;

	// Number of primitives in each bin
	int bins[NB_BINS];
	// Bbox of the primitives in the bin
	BBox binsBbox[NB_BINS];

	//--------------
	// Fill in the bins, considering all the primitives when a given
	// threshold is reached, else considering only a portion of the
	// primitives for the binned-SAH process. Also compute the bins bboxes
	// for the primitives. 

	for (u_int i = 0; i < NB_BINS; ++i)
		bins[i] = 0;

	const u_int step = (end - start < fullSweepThreshold) ? 1 : skipFactor;

	// Choose the split axis, taking the axis of maximum extent for the
	// centroids (else weird cases can occur, where the maximum extent axis
	// for the nodeBbox is an axis of 0 extent for the centroids one.).
	const int axis = centroidsBbox.MaximumExtent();

	// Create an intermediate node if the depth indicates to do so.
	// Register the split axis.
	if (depth % 2 == 0) {
		currentNode = CreateNode(parentIndex, childIndex, nodeBbox);
		leftChildIndex = 0;
		rightChildIndex = 2;
	}

	// Precompute values that are constant with respect to the current
	// primitive considered.
	const float k0 = centroidsBbox.pMin[axis];
	const float k1 = NB_BINS / (centroidsBbox.pMax[axis] - k0);

	if (k1 == INFINITY) {
		// All centroids are exactly the same. I will just store even primitives
		// on one side and odd on the other.

		LR_LOG(ctx, "MQBVH detects too many primitives with the same centroid");

		BBox leftChildBbox, rightChildBbox;
		BBox leftChildCentroidsBbox, rightChildCentroidsBbox;

		u_int storeIndex = start;
		for (u_int i = start; i < end; ++i) {
			u_int primIndex = primsIndexes[i];

			// Just store even primitives on one side and odd on the other
			if (i % 2) {
				// Swap
				primsIndexes[i] = primsIndexes[storeIndex];
				primsIndexes[storeIndex] = primIndex;
				++storeIndex;

				// Update the bounding boxes,
				// this object is on the left side
				leftChildBbox = Union(leftChildBbox, primsBboxes[primIndex]);
				leftChildCentroidsBbox = Union(leftChildCentroidsBbox, primsCentroids[primIndex]);
			} else {
				// Update the bounding boxes,
				// this object is on the right side.
				rightChildBbox = Union(rightChildBbox, primsBboxes[primIndex]);
				rightChildCentroidsBbox = Union(rightChildCentroidsBbox, primsCentroids[primIndex]);
			}
		}

		// Build recursively
		BuildTree(start, storeIndex, primsIndexes, primsBboxes, primsCentroids,
				leftChildBbox, leftChildCentroidsBbox, currentNode,
				leftChildIndex, depth + 1);
		BuildTree(storeIndex, end, primsIndexes, primsBboxes, primsCentroids,
				rightChildBbox, rightChildCentroidsBbox, currentNode,
				rightChildIndex, depth + 1);
		return;
	} else {
		for (u_int i = start; i < end; i += step) {
			u_int primIndex = primsIndexes[i];

			// Binning is relative to the centroids bbox and to the
			// primitives' centroid.
			const int binId = Min(NB_BINS - 1, Floor2Int(k1 * (primsCentroids[primIndex][axis] - k0)));

			bins[binId]++;
			binsBbox[binId] = Union(binsBbox[binId], primsBboxes[primIndex]);
		}
	}

	//--------------
	// Evaluate where to split.

	// Cumulative number of primitives in the bins from the first to the
	// ith, and from the last to the ith.
	int nbPrimsLeft[NB_BINS];
	int nbPrimsRight[NB_BINS];
	// The corresponding cumulative bounding boxes.
	BBox bboxesLeft[NB_BINS];
	BBox bboxesRight[NB_BINS];

	// The corresponding volumes.
	float vLeft[NB_BINS];
	float vRight[NB_BINS];

	BBox currentBboxLeft, currentBboxRight;
	int currentNbLeft = 0, currentNbRight = 0;

	for (int i = 0; i < NB_BINS; ++i) {
		//-----
		// Left side
		// Number of prims
		currentNbLeft += bins[i];
		nbPrimsLeft[i] = currentNbLeft;
		// Prims bbox
		currentBboxLeft = Union(currentBboxLeft, binsBbox[i]);
		bboxesLeft[i] = currentBboxLeft;
		// Surface area
		vLeft[i] = currentBboxLeft.SurfaceArea();


		//-----
		// Right side
		// Number of prims
		int rightIndex = NB_BINS - 1 - i;
		currentNbRight += bins[rightIndex];
		nbPrimsRight[rightIndex] = currentNbRight;
		// Prims bbox
		currentBboxRight = Union(currentBboxRight, binsBbox[rightIndex]);
		bboxesRight[rightIndex] = currentBboxRight;
		// Surface area
		vRight[rightIndex] = currentBboxRight.SurfaceArea();
	}

	int minBin = -1;
	float minCost = INFINITY;
	// Find the best split axis,
	// there must be at least a bin on the right side
	for (int i = 0; i < NB_BINS - 1; ++i) {
		float cost = vLeft[i] * nbPrimsLeft[i] +
				vRight[i + 1] * nbPrimsRight[i + 1];
		if (cost < minCost) {
			minBin = i;
			minCost = cost;
		}
	}

	//-----------------
	// Make the partition, in a "quicksort partitioning" way,
	// the pivot being the position of the split plane
	// (no more binId computation)
	// track also the bboxes (primitives and centroids)
	// for the left and right halves.

	// The split plane coordinate is the coordinate of the end of
	// the chosen bin along the split axis
	float splitPos = centroidsBbox.pMin[axis] + (minBin + 1) *
			(centroidsBbox.pMax[axis] - centroidsBbox.pMin[axis]) / NB_BINS;

	BBox leftChildBbox, rightChildBbox;
	BBox leftChildCentroidsBbox, rightChildCentroidsBbox;

	u_int storeIndex = start;
	for (u_int i = start; i < end; ++i) {
		u_int primIndex = primsIndexes[i];

		if (primsCentroids[primIndex][axis] <= splitPos) {
			// Swap
			primsIndexes[i] = primsIndexes[storeIndex];
			primsIndexes[storeIndex] = primIndex;
			++storeIndex;

			// Update the bounding boxes,
			// this object is on the left side
			leftChildBbox = Union(leftChildBbox, primsBboxes[primIndex]);
			leftChildCentroidsBbox = Union(leftChildCentroidsBbox, primsCentroids[primIndex]);
		} else {
			// Update the bounding boxes,
			// this object is on the right side.
			rightChildBbox = Union(rightChildBbox, primsBboxes[primIndex]);
			rightChildCentroidsBbox = Union(rightChildCentroidsBbox, primsCentroids[primIndex]);
		}
	}

	// Build recursively
	BuildTree(start, storeIndex, primsIndexes, primsBboxes, primsCentroids,
			leftChildBbox, leftChildCentroidsBbox, currentNode,
			leftChildIndex, depth + 1);
	BuildTree(storeIndex, end, primsIndexes, primsBboxes, primsCentroids,
			rightChildBbox, rightChildCentroidsBbox, currentNode,
			rightChildIndex, depth + 1);
}

int32_t  MQBVHAccel::CreateNode(int32_t parentIndex, int32_t childIndex,
	const BBox &nodeBbox) {
	const int32_t index = nNodes++; // increment after assignment
	if (nNodes >= maxNodes) {
		QBVHNode *newNodes = AllocAligned<QBVHNode>(2 * maxNodes);
		memcpy(newNodes, nodes, sizeof(QBVHNode) * maxNodes);
		for (u_int i = 0; i < maxNodes; ++i)
			newNodes[maxNodes + i] = QBVHNode();
		FreeAligned(nodes);
		nodes = newNodes;
		maxNodes *= 2;
	}

	if (parentIndex >= 0) {
		nodes[parentIndex].children[childIndex] = index;
		nodes[parentIndex].SetBBox(childIndex, nodeBbox);
	}

	return index;
}

void MQBVHAccel::CreateLeaf(int32_t parentIndex, int32_t childIndex,
		u_int start, const BBox &nodeBbox) {
	// The leaf is directly encoded in the intermediate node.
	if (parentIndex < 0) {
		// The entire tree is a leaf
		nNodes = 1;
		parentIndex = 0;
	}

	QBVHNode &node = nodes[parentIndex];
	node.SetBBox(childIndex, nodeBbox);
	node.InitializeLeaf(childIndex, 1, start);
}

bool MQBVHAccel::Intersect(const Ray *initialRay, RayHit *rayHit) const {
	Ray ray(*initialRay);
	rayHit->SetMiss();

	// Prepare the ray for intersection
	QuadRay ray4(ray);
	__m128 invDir[3];
	invDir[0] = _mm_set1_ps(1.f / ray.d.x);
	invDir[1] = _mm_set1_ps(1.f / ray.d.y);
	invDir[2] = _mm_set1_ps(1.f / ray.d.z);

	int signs[3];
	ray.GetDirectionSigns(signs);

	// Main loop
	int todoNode = 0; // the index in the stack
	int32_t nodeStack[64];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		// Leaves are identified by a negative index
		if (!QBVHNode::IsLeaf(nodeStack[todoNode])) {
			QBVHNode &node = nodes[nodeStack[todoNode]];
			--todoNode;
		
			// It is quite strange but checking here for empty nodes slows down the rendering
			const int32_t visit = node.BBoxIntersect(ray4, invDir, signs);

			switch (visit) {
				case (0x1 | 0x0 | 0x0 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					break;
				case (0x0 | 0x2 | 0x0 | 0x0):
					nodeStack[++todoNode] = node.children[1];
					break;
				case (0x1 | 0x2 | 0x0 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					break;
				case (0x0 | 0x0 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[2];
					break;
				case (0x1 | 0x0 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[2];
					break;
				case (0x0 | 0x2 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					break;
				case (0x1 | 0x2 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					break;

				case (0x0 | 0x0 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x0 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x0 | 0x2 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x2 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x0 | 0x0 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x0 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x0 | 0x2 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x2 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
			}
		} else {
			// It is a leaf,
			// all the informations are encoded in the index
			const int32_t leafData = nodeStack[todoNode];
			--todoNode;

			if (QBVHNode::IsEmpty(leafData))
				continue;

			const u_int leafIndex = QBVHNode::FirstQuadIndex(leafData);
			QBVHAccel *qbvh = leafs[leafIndex];

			Ray localRay;
			if (leafsTransform[leafIndex])
				localRay = Ray(Inverse(*leafsTransform[leafIndex]) * ray);
			else if (leafsMotionSystem[leafIndex])
				localRay = Ray(leafsMotionSystem[leafIndex]->Sample(ray.time) * ray);
			else
				localRay = ray;

			RayHit rh;
			if (qbvh->Intersect(&localRay, &rh)) {
				rayHit->t = rh.t;
				rayHit->b1 = rh.b1;
				rayHit->b2 = rh.b2;
				rayHit->meshIndex = leafIndex;
				rayHit->triangleIndex = rh.triangleIndex;

				ray.maxt = rh.t;
			}
		}
	}

	return !rayHit->Miss();
}

}
