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

#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
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

class OpenCLBVHKernels : public OpenCLKernels {
public:
	OpenCLBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const BVHAccel *bvh) :
		OpenCLKernels(dev, kernelCount) {
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
		const size_t maxVertCount = maxMemAlloc / sizeof(Point);
		const u_int totalVertCount = bvh->preprocessedMesh->GetTotalVertexCount();
		const Point *verts = bvh->preprocessedMesh->GetVertices();
		u_int vertIndex = 0;

		do {
			const u_int leftVertCount = totalVertCount - vertIndex;
			const u_int pageVertCount = Min<size_t>(leftVertCount, maxVertCount);

			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] Vertices buffer size (Page " << vertsBuffs.size() <<", " <<
				pageVertCount << " vertices): " <<
				(sizeof(Point) * pageVertCount / 1024) <<
				"Kbytes");
			cl::Buffer *vb = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Point) * pageVertCount,
				(void *)&verts[vertIndex]);
			device->AllocMemory(vb->getInfo<CL_MEM_SIZE>());
			vertsBuffs.push_back(vb);

			if (vertsBuffs.size() > 8)
				throw std::runtime_error("Too many vertex pages required in OpenCLBVHKernels()");

			vertIndex += pageVertCount;
		} while (vertIndex < totalVertCount);

		//----------------------------------------------------------------------
		// Allocate BVH node buffers
		//----------------------------------------------------------------------

		// Check how many pages I have to allocate
		const size_t maxNodeCount = maxMemAlloc / sizeof(BVHAccelArrayNode);
		const u_int totalNodeCount = bvh->nNodes;
		const BVHAccelArrayNode *nodes = bvh->bvhTree;
		// Allocate a temporary buffer for the copy of the BVH nodes
		BVHAccelArrayNode *tmpNodes = new BVHAccelArrayNode[Min<size_t>(bvh->nNodes, maxNodeCount)];
		u_int nodeIndex = 0;

		do {
			const u_int leftNodeCount = totalNodeCount - nodeIndex;
			const u_int pageNodeCount = Min<size_t>(leftNodeCount, maxNodeCount);

			// Make a copy of the nodes
			memcpy(tmpNodes, &nodes[nodeIndex], sizeof(BVHAccelArrayNode) * pageNodeCount);

			// Update the vertex and node references
			for (u_int i = 0; i < pageNodeCount; ++i) {
				BVHAccelArrayNode *node = &tmpNodes[i];
				if (BVHNodeData_IsLeaf(node->nodeData)) {
					// Update the vertex references
					for (u_int j = 0; j < 3; ++j) {
						const u_int vertexPage = node->triangleLeaf.v[j] / maxVertCount;
						const u_int vertexIndex = node->triangleLeaf.v[j] % maxVertCount;
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
				(sizeof(BVHAccelArrayNode) * pageNodeCount / 1024) <<
				"Kbytes");
			cl::Buffer *bb = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(BVHAccelArrayNode) * pageNodeCount,
				(void *)tmpNodes);
			device->AllocMemory(bb->getInfo<CL_MEM_SIZE>());
			nodeBuffs.push_back(bb);

			if (nodeBuffs.size() > 8)
				throw std::runtime_error("Too many node pages required in OpenCLBVHKernels()");

			nodeIndex += pageNodeCount;
		} while (nodeIndex < totalNodeCount);
		delete tmpNodes;

		//----------------------------------------------------------------------
		// Compile kernel sources
		//----------------------------------------------------------------------

		// Compile options
		std::stringstream opts;
		opts << " -D LUXRAYS_OPENCL_KERNEL"
				" -D BVH_VERTS_PAGE_COUNT=" << vertsBuffs.size() <<
				" -D BVH_NODES_PAGE_SIZE=" << maxNodeCount <<
				" -D BVH_NODES_PAGE_COUNT=" << nodeBuffs.size();
		for (u_int i = 0; i < vertsBuffs.size(); ++i)
			opts << " -D BVH_VERTS_PAGE" << i << "=1";
		for (u_int i = 0; i < nodeBuffs.size(); ++i)
			opts << " -D BVH_NODES_PAGE" << i << "=1";
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compile options: " << opts.str());

		std::string code(
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_bbox_types);
		code += luxrays::ocl::KernelSource_bvh;
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compilation error:\n" << strError.c_str());

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
			for (u_int j = 0; j < vertsBuffs.size(); ++j)
				kernels[i]->setArg(argIndex++, *vertsBuffs[j]);
			for (u_int j = 0; j < nodeBuffs.size(); ++j)
				kernels[i]->setArg(argIndex++, *nodeBuffs[j]);
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

OpenCLKernels *BVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	// Setup kernels
	return new OpenCLBVHKernels(device, kernelCount, this);
}

#else

OpenCLKernels *BVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	return NULL;
}

#endif

// BVHAccel Method Definitions

BVHAccel::BVHAccel(const Context *context,
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

	preprocessedMesh = NULL;

	initialized = false;
}

void BVHAccel::Init(const std::deque<const Mesh *> &meshes, const u_int totalVertexCount,
		const u_int totalTriangleCount) {
	assert (!initialized);

	// Build the preprocessed mesh
	preprocessedMesh = TriangleMesh::Merge(totalVertexCount, totalTriangleCount, meshes);
	assert (preprocessedMesh->GetTotalVertexCount() == totalVertexCount);
	assert (preprocessedMesh->GetTotalTriangleCount() == totalTriangleCount);

	LR_LOG(ctx, "Total vertices memory usage: " << totalVertexCount * sizeof(Point) / 1024 << "Kbytes");
	LR_LOG(ctx, "Total triangles memory usage: " << totalTriangleCount * sizeof(Triangle) / 1024 << "Kbytes");

	Init(preprocessedMesh);
}

void BVHAccel::Init(const Mesh *m) {
	mesh = m;

	const Point *v = mesh->GetVertices();
	const Triangle *p = mesh->GetTriangles();

	std::vector<BVHAccelTreeNode *> bvList;
	bvList.reserve(mesh->GetTotalTriangleCount());
	for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
		BVHAccelTreeNode *node = new BVHAccelTreeNode();
		node->bbox = p[i].WorldBound(v);
		// NOTE - Ratow - Expand bbox a little to make sure rays collide
		node->bbox.Expand(MachineEpsilon::E(node->bbox));
		node->triangleLeaf.index = i;
		node->leftChild = NULL;
		node->rightSibling = NULL;
		bvList.push_back(node);
	}

	LR_LOG(ctx, "Building Bounding Volume Hierarchy, primitives: " << mesh->GetTotalTriangleCount());

	nNodes = 0;
	BVHAccelTreeNode *rootNode = BuildHierarchy(&nNodes, params, bvList, 0, bvList.size(), 2);

	LR_LOG(ctx, "Pre-processing Bounding Volume Hierarchy, total nodes: " << nNodes);

	bvhTree = new BVHAccelArrayNode[nNodes];
	BuildArray(p, rootNode, 0, bvhTree);
	FreeHierarchy(rootNode);

	LR_LOG(ctx, "Total BVH memory usage: " << nNodes * sizeof(BVHAccelArrayNode) / 1024 << "Kbytes");
	//LR_LOG(ctx, "Finished building Bounding Volume Hierarchy array");

	initialized = true;
}

BVHAccel::~BVHAccel() {
	if (initialized) {
		if (preprocessedMesh) {
			// preprocessedMesh is not allocated when BVHAccel is used by MBVHAccel
			preprocessedMesh->Delete();
			delete preprocessedMesh;
		}

		delete bvhTree;
	}
}

void BVHAccel::FreeHierarchy(BVHAccelTreeNode *node) {
	if (node) {
		FreeHierarchy(node->leftChild);
		FreeHierarchy(node->rightSibling);

		delete node;
	}
}

// Build an array of comparators for each axis

static bool bvh_ltf_x(BVHAccelTreeNode *n, float v) {
	return n->bbox.pMax.x + n->bbox.pMin.x < v;
}

static bool bvh_ltf_y(BVHAccelTreeNode *n, float v) {
	return n->bbox.pMax.y + n->bbox.pMin.y < v;
}

static bool bvh_ltf_z(BVHAccelTreeNode *n, float v) {
	return n->bbox.pMax.z + n->bbox.pMin.z < v;
}

bool (* const bvh_ltf[3])(BVHAccelTreeNode *n, float v) = {bvh_ltf_x, bvh_ltf_y, bvh_ltf_z};

BVHAccelTreeNode *BVHAccel::BuildHierarchy(u_int *nNodes, const BVHParams &params,
		std::vector<BVHAccelTreeNode *> &list, u_int begin, u_int end, u_int axis) {
	u_int splitAxis = axis;
	float splitValue;

	*nNodes += 1;
	if (end - begin == 1) // Only a single item in list so return it
		return list[begin];

	BVHAccelTreeNode *parent = new BVHAccelTreeNode();
	parent->leftChild = NULL;
	parent->rightSibling = NULL;

	std::vector<u_int> splits;
	splits.reserve(params.treeType + 1);
	splits.push_back(begin);
	splits.push_back(end);
	for (u_int i = 2; i <= params.treeType; i *= 2) { // Calculate splits, according to tree type and do partition
		for (u_int j = 0, offset = 0; j + offset < i && splits.size() > j + 1; j += 2) {
			if (splits[j + 1] - splits[j] < 2) {
				j--;
				offset++;
				continue; // Less than two elements: no need to split
			}

			FindBestSplit(params, list, splits[j], splits[j + 1], &splitValue, &splitAxis);

			std::vector<BVHAccelTreeNode *>::iterator it =
					partition(list.begin() + splits[j], list.begin() + splits[j + 1], bind2nd(ptr_fun(bvh_ltf[splitAxis]), splitValue));
			u_int middle = distance(list.begin(), it);
			middle = Max(splits[j] + 1, Min(splits[j + 1] - 1, middle)); // Make sure coincidental BBs are still split
			splits.insert(splits.begin() + j + 1, middle);
		}
	}

	BVHAccelTreeNode *child, *lastChild;
	// Left Child
	child = BuildHierarchy(nNodes, params, list, splits[0], splits[1], splitAxis);
	parent->leftChild = child;
	parent->bbox = child->bbox;
	lastChild = child;

	// Add remaining children
	for (u_int i = 1; i < splits.size() - 1; i++) {
		child = BuildHierarchy(nNodes, params, list, splits[i], splits[i + 1], splitAxis);
		lastChild->rightSibling = child;
		parent->bbox = Union(parent->bbox, child->bbox);
		lastChild = child;
	}

	return parent;
}

void BVHAccel::FindBestSplit(const BVHParams &params, std::vector<BVHAccelTreeNode *> &list,
		u_int begin, u_int end, float *splitValue, u_int *bestAxis) {
	if (end - begin == 2) {
		// Trivial case with two elements
		*splitValue = (list[begin]->bbox.pMax[0] + list[begin]->bbox.pMin[0] +
				list[end - 1]->bbox.pMax[0] + list[end - 1]->bbox.pMin[0]) / 2;
		*bestAxis = 0;
	} else {
		// Calculate BBs mean center (times 2)
		Point mean2(0, 0, 0), var(0, 0, 0);
		for (u_int i = begin; i < end; i++)
			mean2 += list[i]->bbox.pMax + list[i]->bbox.pMin;
		mean2 /= static_cast<float>(end - begin);

		// Calculate variance
		for (u_int i = begin; i < end; i++) {
			Vector v = list[i]->bbox.pMax + list[i]->bbox.pMin - mean2;
			v.x *= v.x;
			v.y *= v.y;
			v.z *= v.z;
			var += v;
		}
		// Select axis with more variance
		if (var.x > var.y && var.x > var.z)
			*bestAxis = 0;
		else if (var.y > var.z)
			*bestAxis = 1;
		else
			*bestAxis = 2;

		if (params.costSamples > 1) {
			BBox nodeBounds;
			for (u_int i = begin; i < end; i++)
				nodeBounds = Union(nodeBounds, list[i]->bbox);

			Vector d = nodeBounds.pMax - nodeBounds.pMin;
			const float invTotalSA = 1.f / nodeBounds.SurfaceArea();

			// Sample cost for split at some points
			float increment = 2 * d[*bestAxis] / (params.costSamples + 1);
			float bestCost = INFINITY;
			for (float splitVal = 2 * nodeBounds.pMin[*bestAxis] + increment; splitVal < 2 * nodeBounds.pMax[*bestAxis]; splitVal += increment) {
				int nBelow = 0, nAbove = 0;
				BBox bbBelow, bbAbove;
				for (u_int j = begin; j < end; j++) {
					if ((list[j]->bbox.pMax[*bestAxis] + list[j]->bbox.pMin[*bestAxis]) < splitVal) {
						nBelow++;
						bbBelow = Union(bbBelow, list[j]->bbox);
					} else {
						nAbove++;
						bbAbove = Union(bbAbove, list[j]->bbox);
					}
				}
				const float pBelow = bbBelow.SurfaceArea() * invTotalSA;
				const float pAbove = bbAbove.SurfaceArea() * invTotalSA;
				float eb = (nAbove == 0 || nBelow == 0) ? params.emptyBonus : 0.f;
				float cost = params.traversalCost + params.isectCost * (1.f - eb) * (pBelow * nBelow + pAbove * nAbove);
				// Update best split if this is lowest cost so far
				if (cost < bestCost) {
					bestCost = cost;
					*splitValue = splitVal;
				}
			}
		} else {
			// Split in half around the mean center
			*splitValue = mean2[*bestAxis];
		}
	}
}

u_int BVHAccel::BuildArray(const Triangle *triangles, BVHAccelTreeNode *node,
		u_int offset, BVHAccelArrayNode *bvhTree) {
	// Build array by recursively traversing the tree depth-first
	while (node) {
		BVHAccelArrayNode *p = &bvhTree[offset];

		if (node->leftChild) {
			// It is a BVH node
			memcpy(&p->bvhNode.bboxMin[0], &node->bbox, sizeof(float) * 6);
			offset = BuildArray(triangles, node->leftChild, offset + 1, bvhTree);
			p->nodeData = offset;
		} else {
			// It is a leaf
			if (triangles) {
				// It is a BVH of triangles
				const Triangle *triangle = &triangles[node->triangleLeaf.index];
				p->triangleLeaf.v[0] = triangle->v[0];
				p->triangleLeaf.v[1] = triangle->v[1];
				p->triangleLeaf.v[2] = triangle->v[2];
				p->triangleLeaf.triangleIndex = node->triangleLeaf.index;
			} else {
				// It is a BVH of BVHs (i.e. MBVH)
				p->bvhLeaf.leafIndex = node->bvhLeaf.leafIndex;
				p->bvhLeaf.transformIndex = node->bvhLeaf.transformIndex;
				p->bvhLeaf.triangleOffsetIndex = node->bvhLeaf.triangleOffsetIndex;
			}

			// Mark as a leaf
			++offset;
			p->nodeData = offset | 0x80000000u;
		}

		node = node->rightSibling;
	}

	return offset;
}

bool BVHAccel::Intersect(const Ray *initialRay, RayHit *rayHit) const {
	assert (initialized);

	Ray ray(*initialRay);

	u_int currentNode = 0; // Root Node
	u_int stopNode = BVHNodeData_GetSkipIndex(bvhTree[0].nodeData); // Non-existent
	rayHit->t = ray.maxt;
	rayHit->SetMiss();

	const Point *vertices = preprocessedMesh->GetVertices();
	float t, b1, b2;
	while (currentNode < stopNode) {
		const BVHAccelArrayNode &node = bvhTree[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the triangle
			const Point &p0 = vertices[node.triangleLeaf.v[0]];
			const Point &p1 = vertices[node.triangleLeaf.v[1]];
			const Point &p2 = vertices[node.triangleLeaf.v[2]];

			if (Triangle::Intersect(ray, p0, p1, p2, &t, &b1, &b2)) {
				if (t < rayHit->t) {
					ray.maxt = t;
					rayHit->t = t;
					rayHit->b1 = b1;
					rayHit->b2 = b2;
					rayHit->index = node.triangleLeaf.triangleIndex;
					// Continue testing for closer intersections
				}
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (BBox::IntersectP(ray,
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
