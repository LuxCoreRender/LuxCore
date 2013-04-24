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
//
//class OpenCLBVHKernels : public OpenCLKernels {
//public:
//	OpenCLBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount) :
//		OpenCLKernels(dev, kernelCount), vertsBuff(NULL), trisBuff(NULL), bvhBuff(NULL) {
//		const Context *deviceContext = device->GetContext();
//		const std::string &deviceName(device->GetName());
//		cl::Context &oclContext = device->GetOpenCLContext();
//		cl::Device &oclDevice = device->GetOpenCLDevice();
//
//		// Compile sources
//		std::string code(
//			luxrays::ocl::KernelSource_luxrays_types +
//			luxrays::ocl::KernelSource_point_types +
//			luxrays::ocl::KernelSource_vector_types +
//			luxrays::ocl::KernelSource_ray_types +
//			luxrays::ocl::KernelSource_bbox_types +
//			luxrays::ocl::KernelSource_triangle_types);
//		code += luxrays::ocl::KernelSource_bvh;
//		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
//		cl::Program program = cl::Program(oclContext, source);
//		try {
//			VECTOR_CLASS<cl::Device> buildDevice;
//			buildDevice.push_back(oclDevice);
//			program.build(buildDevice, "-D LUXRAYS_OPENCL_KERNEL");
//		} catch (cl::Error err) {
//			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
//			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compilation error:\n" << strError.c_str());
//
//			throw err;
//		}
//
//		// Setup kernels
//		for (u_int i = 0; i < kernelCount; ++i) {
//			kernels[i] = new cl::Kernel(program, "Intersect");
//			kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
//				CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
//			//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
//			//	"] BVH kernel work group size: " << workGroupSize);
//
//			kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
//				CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
//			//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
//			//	"] Suggested work group size: " << workGroupSize);
//
//			if (device->GetDeviceDesc()->GetForceWorkGroupSize() > 0) {
//				workGroupSize = device->GetDeviceDesc()->GetForceWorkGroupSize();
//				//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
//				//	"] Forced work group size: " << workGroupSize);
//			}
//		}
//	}
//	virtual ~OpenCLBVHKernels() {
//		device->FreeMemory(vertsBuff->getInfo<CL_MEM_SIZE>());
//		delete vertsBuff;
//		vertsBuff = NULL;
//		device->FreeMemory(trisBuff->getInfo<CL_MEM_SIZE>());
//		delete trisBuff;
//		trisBuff = NULL;
//		device->FreeMemory(bvhBuff->getInfo<CL_MEM_SIZE>());
//		delete bvhBuff;
//		bvhBuff = NULL;
//	}
//
//	void SetBuffers(cl::Buffer *v, u_int nt, cl::Buffer *t,
//		u_int nn, cl::Buffer *b);
//	virtual void UpdateDataSet(const DataSet *newDataSet) { assert(false); }
//	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
//		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
//		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);
//
//	// BVH fields
//	cl::Buffer *vertsBuff;
//	cl::Buffer *trisBuff;
//	cl::Buffer *bvhBuff;
//};
//
//void OpenCLBVHKernels::SetBuffers(cl::Buffer *v,
//	u_int nt, cl::Buffer *t, u_int nn, cl::Buffer *b) {
//	vertsBuff = v;
//	trisBuff = t;
//	bvhBuff = b;
//
//	// Set arguments
//	BOOST_FOREACH(cl::Kernel *kernel, kernels) {
//		kernel->setArg(2, *vertsBuff);
//		kernel->setArg(3, *trisBuff);
//		kernel->setArg(4, nt);
//		kernel->setArg(5, nn);
//		kernel->setArg(6, *bvhBuff);
//	}
//}
//
//void OpenCLBVHKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
//		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
//		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
//	kernels[kernelIndex]->setArg(0, rBuff);
//	kernels[kernelIndex]->setArg(1, hBuff);
//	kernels[kernelIndex]->setArg(7, rayCount);
//
//	const u_int globalRange = RoundUp<u_int>(rayCount, workGroupSize);
//	oclQueue.enqueueNDRangeKernel(*kernels[kernelIndex], cl::NullRange,
//		cl::NDRange(globalRange), cl::NDRange(workGroupSize), events,
//		event);
//}

OpenCLKernels *MBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool disableImageStorage) const {
	return NULL;

//	const Context *deviceContext = device->GetContext();
//	cl::Context &oclContext = device->GetOpenCLContext();
//	const std::string &deviceName(device->GetName());
//
//	// Allocate buffers
//	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
//		"] Vertices buffer size: " <<
//		(sizeof(Point) * preprocessedMesh->GetTotalVertexCount() / 1024) <<
//		"Kbytes");
//	cl::Buffer *vertsBuff = new cl::Buffer(oclContext,
//		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
//		sizeof(Point) * preprocessedMesh->GetTotalVertexCount(),
//		preprocessedMesh->GetVertices());
//	device->AllocMemory(vertsBuff->getInfo<CL_MEM_SIZE>());
//
//	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
//		"] Triangle indices buffer size: " <<
//		(sizeof(Triangle) * preprocessedMesh->GetTotalTriangleCount() / 1024) <<
//		"Kbytes");
//	cl::Buffer *trisBuff = new cl::Buffer(oclContext,
//		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
//		sizeof(Triangle) * preprocessedMesh->GetTotalTriangleCount(),
//		preprocessedMesh->GetTriangles());
//	device->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());
//
//	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
//		"] BVH buffer size: " <<
//		(sizeof(BVHAccelArrayNode) * nNodes / 1024) <<
//		"Kbytes");
//	cl::Buffer *bvhBuff = new cl::Buffer(oclContext,
//		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
//		sizeof(BVHAccelArrayNode) * nNodes,
//		(void*)bvhTree);
//	device->AllocMemory(bvhBuff->getInfo<CL_MEM_SIZE>());
//
//	// Setup kernels
//	OpenCLBVHKernels *kernels = new OpenCLBVHKernels(device, kernelCount);
//	kernels->SetBuffers(vertsBuff, preprocessedMesh->GetTotalTriangleCount(),
//		trisBuff, nNodes, bvhBuff);
//
//	return kernels;
}

#else

OpenCLKernels *MBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool disableImageStorage) const {
	return NULL;
}

#endif

// MBVHAccel Method Definitions

MBVHAccel::MBVHAccel(const Context *context,
		const u_int treetype, const int csamples, const int icost,
		const int tcost, const float ebonus) : leafIndexByMesh(MeshPtrCompare), ctx(context) {
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
	leafs.reserve(nLeafs);
	leafsOffset.reserve(nLeafs);
	leafsTransformIndex.reserve(nLeafs);

	meshIDs = new TriangleMeshID[totalTriangleCount];
	meshTriangleIDs = new TriangleID[totalTriangleCount];
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

				leafIndexByMesh[meshes[i]] = leafs.size();
				leafs.push_back(leaf);
				leafsTransformIndex.push_back(NULL_INDEX);
				break;
			}
			case TYPE_TRIANGLE_INSTANCE: {
				InstanceTriangleMesh *itm = (InstanceTriangleMesh *)meshes[i];

				// Check if a BVH has already been created
				std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it = leafIndexByMesh.find(itm->GetTriangleMesh());

				if (it == leafIndexByMesh.end()) {
					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx, params.treeType, params.costSamples, params.isectCost, params.traversalCost, params.emptyBonus);
					leaf->Init(itm);

					leafIndexByMesh[itm->GetTriangleMesh()] = leafs.size();
					leafs.push_back(leaf);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafs.push_back(leafs[it->second]);
				}

				leafsTransformIndex.push_back(leafsTransform.size());
				leafsTransform.push_back(itm->GetTransformation());
				break;
			}
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				ExtInstanceTriangleMesh *eitm = (ExtInstanceTriangleMesh *)meshes[i];

				// Check if a BVH has already been created
				std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it = leafIndexByMesh.find(eitm->GetExtTriangleMesh());

				if (it == leafIndexByMesh.end()) {
					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx, params.treeType, params.costSamples, params.isectCost, params.traversalCost, params.emptyBonus);
					leaf->Init(eitm);

					leafIndexByMesh[eitm->GetExtTriangleMesh()] = leafs.size();
					leafs.push_back(leaf);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafs.push_back(leafs[it->second]);
				}

				leafsTransformIndex.push_back(leafsTransform.size());
				leafsTransform.push_back(eitm->GetTransformation());
				break;
			}
			default:
				assert (false);
				break;
		}

		leafsOffset[i] = currentOffset;

		for (u_int j = 0; j < meshes[i]->GetTotalTriangleCount(); ++j) {
			meshIDs[currentOffset + j] = i;
			meshTriangleIDs[currentOffset + j] = j;
		}

		currentOffset += meshes[i]->GetTotalTriangleCount();
	}

	//--------------------------------------------------------------------------
	// Build the root BVH
	//--------------------------------------------------------------------------

	std::vector<BVHAccelTreeNode *> bvList;
	bvList.reserve(totalTriangleCount);
	for (u_int i = 0; i < nLeafs; ++i) {
		BVHAccelTreeNode *ptr = new BVHAccelTreeNode();
		// Get the bounding box from the mesh so it is in global coordinates
		ptr->bbox = meshes[i]->GetBBox();
		ptr->primitive = i;
		ptr->leftChild = NULL;
		ptr->rightSibling = NULL;
		bvList.push_back(ptr);
	}

	nRootNodes = 0;
	BVHAccelTreeNode *rootNode = BVHAccel::BuildHierarchy(&nRootNodes, params, bvList, 0, bvList.size(), 2);

	LR_LOG(ctx, "Pre-processing Multilevel Bounding Volume Hierarchy, total nodes: " << nRootNodes);

	bvhRootTree = new BVHAccelArrayNode[nRootNodes];
	BVHAccel::BuildArray(NULL, rootNode, 0, bvhRootTree);
	BVHAccel::FreeHierarchy(rootNode);

	size_t totalMem = nRootNodes;
	for (std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it = leafIndexByMesh.begin(); it != leafIndexByMesh.end(); it++)
		totalMem += leafs[it->second]->nNodes;
	totalMem *= sizeof(BVHAccelArrayNode);
	LR_LOG(ctx, "Total Multilevel BVH memory usage: " << totalMem / 1024 << "Kbytes");
	//LR_LOG(ctx, "Finished building Multilevel Bounding Volume Hierarchy array");

	initialized = true;
}

MBVHAccel::~MBVHAccel() {
	if (initialized) {
		delete[] meshIDs;
		delete[] meshTriangleIDs;
		for (std::map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it = leafIndexByMesh.begin(); it != leafIndexByMesh.end(); it++)
			delete leafs[it->second];
		delete bvhRootTree;
	}
}

bool MBVHAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

bool MBVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
	assert (initialized);

	bool insideLeafTree = false;
	u_int currentLeafIndex = 0;
	u_int currentRootNode = 0;
	u_int rootStopNode = BVHNodeData_GetSkipIndex(bvhRootTree[0].nodeData); // Non-existent
	u_int currentNode = currentRootNode;
	u_int currentStopNode = rootStopNode; // Non-existent
	BVHAccelArrayNode *currentTree = bvhRootTree;
	const Mesh *currentMesh = NULL;

	Ray currentRay(*ray);
	rayHit->t = ray->maxt;
	rayHit->SetMiss();

	float t, b1, b2;
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
				// It is a leaf, check the triangle
				const Point *vertices = currentMesh->GetVertices();
				const Point &p0 = vertices[node.triangleLeaf.v[0]];
				const Point &p1 = vertices[node.triangleLeaf.v[1]];
				const Point &p2 = vertices[node.triangleLeaf.v[2]];

				if (Triangle::Intersect(currentRay, p0, p1, p2, &t, &b1, &b2)) {
					if (t < rayHit->t) {
						currentRay.maxt = t;
						rayHit->t = t;
						rayHit->b1 = b1;
						rayHit->b2 = b2;
						rayHit->index = node.triangleLeaf.triangleIndex + leafsOffset[currentLeafIndex];
						// Continue testing for closer intersections
					}
				}

				++currentNode;
			} else {
				// I have to check a leaf tree
				currentLeafIndex = node.bvhLeaf.index;
				BVHAccel *leaf = leafs[currentLeafIndex];
				currentTree = leaf->bvhTree;
				currentMesh = leaf->mesh;
				// Transform the ray in the local coordinate system
				if (leafsTransformIndex[currentLeafIndex] != NULL_INDEX) {
					currentRay = Inverse(leafsTransform[leafsTransformIndex[currentLeafIndex]]) * (*ray);
					currentRay.maxt = rayHit->t;
				}

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
