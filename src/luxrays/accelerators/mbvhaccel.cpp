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

// Boundary Volume Hierarchy accelerator
// Based of "Efficiency Issues for Ray Tracing" by Brian Smits
// Available at http://www.cs.utah.edu/~bes/papers/fastRT/paper.html

#include <iostream>
#include <functional>
#include <algorithm>
#include <limits>
#include <boost/foreach.hpp>

#include "luxrays/accelerators/mbvhaccel.h"
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

class OpenCLMBVHKernels : public OpenCLKernels {
public:
	OpenCLMBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const MBVHAccel *ac) :
			OpenCLKernels(dev, kernelCount), mbvh(ac),
			uniqueLeafsTransformBuff(NULL), uniqueLeafsMotionSystemBuff(NULL),
			uniqueLeafsInterpolatedTransformBuff(NULL) {
		const Context *deviceContext = device->GetContext();
		const std::string &deviceName(device->GetName());
		cl::Context &oclContext = device->GetOpenCLContext();
		cl::Device &oclDevice = device->GetOpenCLDevice();

		size_t maxNodeCount = 0;
		u_int pageNodeCount = 0;
		if (mbvh->nRootNodes) {
			// Check the max. number of vertices I can store in a single page
			const size_t maxMemAlloc = device->GetDeviceDesc()->GetMaxMemoryAllocSize();

			//------------------------------------------------------------------
			// Allocate vertex buffers
			//------------------------------------------------------------------

			// Check how many pages I have to allocate
			const size_t maxVertCount = maxMemAlloc / sizeof(Point);
			std::vector<std::vector<u_int> > vertOffsetPerLeafMesh;
			vertOffsetPerLeafMesh.resize(mbvh->uniqueLeafs.size());
			u_int totalVertCount = 0;
			for (u_int i = 0; i < mbvh->uniqueLeafs.size(); ++i) {
				const BVHAccel *leaf = mbvh->uniqueLeafs[i];

				for (u_int j = 0; j < leaf->meshes.size(); ++j) {
					vertOffsetPerLeafMesh[i].push_back(totalVertCount);
					totalVertCount += leaf->meshes[j]->GetTotalVertexCount();
				}
			}
			// Allocate a temporary buffer for the copy of the BVH vertices
			const u_int pageVertCount = Min<size_t>(totalVertCount, maxVertCount);
			Point *tmpVerts = new Point[pageVertCount];
			u_int tmpVertIndex = 0;

			u_int currentLeafIndex = 0;
			u_int currentMeshIndex = 0;
			u_int currentMeshVertIndex = 0;

			while (currentLeafIndex < mbvh->uniqueLeafs.size()) {
				const u_int tmpLeftVertCount = pageVertCount - tmpVertIndex;

				// Check if there is enough space in the temporary buffer for all vertices
				const Mesh *currentMesh = mbvh->uniqueLeafs[currentLeafIndex]->meshes[currentMeshIndex];
				const u_int toCopy = currentMesh->GetTotalVertexCount() - currentMeshVertIndex;
				if (tmpLeftVertCount >= toCopy) {
					// There is enough space for all mesh vertices
					memcpy(&tmpVerts[tmpVertIndex], &(currentMesh->GetVertices()[currentMeshVertIndex]),
							sizeof(Point) * toCopy);
					tmpVertIndex += toCopy;

					// Move to the next mesh
					++currentMeshIndex;
					if (currentMeshIndex >= mbvh->uniqueLeafs[currentLeafIndex]->meshes.size()) {
						// Move to the next leaf
						++currentLeafIndex;
						currentMeshIndex = 0;
					}
					currentMeshVertIndex = 0;
				} else {
					// There isn't enough space for all mesh vertices. Fill the current buffer.
					memcpy(&tmpVerts[tmpVertIndex], &(currentMesh->GetVertices()[currentMeshVertIndex]),
							sizeof(Point) * tmpLeftVertCount);

					tmpVertIndex += tmpLeftVertCount;
					currentMeshVertIndex += tmpLeftVertCount;
				}

				if ((tmpVertIndex >= pageVertCount) || (currentLeafIndex >=  mbvh->uniqueLeafs.size())) {
					// The temporary buffer is full, send the data to the OpenCL device
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
							"] Vertices buffer size (Page " << vertsBuffs.size() <<", " <<
							tmpVertIndex << " vertices): " <<
							(sizeof(Point) * pageVertCount / 1024) <<
							"Kbytes");
					cl::Buffer *vb = new cl::Buffer(oclContext,
						CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
						sizeof(Point) * tmpVertIndex,
						(void *)&tmpVerts[0]);
					device->AllocMemory(vb->getInfo<CL_MEM_SIZE>());
					vertsBuffs.push_back(vb);
					if (vertsBuffs.size() > 8)
						throw std::runtime_error("Too many vertex pages required in OpenCLMBVHKernels()");

					tmpVertIndex = 0;
				}
			}
			delete[] tmpVerts;

			//------------------------------------------------------------------
			// Allocate BVH node buffers
			//------------------------------------------------------------------

			// Check how many pages I have to allocate
			maxNodeCount = maxMemAlloc / sizeof(luxrays::ocl::BVHAccelArrayNode);

			u_int totalNodeCount = mbvh->nRootNodes;
			for (u_int i = 0; i < mbvh->uniqueLeafs.size(); ++i)
				totalNodeCount += mbvh->uniqueLeafs[i]->nNodes;

			pageNodeCount = Min<size_t>(totalNodeCount, maxNodeCount);

			// Initialize the node offset vector
			std::vector<u_int> leafNodeOffset;
			u_int currentBVHNodeCount = mbvh->nRootNodes;
			for (u_int i = 0; i < mbvh->uniqueLeafs.size(); ++i) {
				const BVHAccel *leafBVH = mbvh->uniqueLeafs[i];
				leafNodeOffset.push_back(currentBVHNodeCount);
				currentBVHNodeCount += leafBVH->nNodes;
			}

			// Allocate a temporary buffer for the copy of the BVH nodes
			luxrays::ocl::BVHAccelArrayNode *tmpNodes = new luxrays::ocl::BVHAccelArrayNode[pageNodeCount];
			u_int tmpNodeIndex = 0;

			u_int currentNodeIndex = 0;
			currentLeafIndex = 0;
			const luxrays::ocl::BVHAccelArrayNode *currentNodes = mbvh->bvhRootTree;
			u_int currentNodesCount = mbvh->nRootNodes;

			while (currentLeafIndex < mbvh->uniqueLeafs.size()) {
				const u_int tmpLeftNodeCount = pageNodeCount - tmpNodeIndex;
				const bool isRootTree = (currentNodes == mbvh->bvhRootTree);
				const u_int leafIndex = currentLeafIndex;

				// Check if there is enough space in the temporary buffer for all nodes
				u_int copiedIndexStart, copiedIndexEnd;
				const u_int toCopy = currentNodesCount - currentNodeIndex;
				if (tmpLeftNodeCount >= toCopy) {
					// There is enough space for all nodes
					memcpy(&tmpNodes[tmpNodeIndex], &currentNodes[currentNodeIndex],
							sizeof(luxrays::ocl::BVHAccelArrayNode) * toCopy);
					copiedIndexStart = tmpNodeIndex;
					copiedIndexEnd = tmpNodeIndex + toCopy;

					tmpNodeIndex += toCopy;
					// Move to the next leaf tree
					if (isRootTree) {
						// Move from the root nodes to the first leaf node. currentLeafIndex
						// is already 0
					} else
						++currentLeafIndex;

					if (currentLeafIndex < mbvh->uniqueLeafs.size()) {
						currentNodes = mbvh->uniqueLeafs[currentLeafIndex]->bvhTree;
						currentNodesCount = mbvh->uniqueLeafs[currentLeafIndex]->nNodes;
						currentNodeIndex = 0;
					}
				} else {
					// There isn't enough space for all mesh vertices. Fill the current buffer.
					memcpy(&tmpNodes[tmpNodeIndex], &currentNodes[currentNodeIndex],
							sizeof(luxrays::ocl::BVHAccelArrayNode) * tmpLeftNodeCount);
					copiedIndexStart = tmpNodeIndex;
					copiedIndexEnd = tmpNodeIndex + tmpLeftNodeCount;

					tmpNodeIndex += tmpLeftNodeCount;
					currentNodeIndex += tmpLeftNodeCount;
				}

				// Update the vertex references
				for (u_int i = copiedIndexStart; i < copiedIndexEnd; ++i) {
					luxrays::ocl::BVHAccelArrayNode *node = &tmpNodes[i];

					if (isRootTree) {
						// I'm handling the root nodes
						if (BVHNodeData_IsLeaf(node->nodeData)) {
							const u_int nextNodeIndex = leafNodeOffset[node->bvhLeaf.leafIndex];
							const u_int nodePage = nextNodeIndex / maxNodeCount;
							const u_int nodeIndex = nextNodeIndex % maxNodeCount;
							// Encode the page in 30, 29, 28 bits of the node index (last
							// bit is used to encode if it is a leaf or not)
							node->bvhLeaf.leafIndex = (nodePage << 28) | nodeIndex;
						} else {
							const u_int nextNodeIndex = BVHNodeData_GetSkipIndex(node->nodeData);
							const u_int nodePage = nextNodeIndex / maxNodeCount;
							const u_int nodeIndex = nextNodeIndex % maxNodeCount;
							// Encode the page in 30, 29, 28 bits of the node index (last
							// bit is used to encode if it is a leaf or not)
							node->nodeData = (nodePage << 28) | nodeIndex;
						}
					} else {
						// I'm handling a leaf nodes
						if (BVHNodeData_IsLeaf(node->nodeData)) {
							// Update the vertex references
							for (u_int j = 0; j < 3; ++j) {
								const u_int vertIndex = node->triangleLeaf.v[j] + vertOffsetPerLeafMesh[leafIndex][node->triangleLeaf.meshIndex];
								const u_int vertexPage = vertIndex / maxVertCount;
								const u_int vertexIndex = vertIndex % maxVertCount;
								// Encode the page in the last 3 bits of the vertex index
								node->triangleLeaf.v[j] = (vertexPage << 29) | vertexIndex;
							}
						} else {
							const u_int nextNodeIndex = BVHNodeData_GetSkipIndex(node->nodeData) + leafNodeOffset[leafIndex];
							const u_int nodePage = nextNodeIndex / maxNodeCount;
							const u_int nodeIndex = nextNodeIndex % maxNodeCount;
							// Encode the page in 30, 29, 28 bits of the node index (last
							// bit is used to encode if it is a leaf or not)
							node->nodeData = (nodePage << 28) | nodeIndex;
						}
					}
				}

				if ((tmpNodeIndex >= pageNodeCount) || (currentLeafIndex >=  mbvh->uniqueLeafs.size())) {
					// The temporary buffer is full, send the data to the OpenCL device
					LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
						"] MBVH node buffer size (Page " << nodeBuffs.size() <<", " <<
						tmpNodeIndex << " nodes): " <<
						(sizeof(luxrays::ocl::BVHAccelArrayNode) * totalNodeCount / 1024) <<
						"Kbytes");
					cl::Buffer *nb = new cl::Buffer(oclContext,
						CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
						sizeof(luxrays::ocl::BVHAccelArrayNode) * tmpNodeIndex,
						(void *)&tmpNodes[0]);
					device->AllocMemory(nb->getInfo<CL_MEM_SIZE>());
					nodeBuffs.push_back(nb);
					if (nodeBuffs.size() > 8)
						throw std::runtime_error("Too many BVH node pages required in OpenCLMBVHKernels()");

					tmpNodeIndex = 0;
				}
			}
			delete[] tmpNodes;
		}

		//----------------------------------------------------------------------
		// Allocate leaf transformations
		//----------------------------------------------------------------------

		if (mbvh->uniqueLeafsTransform.size() > 0) {
			// Transform CPU data structures in OpenCL data structures
			vector<Matrix4x4> mats;
			BOOST_FOREACH(const Transform *t, mbvh->uniqueLeafsTransform)
				mats.push_back(t->mInv);

			// Allocate the transformation buffer
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] Leaf transformations buffer size: " <<
				(sizeof(luxrays::ocl::Matrix4x4) * mats.size() / 1024) <<
				"Kbytes");
			uniqueLeafsTransformBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(luxrays::ocl::Matrix4x4) * mats.size(),
				(void *)&(mats[0]));
			device->AllocMemory(uniqueLeafsTransformBuff->getInfo<CL_MEM_SIZE>());
		}

		if (mbvh->uniqueLeafsMotionSystem.size() > 0) {
			// Transform CPU data structures in OpenCL data structures
			
			vector<luxrays::ocl::MotionSystem> motionSystems;
			vector<luxrays::ocl::InterpolatedTransform> interpolatedTransforms;
			BOOST_FOREACH(const MotionSystem *ms, mbvh->uniqueLeafsMotionSystem) {
				luxrays::ocl::MotionSystem oclMotionSystem;

				oclMotionSystem.interpolatedTransformFirstIndex = interpolatedTransforms.size();
				BOOST_FOREACH(const InterpolatedTransform &it, ms->interpolatedTransforms) {
					// Here, I assume that luxrays::ocl::InterpolatedTransform and
					// luxrays::InterpolatedTransform are the same
					interpolatedTransforms.push_back(*((const luxrays::ocl::InterpolatedTransform *)&it));
				}
				oclMotionSystem.interpolatedTransformLastIndex = interpolatedTransforms.size() - 1;
				
				motionSystems.push_back(oclMotionSystem);
			}

			// Allocate the motion system buffer
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] Leaf motion systems buffer size: " <<
				(sizeof(luxrays::ocl::MotionSystem) * motionSystems.size() / 1024) <<
				"Kbytes");
			uniqueLeafsMotionSystemBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(luxrays::ocl::MotionSystem) * motionSystems.size(),
				(void *)&(motionSystems[0]));
			device->AllocMemory(uniqueLeafsMotionSystemBuff->getInfo<CL_MEM_SIZE>());

			// Allocate the InterpolatedTransform buffer
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] Leaf interpolated transforms buffer size: " <<
				(sizeof(luxrays::ocl::InterpolatedTransform) * interpolatedTransforms.size() / 1024) <<
				"Kbytes");
			uniqueLeafsInterpolatedTransformBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(luxrays::ocl::InterpolatedTransform) * interpolatedTransforms.size(),
				(void *)&(interpolatedTransforms[0]));
			device->AllocMemory(uniqueLeafsInterpolatedTransformBuff->getInfo<CL_MEM_SIZE>());
		}

		//----------------------------------------------------------------------
		// Compile kernel sources
		//----------------------------------------------------------------------

		// Compile options
		std::stringstream opts;
		opts << " -D LUXRAYS_OPENCL_KERNEL"
				" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
				" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MBVH compile options: \n" << opts.str());

		std::stringstream kernelDefs;
		kernelDefs << "#define MBVH_VERTS_PAGE_COUNT " << vertsBuffs.size() << "\n"
				"#define MBVH_NODES_PAGE_COUNT " << nodeBuffs.size() << "\n";
		if (vertsBuffs.size() > 1) {
			// MBVH_NODES_PAGE_SIZE is used only if MBVH_VERTS_PAGE_COUNT > 1
			// I conditional define this value to avoid kernel recompilation
			kernelDefs << "#define MBVH_NODES_PAGE_SIZE " << pageNodeCount << "\n";
		}
		for (u_int i = 0; i < vertsBuffs.size(); ++i)
			kernelDefs << "#define MBVH_VERTS_PAGE" << i << " 1\n";
		for (u_int i = 0; i < nodeBuffs.size(); ++i)
			kernelDefs << "#define MBVH_NODES_PAGE" << i << " 1\n";
		if (uniqueLeafsTransformBuff)
			kernelDefs << "#define MBVH_HAS_TRANSFORMATIONS 1\n";
		if (uniqueLeafsMotionSystemBuff)
			kernelDefs << "#define MBVH_HAS_MOTIONSYSTEMS 1\n";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MBVH kernel definitions: \n" << kernelDefs.str());

		intersectionKernelSource = kernelDefs.str() +
				luxrays::ocl::KernelSource_bvh_types +
				luxrays::ocl::KernelSource_mbvh;

		const std::string code(
			kernelDefs.str() +
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
			luxrays::ocl::KernelSource_triangle_types +
			luxrays::ocl::KernelSource_triangle_funcs +
			luxrays::ocl::KernelSource_bvh_types +
			luxrays::ocl::KernelSource_mbvh);
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error &err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MBVH compilation error:\n" << strError.c_str());

			throw err;
		}

		// Setup kernels
		for (u_int i = 0; i < kernelCount; ++i) {
			kernels[i] = new cl::Kernel(program, "Accelerator_Intersect_RayBuffer");

			if (device->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
				workGroupSize = device->GetDeviceDesc()->GetForceWorkGroupSize();
			else {
				kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
					CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
				//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				//	"] MBVH kernel work group size: " << workGroupSize);
			}

			// Set arguments
			SetIntersectionKernelArgs(*(kernels[i]), 3);
		}
	}

	virtual ~OpenCLMBVHKernels() {
		BOOST_FOREACH(cl::Buffer *buf, vertsBuffs) {
			device->FreeMemory(buf->getInfo<CL_MEM_SIZE>());
			delete buf;
		}
		BOOST_FOREACH(cl::Buffer *buf, nodeBuffs) {
			device->FreeMemory(buf->getInfo<CL_MEM_SIZE>());
			delete buf;
		}
		if (uniqueLeafsTransformBuff) {
			device->FreeMemory(uniqueLeafsTransformBuff->getInfo<CL_MEM_SIZE>());
			delete uniqueLeafsTransformBuff;
		}
		if (uniqueLeafsMotionSystemBuff) {
			device->FreeMemory(uniqueLeafsMotionSystemBuff->getInfo<CL_MEM_SIZE>());
			delete uniqueLeafsMotionSystemBuff;
		}
		if (uniqueLeafsInterpolatedTransformBuff) {
			device->FreeMemory(uniqueLeafsInterpolatedTransformBuff->getInfo<CL_MEM_SIZE>());
			delete uniqueLeafsInterpolatedTransformBuff;
		}
	}

	virtual void Update(const DataSet *newDataSet);
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex);

	const MBVHAccel *mbvh;

	// BVH fields
	vector<cl::Buffer *> vertsBuffs;
	vector<cl::Buffer *> nodeBuffs;
	cl::Buffer *uniqueLeafsTransformBuff;
	cl::Buffer *uniqueLeafsMotionSystemBuff;
	cl::Buffer *uniqueLeafsInterpolatedTransformBuff;
};

void OpenCLMBVHKernels::Update(const DataSet *newDataSet) {
	const Context *deviceContext = device->GetContext();
	const std::string &deviceName = device->GetName();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Updating DataSet transformations");

	// Transform CPU data structures in OpenCL data structures
	vector<Matrix4x4> mats;
	mats.reserve(mbvh->uniqueLeafsTransform.size());
	BOOST_FOREACH(const Transform *t, mbvh->uniqueLeafsTransform)
		mats.push_back(t->mInv);

	device->GetOpenCLQueue().enqueueWriteBuffer(
		*uniqueLeafsTransformBuff,
		CL_TRUE,
		0,
		sizeof(luxrays::ocl::Matrix4x4) * mats.size(),
		&mats[0]);
}

void OpenCLMBVHKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
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

u_int OpenCLMBVHKernels::SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int index) {
	u_int argIndex = index;
	if (uniqueLeafsTransformBuff)
		kernel.setArg(argIndex++, *uniqueLeafsTransformBuff);
	if (uniqueLeafsMotionSystemBuff) {
		kernel.setArg(argIndex++, *uniqueLeafsMotionSystemBuff);
		kernel.setArg(argIndex++, *uniqueLeafsInterpolatedTransformBuff);
	}
	for (u_int i = 0; i < vertsBuffs.size(); ++i)
		kernel.setArg(argIndex++, *vertsBuffs[i]);
	for (u_int i = 0; i < nodeBuffs.size(); ++i)
		kernel.setArg(argIndex++, *nodeBuffs[i]);

	return argIndex;
}

OpenCLKernels *MBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const {
	// Setup kernels
	return new OpenCLMBVHKernels(device, kernelCount, this);
}

#else

OpenCLKernels *MBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const {
	return NULL;
}

#endif

// MBVHAccel Method Definitions

MBVHAccel::MBVHAccel(const Context *context) : ctx(context) {
	params = BVHAccel::ToBVHParams(ctx->GetConfig());

	initialized = false;
}

MBVHAccel::~MBVHAccel() {
	if (initialized) {
		BOOST_FOREACH(const BVHAccel *bvh, uniqueLeafs)
			delete bvh;
		delete bvhRootTree;
	}
}

bool MBVHAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

void MBVHAccel::Init(const std::deque<const Mesh *> &ms, const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	assert (!initialized);

	// Handle the empty DataSet case
	if (totalTriangleCount == 0) {
		LR_LOG(ctx, "Empty MBVH");
		nRootNodes = 0;
		bvhRootTree = NULL;
		initialized = true;

		return;
	}

	meshes = ms;

	const double t0 = WallClockTime();

	//--------------------------------------------------------------------------
	// Build all BVH leafs
	//--------------------------------------------------------------------------

	const u_int nLeafs = meshes.size();
	LR_LOG(ctx, "Building Multilevel Bounding Volume Hierarchy, leafs: " << nLeafs);

	std::vector<u_int> leafsIndex;
	std::vector<u_int> leafsTransformIndex;
	std::vector<u_int> leafsMotionSystemIndex;

	leafsIndex.reserve(nLeafs);

	std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)> uniqueLeafIndexByMesh(MeshPtrCompare);

	double lastPrint = WallClockTime();
	for (u_int i = 0; i < nLeafs; ++i) {
		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			LR_LOG(ctx, "Building BVH for MBVH leaf: " << i << "/" << nLeafs);
			lastPrint = now;
		}

		const Mesh *mesh = meshes[i];

		switch (mesh->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE: {
				BVHAccel *leaf = new BVHAccel(ctx);
				std::deque<const Mesh *> mlist(1, mesh);
				leaf->Init(mlist, mesh->GetTotalVertexCount(), mesh->GetTotalTriangleCount());

				const u_int uniqueLeafIndex = uniqueLeafs.size();
				uniqueLeafIndexByMesh[mesh] = uniqueLeafIndex;
				uniqueLeafs.push_back(leaf);
				leafsIndex.push_back(uniqueLeafIndex);
				leafsTransformIndex.push_back(NULL_INDEX);
				leafsMotionSystemIndex.push_back(NULL_INDEX);
				break;
			}
			case TYPE_TRIANGLE_INSTANCE:
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(mesh);

				// Check if a BVH has already been created
				std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it =
						uniqueLeafIndexByMesh.find(itm->GetTriangleMesh());

				if (it == uniqueLeafIndexByMesh.end()) {
					TriangleMesh *instancedMesh = itm->GetTriangleMesh();

					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx);
					std::deque<const Mesh *> mlist(1, instancedMesh);
					leaf->Init(mlist, instancedMesh->GetTotalVertexCount(), instancedMesh->GetTotalTriangleCount());

					const u_int uniqueLeafIndex = uniqueLeafs.size();
					uniqueLeafIndexByMesh[instancedMesh] = uniqueLeafIndex;
					uniqueLeafs.push_back(leaf);
					leafsIndex.push_back(uniqueLeafIndex);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafsIndex.push_back(it->second);
				}

				leafsTransformIndex.push_back(uniqueLeafsTransform.size());
				uniqueLeafsTransform.push_back(&itm->GetTransformation());
				leafsMotionSystemIndex.push_back(NULL_INDEX);
				break;
			}
			case TYPE_TRIANGLE_MOTION:
			case TYPE_EXT_TRIANGLE_MOTION: {
				const MotionTriangleMesh *mtm = dynamic_cast<const MotionTriangleMesh *>(mesh);

				// Check if a BVH has already been created
				std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it =
						uniqueLeafIndexByMesh.find(mtm->GetTriangleMesh());

				if (it == uniqueLeafIndexByMesh.end()) {
					TriangleMesh *motionMesh = mtm->GetTriangleMesh();

					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx);
					std::deque<const Mesh *> mlist(1, motionMesh);
					leaf->Init(mlist, motionMesh->GetTotalVertexCount(), motionMesh->GetTotalTriangleCount());

					const u_int uniqueLeafIndex = uniqueLeafs.size();
					uniqueLeafIndexByMesh[motionMesh] = uniqueLeafIndex;
					uniqueLeafs.push_back(leaf);
					leafsIndex.push_back(uniqueLeafIndex);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafsIndex.push_back(it->second);
				}

				leafsMotionSystemIndex.push_back(uniqueLeafsMotionSystem.size());
				uniqueLeafsMotionSystem.push_back(&mtm->GetMotionSystem());
				leafsTransformIndex.push_back(NULL_INDEX);
				break;
			}
			default:
				throw std::runtime_error("Unknown Mesh type in MBVHAccel::Init(): " + ToString(mesh->GetType()));
		}
	}

	//--------------------------------------------------------------------------
	// Build the root BVH
	//--------------------------------------------------------------------------

	std::vector<BVHTreeNode> bvNodes(nLeafs);
	std::vector<BVHTreeNode *> bvList(nLeafs, NULL);
	for (u_int i = 0; i < nLeafs; ++i) {
		BVHTreeNode *node = &bvNodes[i];
		// Get the bounding box from the mesh so it is in global coordinates
		node->bbox = meshes[i]->GetBBox();
		node->bvhLeaf.leafIndex = leafsIndex[i];
		node->bvhLeaf.transformIndex = leafsTransformIndex[i];
		node->bvhLeaf.motionIndex = leafsMotionSystemIndex[i];
		node->bvhLeaf.meshOffsetIndex = i;
		node->leftChild = NULL;
		node->rightSibling = NULL;
		bvList[i] = node;
	}

	nRootNodes = 0;
	BVHTreeNode *rootNode = BuildBVH(&nRootNodes, params, bvList);

	LR_LOG(ctx, "Pre-processing Multilevel Bounding Volume Hierarchy, total nodes: " << nRootNodes);

	bvhRootTree = new luxrays::ocl::BVHAccelArrayNode[nRootNodes];
	BVHAccel::BuildArray(NULL, rootNode, 0, bvhRootTree);
	FreeBVH(rootNode);

	size_t totalMem = nRootNodes;
	BOOST_FOREACH(const BVHAccel *bvh, uniqueLeafs)
		totalMem += bvh->nNodes;
	totalMem *= sizeof(luxrays::ocl::BVHAccelArrayNode);
	LR_LOG(ctx, "Total Multilevel BVH memory usage: " << totalMem / 1024 << "Kbytes");
	//LR_LOG(ctx, "Finished building Multilevel Bounding Volume Hierarchy array");

	LR_LOG(ctx, "MBVH build time: " << int((WallClockTime() - t0) * 1000) << "ms");

	initialized = true;
}

void MBVHAccel::Update() {
	// Nothing to do because uniqueLeafsTransform is a list of pointers
}

bool MBVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
	assert (initialized);

	rayHit->t = ray->maxt;
	rayHit->SetMiss();
	if (!nRootNodes)
		return false;

	bool insideLeafTree = false;
	u_int currentRootNode = 0;
	const u_int rootStopNode = BVHNodeData_GetSkipIndex(bvhRootTree[0].nodeData); // Non-existent
	u_int currentNode = currentRootNode;
	u_int currentStopNode = rootStopNode; // Non-existent
	u_int currentMeshOffset = 0;
	luxrays::ocl::BVHAccelArrayNode *currentTree = bvhRootTree;

	Ray currentRay(*ray);

	for (;;) {
		if (currentNode >= currentStopNode) {
			if (insideLeafTree) {
				// Go back to the root tree
				currentTree = bvhRootTree;
				currentNode = currentRootNode;
				currentStopNode = rootStopNode;
				currentRay = *ray;
				currentRay.maxt = rayHit->t;
				insideLeafTree = false;

				// Check if the leaf was the very last root node
				if (currentNode >= currentStopNode)
					break;
			} else {
				// Done
				break;
			}
		}

		const luxrays::ocl::BVHAccelArrayNode &node = currentTree[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			if (insideLeafTree) {
				// I'm inside a leaf tree, I have to check the triangle
				const u_int absoluteMeshIndex = node.triangleLeaf.meshIndex + currentMeshOffset;
				const Mesh *currentMesh = meshes[absoluteMeshIndex];
				// I use currentMesh->GetVertices() in order to have access to not
				// transformed vertices in the case of instances
				const Point *vertices = currentMesh->GetVertices();
				const Point &p0 = vertices[node.triangleLeaf.v[0]];
				const Point &p1 = vertices[node.triangleLeaf.v[1]];
				const Point &p2 = vertices[node.triangleLeaf.v[2]];

				float t, b1, b2;
				if (Triangle::Intersect(currentRay, p0, p1, p2, &t, &b1, &b2)) {
					if (t < rayHit->t) {
						currentRay.maxt = t;
						rayHit->t = t;
						rayHit->b1 = b1;
						rayHit->b2 = b2;
						rayHit->meshIndex = absoluteMeshIndex;
						rayHit->triangleIndex = node.triangleLeaf.triangleIndex;
						// Continue testing for closer intersections
					}
				}

				++currentNode;
			} else {
				// I have to check a leaf tree
				currentTree = uniqueLeafs[node.bvhLeaf.leafIndex]->bvhTree;

				// Transform the ray in the local coordinate system
				if (node.bvhLeaf.transformIndex != NULL_INDEX)
					currentRay = Ray(Inverse(*uniqueLeafsTransform[node.bvhLeaf.transformIndex]) * (*ray));
				else if (node.bvhLeaf.motionIndex != NULL_INDEX)
					currentRay = Ray(uniqueLeafsMotionSystem[node.bvhLeaf.motionIndex]->Sample(ray->time) * (*ray));
				else
					currentRay = (*ray);

				currentRay.maxt = rayHit->t;

				currentMeshOffset = node.bvhLeaf.meshOffsetIndex;

				currentRootNode = currentNode + 1;
				currentNode = 0;
				currentStopNode = BVHNodeData_GetSkipIndex(currentTree[0].nodeData);

				// Now, I'm inside a leaf tree
				insideLeafTree = true;
			}
		} else {
			// It is a node, check the bounding box
			if (BBox::IntersectP(currentRay,
					*reinterpret_cast<const Point *>(&node.bvhNode.bboxMin[0]),
					*reinterpret_cast<const Point *>(&node.bvhNode.bboxMax[0])))
				++currentNode;
			else {
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return !rayHit->Miss();
}

}
