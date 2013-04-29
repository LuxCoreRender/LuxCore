/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

using std::bind2nd;
using std::ptr_fun;

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLMBVHKernels : public OpenCLKernels {
public:
	OpenCLMBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const MBVHAccel *mbvh) :
		OpenCLKernels(dev, kernelCount), uniqueLeafsTransformBuff(NULL) {
		const Context *deviceContext = device->GetContext();
		const std::string &deviceName(device->GetName());
		cl::Context &oclContext = device->GetOpenCLContext();
		cl::Device &oclDevice = device->GetOpenCLDevice();

		// Check the max. number of vertices I can store in a single page
		const size_t maxMemAlloc = device->GetDeviceDesc()->GetMaxMemoryAllocSize();

		//----------------------------------------------------------------------
		// Allocate vertex buffers
		//----------------------------------------------------------------------

		// Check how many pages I have to allocate
		const size_t maxVertCount = maxMemAlloc / (sizeof(Point) * 3);
		u_int totalVertCount = 0;
		for (u_int i = 0; i < mbvh->uniqueLeafs.size(); ++i)
			totalVertCount += mbvh->uniqueLeafs[i]->mesh->GetTotalVertexCount();
		// Allocate a temporary buffer for the copy of the BVH vertices
		const u_int pageVertCount = Min<size_t>(totalVertCount, maxVertCount);
		Point *tmpVerts = new Point[pageVertCount];
		u_int tmpVertIndex = 0;

		u_int meshVertIndex = 0;
		u_int currentLeafIndex = 0;

		while (currentLeafIndex < mbvh->uniqueLeafs.size()) {
			const u_int tmpLeftVertCount = pageVertCount - tmpVertIndex;

			// Check if there is enough space in the temporary buffer for all vertices
			const Mesh *currentMesh = mbvh->uniqueLeafs[currentLeafIndex]->mesh;
			const u_int toCopy = currentMesh->GetTotalVertexCount() - meshVertIndex;
			if (tmpLeftVertCount >= toCopy) {
				// There is enough space for all mesh vertices
				memcpy(&tmpVerts[tmpVertIndex], &(currentMesh->GetVertices()[meshVertIndex]),
						sizeof(Point) * toCopy);
				tmpVertIndex += toCopy;

				// Move to the next mesh
				++currentLeafIndex;
				meshVertIndex = 0;
			} else {
				// There isn't enough space for all mesh vertices. Fill the current buffer.
				memcpy(&tmpVerts[tmpVertIndex], &(currentMesh->GetVertices()[meshVertIndex]),
						sizeof(Point) * tmpLeftVertCount);

				tmpVertIndex += tmpLeftVertCount;
				meshVertIndex += tmpLeftVertCount;
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

		//----------------------------------------------------------------------
		// Allocate BVH node buffers
		//----------------------------------------------------------------------

		// Check how many pages I have to allocate
		const size_t maxNodeCount = maxMemAlloc / sizeof(BVHAccelArrayNode);

		u_int totalNodeCount = mbvh->nRootNodes;
		for (u_int i = 0; i < mbvh->uniqueLeafs.size(); ++i)
			totalNodeCount += mbvh->uniqueLeafs[i]->nNodes;

		const u_int pageNodeCount = Min<size_t>(totalNodeCount, maxNodeCount);

		// Initialize the node offset vector
		std::vector<u_int> leafNodeOffset;
		u_int currentBVHNodeCount = mbvh->nRootNodes;
		for (u_int i = 0; i < mbvh->uniqueLeafs.size(); ++i) {
			const BVHAccel *leafBVH = mbvh->uniqueLeafs[i];
			leafNodeOffset.push_back(currentBVHNodeCount);
			currentBVHNodeCount += leafBVH->nNodes;
		}

		// Allocate a temporary buffer for the copy of the BVH nodes
		BVHAccelArrayNode *tmpNodes = new BVHAccelArrayNode[pageNodeCount];
		u_int tmpNodeIndex = 0;

		u_int currentNodeIndex = 0;
		currentLeafIndex = 0;
		const BVHAccelArrayNode *currentNodes = mbvh->bvhRootTree;
		u_int currentNodesCount = mbvh->nRootNodes;
		u_int currentVertOffset = 0;

		while (currentLeafIndex < mbvh->uniqueLeafs.size()) {
			const u_int tmpLeftNodeCount = pageNodeCount - tmpNodeIndex;
			const bool isRootTree = (currentNodes == mbvh->bvhRootTree);
			const u_int vertOffset = currentVertOffset;
			const u_int leafIndex = currentLeafIndex;

			// Check if there is enough space in the temporary buffer for all nodes
			u_int copiedIndexStart, copiedIndexEnd;
			const u_int toCopy = currentNodesCount - currentNodeIndex;
			if (tmpLeftNodeCount >= toCopy) {
				// There is enough space for all nodes
				memcpy(&tmpNodes[tmpNodeIndex], &currentNodes[currentNodeIndex],
						sizeof(BVHAccelArrayNode) * toCopy);
				copiedIndexStart = tmpNodeIndex;
				copiedIndexEnd = tmpNodeIndex + toCopy;

				tmpNodeIndex += toCopy;
				// Move to the next leaf tree
				if (isRootTree) {
					// Move from the root nodes to the first leaf node. currentLeafIndex
					// is already 0
				} else {
					currentVertOffset += mbvh->uniqueLeafs[currentLeafIndex]->mesh->GetTotalVertexCount();
					++currentLeafIndex;
				}

				if (currentLeafIndex < mbvh->uniqueLeafs.size()) {
					currentNodes = mbvh->uniqueLeafs[currentLeafIndex]->bvhTree;
					currentNodesCount = mbvh->uniqueLeafs[currentLeafIndex]->nNodes;
					currentNodeIndex = 0;
				}
			} else {
				// There isn't enough space for all mesh vertices. Fill the current buffer.
				memcpy(&tmpNodes[tmpNodeIndex], &currentNodes[currentNodeIndex],
						sizeof(BVHAccelArrayNode) * tmpLeftNodeCount);
				copiedIndexStart = tmpNodeIndex;
				copiedIndexEnd = tmpNodeIndex + tmpLeftNodeCount;

				tmpNodeIndex += tmpLeftNodeCount;
				currentNodeIndex += tmpLeftNodeCount;
			}

			// Update the vertex references
			for (u_int i = copiedIndexStart; i < copiedIndexEnd; ++i) {
				BVHAccelArrayNode *node = &tmpNodes[i];

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
							const u_int vertIndex = node->triangleLeaf.v[j] + vertOffset;
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
					(sizeof(BVHAccelArrayNode) * totalNodeCount / 1024) <<
					"Kbytes");
				cl::Buffer *nb = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(BVHAccelArrayNode) * tmpNodeIndex,
					(void *)&tmpNodes[0]);
				device->AllocMemory(nb->getInfo<CL_MEM_SIZE>());
				nodeBuffs.push_back(nb);
				if (nodeBuffs.size() > 8)
					throw std::runtime_error("Too many BVH node pages required in OpenCLMBVHKernels()");

				tmpNodeIndex = 0;
			}
		}
		delete tmpNodes;

		if (mbvh->uniqueLeafsTransform.size() > 0) {
			// Allocate the transformation buffer
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] Leaf transformations buffer size: " <<
				(sizeof(u_int) * mbvh->uniqueLeafsTransform.size() / 1024) <<
				"Kbytes");
			uniqueLeafsTransformBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Matrix4x4) * mbvh->uniqueLeafsTransform.size(),
				(void *)&(mbvh->uniqueLeafsTransform[0]));
			device->AllocMemory(uniqueLeafsTransformBuff->getInfo<CL_MEM_SIZE>());
		}

		//----------------------------------------------------------------------
		// Compile kernel sources
		//----------------------------------------------------------------------

		// Compile options
		std::stringstream opts;
		opts << " -D LUXRAYS_OPENCL_KERNEL"
				" -D MBVH_VERTS_PAGE_COUNT=" << vertsBuffs.size() <<
				" -D MBVH_NODES_PAGE_SIZE=" << pageNodeCount <<
				" -D MBVH_NODES_PAGE_COUNT=" << nodeBuffs.size();
		for (u_int i = 0; i < vertsBuffs.size(); ++i)
			opts << " -D MBVH_VERTS_PAGE" << i << "=1";
		for (u_int i = 0; i < nodeBuffs.size(); ++i)
			opts << " -D MBVH_NODES_PAGE" << i << "=1";
		if (uniqueLeafsTransformBuff)
			opts << " -D MBVH_HAS_TRANSFORMATIONS=1";
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MBVH compile options: " << opts.str());

		std::string code(
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_bbox_types +
			luxrays::ocl::KernelSource_matrix4x4_types +
			luxrays::ocl::KernelSource_triangle_types);
		code += luxrays::ocl::KernelSource_mbvh;
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] MBVH compilation error:\n" << strError.c_str());

			throw err;
		}

		// Setup kernels
		for (u_int i = 0; i < kernelCount; ++i) {
			kernels[i] = new cl::Kernel(program, "Intersect");
			kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
				CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
			//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			//	"] BVH kernel work group size: " << workGroupSize);

			kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
				CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
			//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			//	"] Suggested work group size: " << workGroupSize);

			if (device->GetDeviceDesc()->GetForceWorkGroupSize() > 0) {
				workGroupSize = device->GetDeviceDesc()->GetForceWorkGroupSize();
				//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				//	"] Forced work group size: " << workGroupSize);
			}

			// Set arguments
			u_int argIndex = 3;
			if (uniqueLeafsTransformBuff)
				kernels[i]->setArg(argIndex++, *uniqueLeafsTransformBuff);
			for (u_int j = 0; j < vertsBuffs.size(); ++j)
				kernels[i]->setArg(argIndex++, *vertsBuffs[j]);
			for (u_int j = 0; j < nodeBuffs.size(); ++j)
				kernels[i]->setArg(argIndex++, *nodeBuffs[j]);
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
	}

	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	// BVH fields
	vector<cl::Buffer *> vertsBuffs;
	vector<cl::Buffer *> nodeBuffs;
	cl::Buffer *uniqueLeafsTransformBuff;
};

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

OpenCLKernels *MBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	// Setup kernels
	return new OpenCLMBVHKernels(device, kernelCount, this);
}

#else

OpenCLKernels *MBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	return NULL;
}

#endif

// MBVHAccel Method Definitions

MBVHAccel::MBVHAccel(const Context *context,
		const u_int treetype, const int csamples, const int icost,
		const int tcost, const float ebonus) : ctx(context) {
	// Make sure treeType is 2, 4 or 8
	if (treetype <= 2) params.treeType = 2;
	else if (treetype <= 4) params.treeType = 4;
	else params.treeType = 8;

	params.costSamples = csamples;
	params.isectCost = icost;
	params.traversalCost = tcost;
	params.emptyBonus = ebonus;

	initialized = false;
}

void MBVHAccel::Init(const std::deque<const Mesh *> &meshes, const u_int totalVertexCount,
		const u_int totalTriangleCount) {
	assert (!initialized);

	//--------------------------------------------------------------------------
	// Build all BVH leafs
	//--------------------------------------------------------------------------

	const u_int nLeafs = meshes.size();
	LR_LOG(ctx, "Building Multilevel Bounding Volume Hierarchy, leafs: " << nLeafs);

	std::vector<u_int> leafsIndex;
	std::vector<u_int> leafsTransformIndex;
	std::vector<u_int> leafsTriangleOffset;

	leafsIndex.reserve(nLeafs);
	leafsTransformIndex.reserve(nLeafs);
	leafsTriangleOffset.reserve(nLeafs);

	std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)> uniqueLeafIndexByMesh(MeshPtrCompare);

	u_int currentOffset = 0;

	double lastPrint = WallClockTime();
	for (u_int i = 0; i < nLeafs; ++i) {
		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			LR_LOG(ctx, "Building BVH for MBVH leaf: " << i << "/" << nLeafs);
			lastPrint = now;
		}

		switch (meshes[i]->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE: {
				BVHAccel *leaf = new BVHAccel(ctx, params.treeType, params.costSamples, params.isectCost, params.traversalCost, params.emptyBonus);
				leaf->Init(meshes[i]);

				const u_int uniqueLeafIndex = uniqueLeafs.size();
				uniqueLeafIndexByMesh[meshes[i]] = uniqueLeafIndex;
				uniqueLeafs.push_back(leaf);
				leafsIndex.push_back(uniqueLeafIndex);
				leafsTransformIndex.push_back(NULL_INDEX);
				break;
			}
			case TYPE_TRIANGLE_INSTANCE: {
				InstanceTriangleMesh *itm = (InstanceTriangleMesh *)meshes[i];

				// Check if a BVH has already been created
				std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it = uniqueLeafIndexByMesh.find(itm->GetTriangleMesh());

				if (it == uniqueLeafIndexByMesh.end()) {
					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx, params.treeType, params.costSamples, params.isectCost, params.traversalCost, params.emptyBonus);
					leaf->Init(itm);

					const u_int uniqueLeafIndex = uniqueLeafs.size();
					uniqueLeafIndexByMesh[itm->GetTriangleMesh()] = uniqueLeafIndex;
					uniqueLeafs.push_back(leaf);
					leafsIndex.push_back(uniqueLeafIndex);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafsIndex.push_back(it->second);
				}

				leafsTransformIndex.push_back(uniqueLeafsTransform.size());
				uniqueLeafsTransform.push_back(itm->GetTransformation().mInv);
				break;
			}
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				ExtInstanceTriangleMesh *eitm = (ExtInstanceTriangleMesh *)meshes[i];

				// Check if a BVH has already been created
				std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it = uniqueLeafIndexByMesh.find(eitm->GetExtTriangleMesh());

				if (it == uniqueLeafIndexByMesh.end()) {
					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx, params.treeType, params.costSamples, params.isectCost, params.traversalCost, params.emptyBonus);
					leaf->Init(eitm);

					const u_int uniqueLeafIndex = uniqueLeafs.size();
					uniqueLeafIndexByMesh[eitm->GetExtTriangleMesh()] = uniqueLeafIndex;
					uniqueLeafs.push_back(leaf);
					leafsIndex.push_back(uniqueLeafIndex);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafsIndex.push_back(it->second);
				}

				leafsTransformIndex.push_back(uniqueLeafsTransform.size());
				uniqueLeafsTransform.push_back(eitm->GetTransformation().mInv);
				break;
			}
			default:
				assert (false);
				break;
		}

		leafsTriangleOffset.push_back(currentOffset);
		currentOffset += meshes[i]->GetTotalTriangleCount();
	}

	//--------------------------------------------------------------------------
	// Build the root BVH
	//--------------------------------------------------------------------------

	std::vector<BVHAccelTreeNode *> bvList;
	bvList.reserve(totalTriangleCount);
	for (u_int i = 0; i < nLeafs; ++i) {
		BVHAccelTreeNode *node = new BVHAccelTreeNode();
		// Get the bounding box from the mesh so it is in global coordinates
		node->bbox = meshes[i]->GetBBox();
		node->bvhLeaf.leafIndex = leafsIndex[i];
		node->bvhLeaf.transformIndex = leafsTransformIndex[i];
		node->bvhLeaf.triangleOffsetIndex = leafsTriangleOffset[i];
		node->leftChild = NULL;
		node->rightSibling = NULL;
		bvList.push_back(node);
	}

	nRootNodes = 0;
	BVHAccelTreeNode *rootNode = BVHAccel::BuildHierarchy(&nRootNodes, params, bvList, 0, bvList.size(), 2);

	LR_LOG(ctx, "Pre-processing Multilevel Bounding Volume Hierarchy, total nodes: " << nRootNodes);

	bvhRootTree = new BVHAccelArrayNode[nRootNodes];
	BVHAccel::BuildArray(NULL, rootNode, 0, bvhRootTree);
	BVHAccel::FreeHierarchy(rootNode);

	size_t totalMem = nRootNodes;
	BOOST_FOREACH(BVHAccel *bvh, uniqueLeafs)
		totalMem += bvh->nNodes;
	totalMem *= sizeof(BVHAccelArrayNode);
	LR_LOG(ctx, "Total Multilevel BVH memory usage: " << totalMem / 1024 << "Kbytes");
	//LR_LOG(ctx, "Finished building Multilevel Bounding Volume Hierarchy array");

	initialized = true;
}

MBVHAccel::~MBVHAccel() {
	if (initialized) {
		BOOST_FOREACH(BVHAccel *bvh, uniqueLeafs)
			delete bvh;
		delete bvhRootTree;
	}
}

bool MBVHAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

bool MBVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
	assert (initialized);

	bool insideLeafTree = false;
	u_int currentRootNode = 0;
	u_int rootStopNode = BVHNodeData_GetSkipIndex(bvhRootTree[0].nodeData); // Non-existent
	u_int currentNode = currentRootNode;
	u_int currentStopNode = rootStopNode; // Non-existent
	u_int currentTriangleOffset = 0;
	BVHAccelArrayNode *currentTree = bvhRootTree;
	const Mesh *currentMesh = NULL;

	Ray currentRay(*ray);
	rayHit->t = ray->maxt;
	rayHit->SetMiss();

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

		const BVHAccelArrayNode &node = currentTree[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			if (insideLeafTree) {
				// I'm inside a leaf tree, I have to check the triangle
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
						rayHit->index = node.triangleLeaf.triangleIndex + currentTriangleOffset;
						// Continue testing for closer intersections
					}
				}

				++currentNode;
			} else {
				// I have to check a leaf tree
				BVHAccel *leaf = uniqueLeafs[node.bvhLeaf.leafIndex];
				currentTree = leaf->bvhTree;
				currentMesh = leaf->mesh;
				// Transform the ray in the local coordinate system
				if (node.bvhLeaf.transformIndex != NULL_INDEX) {
					const Matrix4x4 &m = uniqueLeafsTransform[node.bvhLeaf.transformIndex];

					// Transform ray origin
					currentRay.o.x = m.m[0][0] * ray->o.x + m.m[0][1] * ray->o.x + m.m[0][2] * ray->o.x + m.m[0][3];
					currentRay.o.y = m.m[1][0] * ray->o.x + m.m[1][1] * ray->o.y + m.m[1][2] * ray->o.z + m.m[1][3];
					currentRay.o.z = m.m[2][0] * ray->o.x + m.m[2][1] * ray->o.y + m.m[2][2] * ray->o.z + m.m[2][3];
					const float w = m.m[3][0] * ray->o.x + m.m[3][1] * ray->o.y + m.m[3][2] * ray->o.z + m.m[3][3];
					if (w != 1.f)
						currentRay.o /= w;

					// Transform ray direction
					currentRay.d.x = m.m[0][0] * ray->d.x + m.m[0][1] * ray->d.y + m.m[0][2] * ray->d.z;
					currentRay.d.y = m.m[1][0] * ray->d.x + m.m[1][1] * ray->d.y + m.m[1][2] * ray->d.z;
					currentRay.d.z = m.m[2][0] * ray->d.x + m.m[2][1] * ray->d.y + m.m[2][2] * ray->d.z;

					currentRay.maxt = rayHit->t;
				}
				currentTriangleOffset = node.bvhLeaf.triangleOffsetIndex;

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
