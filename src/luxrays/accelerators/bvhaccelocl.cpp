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

#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"
#endif

using namespace std;

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLBVHKernels : public OpenCLKernels {
public:
	OpenCLBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const BVHAccel *bvh) :
		OpenCLKernels(dev, kernelCount) {
		const Context *deviceContext = device->GetContext();
		const string &deviceName(device->GetName());
		cl::Context &oclContext = device->GetOpenCLContext();
		cl::Device &oclDevice = device->GetOpenCLDevice();

		size_t maxNodeCount = 0;
		if (bvh->nNodes) {
			// Check the max. number of vertices I can store in a single page
			const size_t maxMemAlloc = device->GetDeviceDesc()->GetMaxMemoryAllocSize();

			//------------------------------------------------------------------
			// Allocate vertex buffers
			//------------------------------------------------------------------

			// Check how many pages I have to allocate
			const size_t maxVertCount = maxMemAlloc / sizeof(Point);
			const u_int totalVertCount = bvh->totalVertexCount;

			// Allocate the temporary vertex buffer
			Point *tmpVerts = new Point[Min<size_t>(totalVertCount, maxVertCount)];
			deque<const Mesh *>::const_iterator mesh = bvh->meshes.begin();

			u_int vertsCopied = 0;
			u_int meshVertIndex = 0;
			vector<u_int> meshVertexOffsets;
			meshVertexOffsets.reserve(bvh->meshes.size());
			meshVertexOffsets.push_back(0);
			do {
				const u_int leftVertCount = totalVertCount - vertsCopied;
				const u_int pageVertCount = Min<size_t>(leftVertCount, maxVertCount);

				// Fill the temporary vertex buffer
				u_int meshVertCount = (*mesh)->GetTotalVertexCount();
				for (u_int i = 0; i < pageVertCount; ++i) {
					if (meshVertIndex >= meshVertCount) {
						// Move to the next mesh
						if (++mesh == bvh->meshes.end())
							break;

						meshVertexOffsets.push_back(vertsCopied);

						meshVertCount = (*mesh)->GetTotalVertexCount();
						meshVertIndex = 0;
					}
					tmpVerts[i] = (*mesh)->GetVertex(0.f, meshVertIndex++);
					++vertsCopied;
				}

				// Allocate the OpenCL buffer
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
					"] Vertices buffer size (Page " << vertsBuffs.size() <<", " <<
					pageVertCount << " vertices): " <<
					(sizeof(Point) * pageVertCount / 1024) <<
					"Kbytes");
				cl::Buffer *vb = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(Point) * pageVertCount,
					(void *)tmpVerts);
				device->AllocMemory(vb->getInfo<CL_MEM_SIZE>());
				vertsBuffs.push_back(vb);

				if (vertsBuffs.size() > 8)
					throw runtime_error("Too many vertex pages required in OpenCLBVHKernels()");
			} while (vertsCopied < totalVertCount);
			delete[] tmpVerts;

			//------------------------------------------------------------------
			// Allocate BVH node buffers
			//------------------------------------------------------------------

			// Check how many pages I have to allocate
			maxNodeCount = maxMemAlloc / sizeof(luxrays::ocl::BVHAccelArrayNode);
			const u_int totalNodeCount = bvh->nNodes;
			const luxrays::ocl::BVHAccelArrayNode *nodes = bvh->bvhTree;
			// Allocate a temporary buffer for the copy of the BVH nodes
			luxrays::ocl::BVHAccelArrayNode *tmpNodes = new luxrays::ocl::BVHAccelArrayNode[Min<size_t>(bvh->nNodes, maxNodeCount)];
			u_int nodeIndex = 0;

			do {
				const u_int leftNodeCount = totalNodeCount - nodeIndex;
				const u_int pageNodeCount = Min<size_t>(leftNodeCount, maxNodeCount);

				// Make a copy of the nodes
				memcpy(tmpNodes, &nodes[nodeIndex], sizeof(luxrays::ocl::BVHAccelArrayNode) * pageNodeCount);

				// Update the vertex and node references
				for (u_int i = 0; i < pageNodeCount; ++i) {
					luxrays::ocl::BVHAccelArrayNode *node = &tmpNodes[i];
					if (BVHNodeData_IsLeaf(node->nodeData)) {
						// Update the vertex references
						for (u_int j = 0; j < 3; ++j) {
							const u_int vertexGlobalIndex = node->triangleLeaf.v[j] + meshVertexOffsets[node->triangleLeaf.meshIndex];
							const u_int vertexPage = vertexGlobalIndex / maxVertCount;
							const u_int vertexIndex = vertexGlobalIndex % maxVertCount;
							// Encode the page in the last 3 bits of the vertex index
							node->triangleLeaf.v[j] = (vertexPage << 29) | vertexIndex;
						}
					} else {
						const u_int nextNodeIndex = BVHNodeData_GetSkipIndex(node->nodeData);
						const u_int nodePage = nextNodeIndex / maxNodeCount;
						const u_int nodeIndex = nextNodeIndex % maxNodeCount;
						// Encode the page in 30, 29, 28 bits of the node index (last
						// bit is used to encode if it is a leaf or not)
						node->nodeData = (nodePage << 28) | nodeIndex;
					}
				}

				// Allocate OpenCL buffer
				LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
					"] BVH buffer size (Page " << nodeBuffs.size() <<", " <<
					pageNodeCount << " nodes): " <<
					(sizeof(luxrays::ocl::BVHAccelArrayNode) * pageNodeCount / 1024) <<
					"Kbytes");
				cl::Buffer *bb = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(luxrays::ocl::BVHAccelArrayNode) * pageNodeCount,
					(void *)tmpNodes);
				device->AllocMemory(bb->getInfo<CL_MEM_SIZE>());
				nodeBuffs.push_back(bb);

				if (nodeBuffs.size() > 8)
					throw runtime_error("Too many node pages required in OpenCLBVHKernels()");

				nodeIndex += pageNodeCount;
			} while (nodeIndex < totalNodeCount);
			delete[] tmpNodes;
		}

		//----------------------------------------------------------------------
		// Compile kernel sources
		//----------------------------------------------------------------------

		// Compile options
		stringstream opts;
		opts << " -D LUXRAYS_OPENCL_KERNEL"
				" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
				" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compile options: \n" << opts.str());

		stringstream kernelDefs;
		kernelDefs << "#define BVH_VERTS_PAGE_COUNT " << vertsBuffs.size() << "\n"
				"#define BVH_NODES_PAGE_COUNT " << nodeBuffs.size() <<  "\n";
		if (vertsBuffs.size() > 1) {
			// BVH_NODES_PAGE_SIZE is used only if BVH_VERTS_PAGE_COUNT > 1
			// I conditional define this value to avoid kernel recompilation
			kernelDefs << "#define BVH_NODES_PAGE_SIZE " << maxNodeCount << "\n";
		}
		for (u_int i = 0; i < vertsBuffs.size(); ++i)
			kernelDefs << "#define BVH_VERTS_PAGE" << i << " 1\n";
		for (u_int i = 0; i < nodeBuffs.size(); ++i)
			kernelDefs << "#define BVH_NODES_PAGE" << i << " 1\n";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH kernel definitions: \n" << kernelDefs.str());

		intersectionKernelSource = kernelDefs.str() +
				luxrays::ocl::KernelSource_bvh_types +
				luxrays::ocl::KernelSource_bvh;
		
		const string code(
			kernelDefs.str() +
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_epsilon_types +
			luxrays::ocl::KernelSource_epsilon_funcs +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_ray_funcs +
			luxrays::ocl::KernelSource_bbox_types +
			luxrays::ocl::KernelSource_bbox_funcs +
			luxrays::ocl::KernelSource_triangle_types +
			luxrays::ocl::KernelSource_triangle_funcs +
			luxrays::ocl::KernelSource_bvh_types +
			luxrays::ocl::KernelSource_bvh);
		cl::Program::Sources source(1, make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error &err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compilation error:\n" << strError.c_str());

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
				//	"] BVH kernel work group size: " << workGroupSize);
			}

			// Set arguments
			SetIntersectionKernelArgs(*(kernels[i]), 3);
		}
	}
	virtual ~OpenCLBVHKernels() {
		BOOST_FOREACH(cl::Buffer *buf, vertsBuffs) {
			device->FreeMemory(buf->getInfo<CL_MEM_SIZE>());
			delete buf;
		}
		BOOST_FOREACH(cl::Buffer *buf, nodeBuffs) {
			device->FreeMemory(buf->getInfo<CL_MEM_SIZE>());
			delete buf;
		}
	}

	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex);

	// BVH fields
	vector<cl::Buffer *> vertsBuffs;
	vector<cl::Buffer *> nodeBuffs;
};

void OpenCLBVHKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
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

u_int OpenCLBVHKernels::SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int index) {
	u_int argIndex = index;
	for (u_int i = 0; i < vertsBuffs.size(); ++i)
		kernel.setArg(argIndex++, *vertsBuffs[i]);
	for (u_int i = 0; i < nodeBuffs.size(); ++i)
		kernel.setArg(argIndex++, *nodeBuffs[i]);

	return argIndex;
}

OpenCLKernels *BVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const {
	// Setup kernels
	return new OpenCLBVHKernels(device, kernelCount, this);
}

#else

OpenCLKernels *BVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const {
	return NULL;
}

#endif

}
