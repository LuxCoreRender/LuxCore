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

// Boundary Volume Hierarchy accelerator
// Based of "Efficiency Issues for Ray Tracing" by Brian Smits
// Available at http://www.cs.utah.edu/~bes/papers/fastRT/paper.html

#include <iostream>
#include <functional>
#include <algorithm>
#include <limits>

#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/utils/utils.h"
#include "luxrays/core/context.h"
#include "luxrays/devices/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

using namespace std;

namespace luxrays {

class BVHKernel : public HardwareIntersectionKernel {
public:
	BVHKernel(HardwareIntersectionDevice &dev, const BVHAccel &bvh) :
		HardwareIntersectionKernel(dev), kernel(nullptr) {
		//const Context *deviceContext = device.GetContext();
		//const string &deviceName(device.GetName());

		size_t maxNodeCount = 0;
		if (bvh.nNodes) {
			// Check the max. number of vertices I can store in a single page
			size_t maxMemAlloc = device.GetDeviceDesc()->GetMaxMemoryAllocSize();

			const BufferType memTypeFlags = device.GetContext()->GetUseOutOfCoreBuffers() ?
				((BufferType)(BUFFER_TYPE_READ_ONLY | BUFFER_TYPE_OUT_OF_CORE)) :
				BUFFER_TYPE_READ_ONLY;
			
			//------------------------------------------------------------------
			// Allocate vertex buffers
			//------------------------------------------------------------------

			// Check how many pages I have to allocate
			const size_t maxVertCount = maxMemAlloc / sizeof(Point);
			const u_int totalVertCount = bvh.totalVertexCount;

			// Allocate the temporary vertex buffer
			Point *tmpVerts = new Point[Min<size_t>(totalVertCount, maxVertCount)];
			deque<const Mesh *>::const_iterator mesh = bvh.meshes.begin();

			u_int vertsCopied = 0;
			u_int meshVertIndex = 0;
			vector<u_int> meshVertexOffsets;
			meshVertexOffsets.reserve(bvh.meshes.size());
			meshVertexOffsets.push_back(0);
			do {
				const u_int leftVertCount = totalVertCount - vertsCopied;
				const u_int pageVertCount = Min<size_t>(leftVertCount, maxVertCount);

				// Fill the temporary vertex buffer
				u_int meshVertCount = (*mesh)->GetTotalVertexCount();
				for (u_int i = 0; i < pageVertCount; ++i) {
					if (meshVertIndex >= meshVertCount) {
						// Move to the next mesh
						if (++mesh == bvh.meshes.end())
							break;

						meshVertexOffsets.push_back(vertsCopied);

						meshVertCount = (*mesh)->GetTotalVertexCount();
						meshVertIndex = 0;
					}
					// This is a fast path because I know Mesh can be only TYPE_TRIANGLE/TYPE_EXT_TRIANGLE
					// in BVH
					tmpVerts[i] = (*mesh)->GetVertex(Transform::TRANS_IDENTITY, meshVertIndex++);
					++vertsCopied;
				}

				// Allocate the OpenCL buffer
				vertsBuffs.push_back(nullptr);
				if (vertsBuffs.size() > 8)
					throw runtime_error("Too many vertex pages required in BVHKernels()");

				device.AllocBuffer(&vertsBuffs.back(), memTypeFlags,
						tmpVerts, sizeof(Point) * pageVertCount,
						"BVH mesh vertices");
				device.FinishQueue();
			} while (vertsCopied < totalVertCount);
			delete[] tmpVerts;

			//------------------------------------------------------------------
			// Allocate BVH node buffers
			//------------------------------------------------------------------

			// Check how many pages I have to allocate
			maxNodeCount = maxMemAlloc / sizeof(luxrays::ocl::BVHArrayNode);
			const u_int totalNodeCount = bvh.nNodes;
			const luxrays::ocl::BVHArrayNode *nodes = bvh.bvhTree;
			// Allocate a temporary buffer for the copy of the BVH nodes
			luxrays::ocl::BVHArrayNode *tmpNodes = new luxrays::ocl::BVHArrayNode[Min<size_t>(bvh.nNodes, maxNodeCount)];
			u_int nodeIndex = 0;

			do {
				const u_int leftNodeCount = totalNodeCount - nodeIndex;
				const u_int pageNodeCount = Min<size_t>(leftNodeCount, maxNodeCount);

				// Make a copy of the nodes
				memcpy(tmpNodes, &nodes[nodeIndex], sizeof(luxrays::ocl::BVHArrayNode) * pageNodeCount);

				// Update the vertex and node references
				for (u_int i = 0; i < pageNodeCount; ++i) {
					luxrays::ocl::BVHArrayNode *node = &tmpNodes[i];
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
				nodeBuffs.push_back(nullptr);
				if (nodeBuffs.size() > 8)
					throw runtime_error("Too many node pages required in BVHKernels()");

				device.AllocBuffer(&nodeBuffs.back(), memTypeFlags,
						tmpNodes, sizeof(luxrays::ocl::BVHArrayNode) * pageNodeCount,
						"BVH nodes");
				device.FinishQueue();

				nodeIndex += pageNodeCount;
			} while (nodeIndex < totalNodeCount);
			delete[] tmpNodes;
		}

		//----------------------------------------------------------------------
		// Compile kernel sources
		//----------------------------------------------------------------------

		// Compile options
		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D PARAM_RAY_EPSILON_MIN=" + ToString(MachineEpsilon::GetMin()) + "f");
		opts.push_back("-D PARAM_RAY_EPSILON_MAX=" + ToString(MachineEpsilon::GetMax()) + "f");
		//LR_LOG(deviceContext, "[HardwareIntersectionDevice::" << deviceName << "] BVH compile options: \n" << oclKernelPersistentCache::ToOptsString(opts));

		stringstream kernelDefs;
		kernelDefs << "#define BVH_VERTS_PAGE_COUNT " << vertsBuffs.size() << "\n"
				"#define BVH_NODES_PAGE_COUNT " << nodeBuffs.size() <<  "\n";
		if (nodeBuffs.size() > 1) {
			// BVH_NODES_PAGE_SIZE is used only if BVH_NODES_PAGE_COUNT > 1
			// I conditional define this value to avoid kernel recompilation
			kernelDefs << "#define BVH_NODES_PAGE_SIZE " << maxNodeCount << "\n";
		}
		for (u_int i = 0; i < vertsBuffs.size(); ++i)
			kernelDefs << "#define BVH_VERTS_PAGE" << i << " 1\n";
		for (u_int i = 0; i < nodeBuffs.size(); ++i)
			kernelDefs << "#define BVH_NODES_PAGE" << i << " 1\n";
		//LR_LOG(deviceContext, "[HardwareIntersectionDevice::" << deviceName << "] BVH kernel definitions: \n" << kernelDefs.str());

		stringstream code;
		code <<
			kernelDefs.str() <<
			luxrays::ocl::KernelSource_luxrays_types <<
			luxrays::ocl::KernelSource_epsilon_types <<
			luxrays::ocl::KernelSource_epsilon_funcs <<
			luxrays::ocl::KernelSource_point_types <<
			luxrays::ocl::KernelSource_vector_types <<
			luxrays::ocl::KernelSource_ray_types <<
			luxrays::ocl::KernelSource_ray_funcs <<
			luxrays::ocl::KernelSource_bbox_types <<
			luxrays::ocl::KernelSource_bbox_funcs <<
			luxrays::ocl::KernelSource_triangle_types <<
			luxrays::ocl::KernelSource_triangle_funcs <<
			luxrays::ocl::KernelSource_bvhbuild_types <<
			luxrays::ocl::KernelSource_bvh;
		
		HardwareDeviceProgram *program = nullptr;
		device.CompileProgram(&program,
				opts,
				code.str(),
				"BVHKernel");

		// Setup the kernel
		device.GetKernel(program, &kernel, "Accelerator_Intersect_RayBuffer");

		if (device.GetDeviceDesc()->GetForceWorkGroupSize() > 0)
			workGroupSize = device.GetDeviceDesc()->GetForceWorkGroupSize();
		else {
			workGroupSize = device.GetKernelWorkGroupSize(kernel); 
			//LR_LOG(deviceContext, "[HardwareIntersectionDevice::" << deviceName <<
			//	"] BVH kernel work group size: " << workGroupSize);
		}

		// Set kernel arguments
		u_int argIndex = 3;
		for (u_int i = 0; i < 8; ++i) {
			if (i >= vertsBuffs.size())
				device.SetKernelArg(kernel, argIndex++, nullptr);
			else
				device.SetKernelArg(kernel, argIndex++, vertsBuffs[i]);
		}
		for (u_int i = 0; i < 8; ++i) {
			if (i >= nodeBuffs.size())
				device.SetKernelArg(kernel, argIndex++, nullptr);
			else
				device.SetKernelArg(kernel, argIndex++, nodeBuffs[i]);
		}

		delete program;
	}
	virtual ~BVHKernel() {
		for (u_int i = 0; i < vertsBuffs.size(); ++i)
			device.FreeBuffer(&vertsBuffs[i]);
		for (u_int i = 0; i < nodeBuffs.size(); ++i)
			device.FreeBuffer(&nodeBuffs[i]);
	}

	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount);

	// BVH fields
	vector<HardwareDeviceBuffer *> vertsBuffs;
	vector<HardwareDeviceBuffer *> nodeBuffs;
	
	HardwareDeviceKernel *kernel;
	u_int workGroupSize;
};

void BVHKernel::EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount) {
	device.SetKernelArg(kernel, 0, rayBuff);
	device.SetKernelArg(kernel, 1, rayHitBuff);
	device.SetKernelArg(kernel, 2, rayCount);

	const u_int globalRange = RoundUp<u_int>(rayCount, workGroupSize);
	device.EnqueueKernel(kernel, HardwareDeviceRange(globalRange),
			HardwareDeviceRange(workGroupSize));
}

bool BVHAccel::HasNativeSupport(const IntersectionDevice &device) const {
	return true;
}

bool BVHAccel::HasHWSupport(const IntersectionDevice &device) const {
	return device.HasHWSupport();
}

HardwareIntersectionKernel *BVHAccel::NewHardwareIntersectionKernel(HardwareIntersectionDevice &device) const {
	// Setup the kernel
	return new BVHKernel(device, *this);
}

}
