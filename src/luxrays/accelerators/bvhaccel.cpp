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

// BVHAccel Method Definitions

BVHAccel::BVHAccel(const Context *context) : ctx(context) {
	params = ToBVHParams(ctx->GetConfig());

	initialized = false;
}

BVHAccel::~BVHAccel() {
	if (initialized)
		delete bvhTree;
}

BVHParams BVHAccel::ToBVHParams(const Properties &props) {
	// Tree type to generate (2 = binary, 4 = quad, 8 = octree)
	const int treeType = props.Get(Property("accelerator.bvh.treetype")(4)).Get<int>();
	// Samples to get for cost minimization
	const int costSamples = props.Get(Property("accelerator.bvh.costsamples")(0)).Get<int>();
	const int isectCost = props.Get(Property("accelerator.bvh.isectcost")(80)).Get<int>();
	const int travCost = props.Get(Property("accelerator.bvh.travcost")(10)).Get<int>();
	const float emptyBonus = props.Get(Property("accelerator.bvh.emptybonus")(.5)).Get<float>();
	
	BVHParams params;
	// Make sure treeType is 2, 4 or 8
	if (treeType <= 2) params.treeType = 2;
	else if (treeType <= 4) params.treeType = 4;
	else params.treeType = 8;

	params.costSamples = costSamples;
	params.isectCost = isectCost;
	params.traversalCost = travCost;
	params.emptyBonus = emptyBonus;

	return params;
}

void BVHAccel::Init(const deque<const Mesh *> &ms, const u_longlong totVert,
		const u_longlong totTri) {
	assert (!initialized);

	meshes = ms;
	totalVertexCount = totVert;
	totalTriangleCount = totTri;

	// Handle the empty DataSet case
	if (totalTriangleCount == 0) {
		LR_LOG(ctx, "Empty BVH");
		nNodes = 0;
		bvhTree = NULL;
		initialized = true;

		return;
	}

	const double t0 = WallClockTime();

	//--------------------------------------------------------------------------
	// Build the list of triangles
	//--------------------------------------------------------------------------
	
	vector<BVHTreeNode> bvNodes(totalTriangleCount);
	vector<BVHTreeNode *> bvList(totalTriangleCount, NULL);
	u_int meshIndex = 0;
	u_int bvListIndex = 0;
	BOOST_FOREACH(const Mesh *mesh, meshes) {
		const Triangle *p = mesh->GetTriangles();
		const u_int triangleCount = mesh->GetTotalTriangleCount();

		#pragma omp parallel for
		for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int i = 0; i < triangleCount; ++i) {
			const int index = bvListIndex + i;
			BVHTreeNode *node = &bvNodes[index];

			node->bbox = Union(
					BBox(mesh->GetVertex(0.f, p[i].v[0]), mesh->GetVertex(0.f, p[i].v[1])),
					mesh->GetVertex(0.f, p[i].v[2]));
			// NOTE - Ratow - Expand bbox a little to make sure rays collide
			node->bbox.Expand(MachineEpsilon::E(node->bbox));
			node->triangleLeaf.meshIndex = meshIndex;
			node->triangleLeaf.triangleIndex = i;

			node->leftChild = NULL;
			node->rightSibling = NULL;

			bvList[index] = node;
		}
		
		bvListIndex += triangleCount;
		++meshIndex;
	}

	LR_LOG(ctx, "BVH Dataset preprocessing time: " << int((WallClockTime() - t0) * 1000) << "ms");

	//--------------------------------------------------------------------------
	// Build the BVH hierarchy
	//--------------------------------------------------------------------------

	const double t1 = WallClockTime();
	LR_LOG(ctx, "Building BVH, primitives: " << totalTriangleCount);

	BVHTreeNode *rootNode;
	const string builderType = ctx->GetConfig().Get(Property("accelerator.bvh.builder.type")("EMBREE")).Get<string>();
	LR_LOG(ctx, "BVH builder: " << builderType);
	if (builderType == "EMBREE") {
		rootNode = BuildEmbreeBVH(params, bvList);
		nNodes = CountBVHNodes(rootNode);
	} else if (builderType == "CLASSIC") {
		nNodes = 0;
		rootNode = BuildBVH(&nNodes, params, bvList);
	} else
		throw runtime_error("unknown BVh builder type in BVHAccel::Init(): " + builderType);

	LR_LOG(ctx, "BVH build hierarchy time: " << int((WallClockTime() - t1) * 1000) << "ms");

	//--------------------------------------------------------------------------
	// Pack the BVH hierarchy in an array
	//--------------------------------------------------------------------------

	const double t2 = WallClockTime();

	LR_LOG(ctx, "Pre-processing BVH, total nodes: " << nNodes);
	bvhTree = new luxrays::ocl::BVHAccelArrayNode[nNodes];
	BuildArray(&meshes, rootNode, 0, bvhTree);
	FreeBVH(rootNode);
	LR_LOG(ctx, "BVH build array time: " << int((WallClockTime() - t2) * 1000) << "ms");

	//--------------------------------------------------------------------------
	// Done
	//--------------------------------------------------------------------------

	LR_LOG(ctx, "Total BVH memory usage: " << nNodes * sizeof(luxrays::ocl::BVHAccelArrayNode) / 1024 << "Kbytes");
	LR_LOG(ctx, "BVH build time: " << int((WallClockTime() - t0) * 1000) << "ms");

	initialized = true;
}

u_int BVHAccel::BuildArray(const deque<const Mesh *> *meshes, BVHTreeNode *node,
		u_int offset, luxrays::ocl::BVHAccelArrayNode *bvhTree) {
	// Build array by recursively traversing the tree depth-first
	while (node) {
		luxrays::ocl::BVHAccelArrayNode *p = &bvhTree[offset];

		if (node->leftChild) {
			// It is a BVH node
			memcpy(&p->bvhNode.bboxMin[0], &node->bbox, sizeof(float) * 6);
			offset = BuildArray(meshes, node->leftChild, offset + 1, bvhTree);
			p->nodeData = offset;
		} else {
			// It is a leaf
			if (meshes) {
				// It is a BVH of triangles
				const Triangle *triangles = (*meshes)[node->triangleLeaf.meshIndex]->GetTriangles();
				const Triangle *triangle = &triangles[node->triangleLeaf.triangleIndex];
				p->triangleLeaf.v[0] = triangle->v[0];
				p->triangleLeaf.v[1] = triangle->v[1];
				p->triangleLeaf.v[2] = triangle->v[2];
				p->triangleLeaf.meshIndex = node->triangleLeaf.meshIndex;
				p->triangleLeaf.triangleIndex = node->triangleLeaf.triangleIndex;
			} else {
				// It is a BVH of BVHs (i.e. MBVH)
				p->bvhLeaf.leafIndex = node->bvhLeaf.leafIndex;
				p->bvhLeaf.transformIndex = node->bvhLeaf.transformIndex;
				p->bvhLeaf.motionIndex = node->bvhLeaf.motionIndex;
				p->bvhLeaf.meshOffsetIndex = node->bvhLeaf.meshOffsetIndex;
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

	rayHit->t = initialRay->maxt;
	rayHit->SetMiss();
	if (!nNodes)
		return false;

	Ray ray(*initialRay);

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(bvhTree[0].nodeData); // Non-existent

	float t, b1, b2;
	while (currentNode < stopNode) {
		const luxrays::ocl::BVHAccelArrayNode &node = bvhTree[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the triangle
			const Mesh *mesh = meshes[node.triangleLeaf.meshIndex];
			const Point p0 = mesh->GetVertex(0.f, node.triangleLeaf.v[0]);
			const Point p1 = mesh->GetVertex(0.f, node.triangleLeaf.v[1]);
			const Point p2 = mesh->GetVertex(0.f, node.triangleLeaf.v[2]);

			if (Triangle::Intersect(ray, p0, p1, p2, &t, &b1, &b2)) {
				if (t < rayHit->t) {
					ray.maxt = t;
					rayHit->t = t;
					rayHit->b1 = b1;
					rayHit->b2 = b2;
					rayHit->meshIndex = node.triangleLeaf.meshIndex;
					rayHit->triangleIndex = node.triangleLeaf.triangleIndex;
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
