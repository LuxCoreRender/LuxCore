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
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/kernels/kernels.h"

using std::bind2nd;
using std::ptr_fun;

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLBVHKernel : public OpenCLKernel {
public:
	OpenCLBVHKernel(OpenCLIntersectionDevice *dev) : OpenCLKernel(dev),
		vertsBuff(NULL), trisBuff(NULL), bvhBuff(NULL) {
		const Context *deviceContext = device->GetContext();
		cl::Context &oclContext = device->GetOpenCLContext();
		cl::Device &oclDevice = device->GetOpenCLDevice();
		const std::string &deviceName(device->GetName());
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
	}
	virtual ~OpenCLBVHKernel() { FreeBuffers(); }

	virtual void FreeBuffers();
	void SetBuffers(cl::Buffer *v, unsigned int nt, cl::Buffer *t,
		unsigned int nn, cl::Buffer *b);
	virtual void UpdateDataSet(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueRayBuffer(cl::Buffer &rBuff, cl::Buffer &hBuff,
		const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	// BVH fields
	cl::Buffer *vertsBuff;
	cl::Buffer *trisBuff;
	cl::Buffer *bvhBuff;
};

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

void OpenCLBVHKernel::SetBuffers(cl::Buffer *v,
	unsigned int nt, cl::Buffer *t, unsigned int nn, cl::Buffer *b)
{
	vertsBuff = v;
	trisBuff = t;
	bvhBuff = b;
	// Set arguments
	kernel->setArg(2, *vertsBuff);
	kernel->setArg(3, *trisBuff);
	kernel->setArg(4, nt);
	kernel->setArg(5, nn);
	kernel->setArg(6, *bvhBuff);
}

void OpenCLBVHKernel::EnqueueRayBuffer(cl::Buffer &rBuff, cl::Buffer &hBuff,
	const unsigned int rayCount, const VECTOR_CLASS<cl::Event> *events,
	cl::Event *event)
{
	kernel->setArg(0, rBuff);
	kernel->setArg(1, hBuff);
	kernel->setArg(7, rayCount);
	device->GetOpenCLQueue().enqueueNDRangeKernel(*kernel, cl::NullRange,
		cl::NDRange(rayCount), cl::NDRange(workGroupSize), events,
		event);
}

OpenCLKernel *BVHAccel::NewOpenCLKernel(OpenCLIntersectionDevice *dev,
	unsigned int stackSize, bool disableImageStorage) const
{
	OpenCLBVHKernel *kernel = new OpenCLBVHKernel(dev);

	const Context *deviceContext = dev->GetContext();
	cl::Context &oclContext = dev->GetOpenCLContext();
	const std::string &deviceName(dev->GetName());
	OpenCLDeviceDescription *deviceDesc = dev->GetDeviceDesc();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Vertices buffer size: " <<
		(sizeof(Point) * preprocessedMesh->GetTotalVertexCount() / 1024) <<
		"Kbytes");
	cl::Buffer *vertsBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(Point) * preprocessedMesh->GetTotalVertexCount(),
		preprocessedMesh->GetVertices());
	deviceDesc->AllocMemory(vertsBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Triangle indices buffer size: " <<
		(sizeof(Triangle) * preprocessedMesh->GetTotalTriangleCount() / 1024) <<
		"Kbytes");
	cl::Buffer *trisBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(Triangle) * preprocessedMesh->GetTotalTriangleCount(),
		preprocessedMesh->GetTriangles());
	deviceDesc->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] BVH buffer size: " <<
		(sizeof(BVHAccelArrayNode) * nNodes / 1024) <<
		"Kbytes");
	cl::Buffer *bvhBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(BVHAccelArrayNode) * nNodes,
		(void*)bvhTree);
	deviceDesc->AllocMemory(bvhBuff->getInfo<CL_MEM_SIZE>());

	kernel->SetBuffers(vertsBuff, preprocessedMesh->GetTotalTriangleCount(),
		trisBuff, nNodes, bvhBuff);

	return kernel;
}

#else

OpenCLKernel *BVHAccel::NewOpenCLKernel(OpenCLIntersectionDevice *dev,
	unsigned int stackSize, bool disableImageStorage) const
{
	return NULL;
}

#endif

// BVHAccel Method Definitions

BVHAccel::BVHAccel(const Context *context,
		const unsigned int treetype, const int csamples, const int icost,
		const int tcost, const float ebonus) :
		costSamples(csamples), isectCost(icost), traversalCost(tcost), emptyBonus(ebonus), ctx(context) {
	// Make sure treeType is 2, 4 or 8
	if (treetype <= 2) treeType = 2;
	else if (treetype <= 4) treeType = 4;
	else treeType = 8;

	initialized = false;
}

void BVHAccel::Init(const std::deque<Mesh *> &meshes, const unsigned int totalVertexCount,
		const unsigned int totalTriangleCount) {
	assert (!initialized);

	preprocessedMesh = TriangleMesh::Merge(totalVertexCount, totalTriangleCount,
			meshes, &preprocessedMeshIDs, &preprocessedMeshTriangleIDs);
	assert (preprocessedMesh->GetTotalVertexCount() == totalVertexCount);
	assert (preprocessedMesh->GetTotalTriangleCount() == totalTriangleCount);

	LR_LOG(ctx, "Total vertices memory usage: " << totalVertexCount * sizeof(Point) / 1024 << "Kbytes");
	LR_LOG(ctx, "Total triangles memory usage: " << totalTriangleCount * sizeof(Triangle) / 1024 << "Kbytes");

	const Point *v = preprocessedMesh->GetVertices();
	const Triangle *p = preprocessedMesh->GetTriangles();

	std::vector<BVHAccelTreeNode *> bvList;
	for (unsigned int i = 0; i < totalTriangleCount; ++i) {
		BVHAccelTreeNode *ptr = new BVHAccelTreeNode();
		ptr->bbox = p[i].WorldBound(v);
		// NOTE - Ratow - Expand bbox a little to make sure rays collide
		ptr->bbox.Expand(MachineEpsilon::E(ptr->bbox));
		ptr->primitive = i;
		ptr->leftChild = NULL;
		ptr->rightSibling = NULL;
		bvList.push_back(ptr);
	}

	LR_LOG(ctx, "Building Bounding Volume Hierarchy, primitives: " << totalTriangleCount);

	nNodes = 0;
	BVHAccelTreeNode *rootNode = BuildHierarchy(bvList, 0, bvList.size(), 2);

	LR_LOG(ctx, "Pre-processing Bounding Volume Hierarchy, total nodes: " << nNodes);

	bvhTree = new BVHAccelArrayNode[nNodes];
	BuildArray(rootNode, 0);
	FreeHierarchy(rootNode);

	LR_LOG(ctx, "Total BVH memory usage: " << nNodes * sizeof(BVHAccelArrayNode) / 1024 << "Kbytes");
	LR_LOG(ctx, "Finished building Bounding Volume Hierarchy array");

	initialized = true;
}

BVHAccel::~BVHAccel() {
	if (initialized) {
		preprocessedMesh->Delete();
		delete preprocessedMesh;
		delete[] preprocessedMeshIDs;
		delete[] preprocessedMeshTriangleIDs;
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

bool bvh_ltf_x(BVHAccelTreeNode *n, float v) {
	return n->bbox.pMax.x + n->bbox.pMin.x < v;
}

bool bvh_ltf_y(BVHAccelTreeNode *n, float v) {
	return n->bbox.pMax.y + n->bbox.pMin.y < v;
}

bool bvh_ltf_z(BVHAccelTreeNode *n, float v) {
	return n->bbox.pMax.z + n->bbox.pMin.z < v;
}

bool (* const bvh_ltf[3])(BVHAccelTreeNode *n, float v) = {bvh_ltf_x, bvh_ltf_y, bvh_ltf_z};

BVHAccelTreeNode *BVHAccel::BuildHierarchy(std::vector<BVHAccelTreeNode *> &list, unsigned int begin, unsigned int end, unsigned int axis) {
	unsigned int splitAxis = axis;
	float splitValue;

	nNodes += 1;
	if (end - begin == 1) // Only a single item in list so return it
		return list[begin];

	BVHAccelTreeNode *parent = new BVHAccelTreeNode();
	parent->primitive = 0xffffffffu;
	parent->leftChild = NULL;
	parent->rightSibling = NULL;

	std::vector<unsigned int> splits;
	splits.reserve(treeType + 1);
	splits.push_back(begin);
	splits.push_back(end);
	for (unsigned int i = 2; i <= treeType; i *= 2) { // Calculate splits, according to tree type and do partition
		for (unsigned int j = 0, offset = 0; j + offset < i && splits.size() > j + 1; j += 2) {
			if (splits[j + 1] - splits[j] < 2) {
				j--;
				offset++;
				continue; // Less than two elements: no need to split
			}

			FindBestSplit(list, splits[j], splits[j + 1], &splitValue, &splitAxis);

			std::vector<BVHAccelTreeNode *>::iterator it =
					partition(list.begin() + splits[j], list.begin() + splits[j + 1], bind2nd(ptr_fun(bvh_ltf[splitAxis]), splitValue));
			unsigned int middle = distance(list.begin(), it);
			middle = Max(splits[j] + 1, Min(splits[j + 1] - 1, middle)); // Make sure coincidental BBs are still split
			splits.insert(splits.begin() + j + 1, middle);
		}
	}

	BVHAccelTreeNode *child, *lastChild;
	// Left Child
	child = BuildHierarchy(list, splits[0], splits[1], splitAxis);
	parent->leftChild = child;
	parent->bbox = child->bbox;
	lastChild = child;

	// Add remaining children
	for (unsigned int i = 1; i < splits.size() - 1; i++) {
		child = BuildHierarchy(list, splits[i], splits[i + 1], splitAxis);
		lastChild->rightSibling = child;
		parent->bbox = Union(parent->bbox, child->bbox);
		lastChild = child;
	}

	return parent;
}

void BVHAccel::FindBestSplit(std::vector<BVHAccelTreeNode *> &list, unsigned int begin, unsigned int end, float *splitValue, unsigned int *bestAxis) {
	if (end - begin == 2) {
		// Trivial case with two elements
		*splitValue = (list[begin]->bbox.pMax[0] + list[begin]->bbox.pMin[0] +
				list[end - 1]->bbox.pMax[0] + list[end - 1]->bbox.pMin[0]) / 2;
		*bestAxis = 0;
	} else {
		// Calculate BBs mean center (times 2)
		Point mean2(0, 0, 0), var(0, 0, 0);
		for (unsigned int i = begin; i < end; i++)
			mean2 += list[i]->bbox.pMax + list[i]->bbox.pMin;
		mean2 /= static_cast<float>(end - begin);

		// Calculate variance
		for (unsigned int i = begin; i < end; i++) {
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

		if (costSamples > 1) {
			BBox nodeBounds;
			for (unsigned int i = begin; i < end; i++)
				nodeBounds = Union(nodeBounds, list[i]->bbox);

			Vector d = nodeBounds.pMax - nodeBounds.pMin;
			const float invTotalSA = 1.f / nodeBounds.SurfaceArea();

			// Sample cost for split at some points
			float increment = 2 * d[*bestAxis] / (costSamples + 1);
			float bestCost = INFINITY;
			for (float splitVal = 2 * nodeBounds.pMin[*bestAxis] + increment; splitVal < 2 * nodeBounds.pMax[*bestAxis]; splitVal += increment) {
				int nBelow = 0, nAbove = 0;
				BBox bbBelow, bbAbove;
				for (unsigned int j = begin; j < end; j++) {
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
				float eb = (nAbove == 0 || nBelow == 0) ? emptyBonus : 0.f;
				float cost = traversalCost + isectCost * (1.f - eb) * (pBelow * nBelow + pAbove * nAbove);
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

unsigned int BVHAccel::BuildArray(BVHAccelTreeNode *node, unsigned int offset) {
	// Build array by recursively traversing the tree depth-first
	while (node) {
		BVHAccelArrayNode *p = &bvhTree[offset];

		p->bbox = node->bbox;
		p->primitive = node->primitive;
		offset = BuildArray(node->leftChild, offset + 1);
		p->skipIndex = offset;

		node = node->rightSibling;
	}

	return offset;
}

bool BVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
	assert (initialized);

	unsigned int currentNode = 0; // Root Node
	unsigned int stopNode = bvhTree[0].skipIndex; // Non-existent
	bool hit = false;
	rayHit->t = std::numeric_limits<float>::infinity();
	rayHit->index = 0xffffffffu;
	RayHit triangleHit;

	const Point *vertices = preprocessedMesh->GetVertices();
	const Triangle *triangles = preprocessedMesh->GetTriangles();

	while (currentNode < stopNode) {
		if (bvhTree[currentNode].bbox.IntersectP(*ray)) {
			if (bvhTree[currentNode].primitive != 0xffffffffu) {
				if (triangles[bvhTree[currentNode].primitive].Intersect(*ray, vertices, &triangleHit)) {
					hit = true; // Continue testing for closer intersections
					if (triangleHit.t < rayHit->t) {
						rayHit->t = triangleHit.t;
						rayHit->b1 = triangleHit.b1;
						rayHit->b2 = triangleHit.b2;
						rayHit->index = bvhTree[currentNode].primitive;
					}
				}
			}

			currentNode++;
		} else
			currentNode = bvhTree[currentNode].skipIndex;
	}

	return hit;
}

}
