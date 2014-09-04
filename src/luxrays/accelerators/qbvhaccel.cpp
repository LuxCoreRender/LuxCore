/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/accelerators/qbvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"
#endif

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

//------------------------------------------------------------------------------
// Intersection kernel using normal OpenCL buffers for storing data
//------------------------------------------------------------------------------

class OpenCLQBVHKernels : public OpenCLKernels {
public:
	OpenCLQBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, u_int s) :
		OpenCLKernels(dev, kernelCount), trisBuff(NULL), qbvhBuff(NULL) {
		stackSize = s;

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

		std::stringstream kernelDefs;
		kernelDefs << "#define QBVH_STACK_SIZE " << stackSize << "\n";
		// Use local memory only if not running on a CPU
		if (device->GetType() != DEVICE_TYPE_OPENCL_CPU)
			kernelDefs << "#define QBVH_USE_LOCAL_MEMORY\n";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH kernel definitions: \n" << kernelDefs.str());

		intersectionKernelSource = kernelDefs.str() +
				luxrays::ocl::KernelSource_qbvh_types +
				luxrays::ocl::KernelSource_qbvh;

		const std::string code(
			kernelDefs.str() +
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_epsilon_types +
			luxrays::ocl::KernelSource_epsilon_funcs +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_ray_funcs +
			luxrays::ocl::KernelSource_bbox_types +
			luxrays::ocl::KernelSource_qbvh_types +
			luxrays::ocl::KernelSource_qbvh);
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] QBVH compilation error:\n" << strError.c_str());

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
				//	"] QBVH kernel work group size: " << workGroupSize);

				if ((device->GetType() != DEVICE_TYPE_OPENCL_CPU) && (workGroupSize > 256)) {
					// Otherwise I will probably run out of local memory
					workGroupSize = 256;
					//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
					//	"] Cap work group size to: " << workGroupSize);
				}
			}
		}
	}
	virtual ~OpenCLQBVHKernels() {
		device->FreeMemory(trisBuff->getInfo<CL_MEM_SIZE>());
		delete trisBuff;
		trisBuff = NULL;
		device->FreeMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());
		delete qbvhBuff;
		qbvhBuff = NULL;
	}

	void SetBuffers(cl::Buffer *trisBuff, cl::Buffer *qbvhBuff);
	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex);

protected:
	// QBVH fields
	cl::Buffer *trisBuff;
	cl::Buffer *qbvhBuff;
};

void OpenCLQBVHKernels::SetBuffers(cl::Buffer *t, cl::Buffer *q) {
	trisBuff = t;
	qbvhBuff = q;

	// Set arguments
	BOOST_FOREACH(cl::Kernel *kernel, kernels)
		SetIntersectionKernelArgs(*kernel, 3);
}

void OpenCLQBVHKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
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

u_int OpenCLQBVHKernels::SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int index) {
	u_int argIndex = index;
	kernel.setArg(argIndex++, *qbvhBuff);
	kernel.setArg(argIndex++, *trisBuff);

	// I use local memory only if I'm not running on a CPU
	if (device->GetType() != DEVICE_TYPE_OPENCL_CPU) {
		// Check if we have enough local memory
		if (stackSize * workGroupSize * sizeof(cl_int) >
			device->GetOpenCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>())
			throw std::runtime_error("Not enough OpenCL device local memory available for the required work group size"
				" and QBVH stack depth (try to reduce the work group size and/or the stack depth)");
	
		kernel.setArg(argIndex++, stackSize * workGroupSize * sizeof(cl_int), NULL);
	}

	return argIndex;
}

//------------------------------------------------------------------------------
// Intersection kernel using OpenCL image for storing data
//------------------------------------------------------------------------------

class OpenCLQBVHImageKernels : public OpenCLKernels {
public:
	OpenCLQBVHImageKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, u_int s) :
		OpenCLKernels(dev, kernelCount), trisBuff(NULL), qbvhBuff(NULL) {
		stackSize = s;

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

		std::stringstream kernelDefs;
		kernelDefs << "#define QBVH_STACK_SIZE " << stackSize << "\n"
				"#define USE_IMAGE_STORAGE\n";
		// Use local memory only if not running on a CPU
		if (device->GetType() != DEVICE_TYPE_OPENCL_CPU)
			kernelDefs << "#define QBVH_USE_LOCAL_MEMORY\n";
		//LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] QBVH kernel definitions: \n" << kernelDefs.str());

		intersectionKernelSource = kernelDefs.str() + luxrays::ocl::KernelSource_qbvh;

		const std::string code(
			kernelDefs.str() +
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_epsilon_types +
			luxrays::ocl::KernelSource_epsilon_funcs +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_ray_funcs +
			luxrays::ocl::KernelSource_bbox_types +
			luxrays::ocl::KernelSource_qbvh_types +
			luxrays::ocl::KernelSource_qbvh);
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, opts.str().c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] QBVH Image Storage compilation error:\n" <<
				strError.c_str());

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
				//	"] QBVH kernel work group size: " << workGroupSize);

				if ((device->GetType() != DEVICE_TYPE_OPENCL_CPU) && (workGroupSize > 256)) {
					// Otherwise I will probably run out of local memory
					workGroupSize = 256;
					//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
					//	"] Cap work group size to: " << workGroupSize);
				}
			}
		}
	}
	virtual ~OpenCLQBVHImageKernels() {
		device->FreeMemory(trisBuff->getInfo<CL_MEM_SIZE>());
		delete trisBuff;
		trisBuff = NULL;
		device->FreeMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());
		delete qbvhBuff;
		qbvhBuff = NULL;
	}

	void SetBuffers(cl::Image2D *trisBuff, cl::Image2D *qbvhBuff);
	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex);

protected:
	// QBVH with image storage fields
	cl::Image2D *trisBuff;
	cl::Image2D *qbvhBuff;
};

void OpenCLQBVHImageKernels::SetBuffers(cl::Image2D *t, cl::Image2D *q) {
	trisBuff = t;
	qbvhBuff = q;

	// Set arguments
	BOOST_FOREACH(cl::Kernel *kernel, kernels)
		SetIntersectionKernelArgs(*kernel, 3);
}

void OpenCLQBVHImageKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
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

u_int OpenCLQBVHImageKernels::SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int index) {
	u_int argIndex = index;
	kernel.setArg(argIndex++, *qbvhBuff);
	kernel.setArg(argIndex++, *trisBuff);

	// I use local memory only if I'm not running on a CPU
	if (device->GetType() != DEVICE_TYPE_OPENCL_CPU) {
		// Check if we have enough local memory
		if (stackSize * workGroupSize * sizeof(cl_int) >
			device->GetOpenCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>())
			throw std::runtime_error("Not enough OpenCL device local memory available for the required work group size"
				" and QBVH stack depth (try to reduce the work group size and/or the stack depth)");

		kernel.setArg(argIndex++, stackSize * workGroupSize * sizeof(cl_int), NULL);
	}

	return argIndex;
}

//------------------------------------------------------------------------------

OpenCLKernels *QBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	const std::string &deviceName(device->GetName());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] QBVH max. stack size: " << stackSize);

	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	bool useImage = true;
	size_t nodeWidth, nodeHeight, leafWidth, leafHeight;
	// Check if I can use image to store the data set
	if (!enableImageStorage) {
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] Disable forced for QBVH scene storage inside image");
		useImage = false;
	} else if (!deviceDesc->HasImageSupport() ||
		(!(deviceDesc->GetType() & DEVICE_TYPE_OPENCL_GPU))) {
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] OpenCL image support is not available");
		useImage = false;
	} else {
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] OpenCL image support is available");

		// Check if the scene is small enough to be stored inside an image
		const size_t maxWidth = deviceDesc->GetImage2DMaxWidth();
		const size_t maxHeight = deviceDesc->GetImage2DMaxHeight();

		// Calculate the required image size for the storage

		// 7 pixels required for the storage of a QBVH node
		const size_t nodePixelRequired = nNodes * 7;
		nodeWidth = Min(RoundUp(static_cast<u_int>(sqrtf(nodePixelRequired)), 7u),  (0x7fffu / 7u) * 7u);
		nodeHeight = nodePixelRequired / nodeWidth +
			(((nodePixelRequired % nodeWidth) == 0) ? 0 : 1);

		// 11 pixels required for the storage of QBVH Triangles
		const size_t leafPixelRequired = nQuads * 11;
		leafWidth = Min(RoundUp(static_cast<u_int>(sqrtf(leafPixelRequired)), 11u), (0x7fffu / 11u) * 11u);
		leafHeight = leafPixelRequired / leafWidth +
			(((leafPixelRequired % leafWidth) == 0) ? 0 : 1);

		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] OpenCL max. image buffer size: " <<
			maxWidth << "x" << maxHeight);

		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] QBVH node image buffer size: " <<
			nodeWidth << "x" << nodeHeight);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] QBVH triangle image buffer size: " <<
			leafWidth << "x" << leafHeight);

		if ((nodeWidth > maxWidth) || (nodeHeight > maxHeight) ||
			(leafWidth > maxWidth) || (leafHeight > maxHeight)) {
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] OpenCL image max. image size supported is too small");
			useImage = false;
		} else {
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] Enabled QBVH scene storage inside image");
			useImage = true;
		}
	}
	if (!useImage) {
		// Allocate buffers
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] QBVH buffer size: " <<
			(sizeof(QBVHNode) * nNodes / 1024) << "Kbytes");
		cl::Buffer *qbvhBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(QBVHNode) * nNodes, nodes);
		device->AllocMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());

		LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
			"] QuadTriangle buffer size: " <<
			(sizeof(QuadTriangle) * nQuads / 1024) << "Kbytes");
		cl::Buffer *trisBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(QuadTriangle) * nQuads, prims);
		device->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());

		// Setup kernels
		OpenCLQBVHKernels *kernels = new OpenCLQBVHKernels(device, kernelCount, stackSize);
		kernels->SetBuffers(trisBuff, qbvhBuff);

		return kernels;
	} else {
		// Allocate image buffers
		u_int *inodes = new u_int[nodeWidth * nodeHeight * 4];
		for (size_t i = 0; i < nNodes; ++i) {
			u_int *pnodes = (u_int *)(nodes + i);
			const size_t offset = i * 7 * 4;

			for (size_t j = 0; j < 6 * 4; ++j)
				inodes[offset + j] = pnodes[j];

			for (size_t j = 0; j < 4; ++j) {
				int index = nodes[i].children[j];

				if (QBVHNode::IsEmpty(index)) {
					inodes[offset + 6 * 4 + j] = index;
				} else if (QBVHNode::IsLeaf(index)) {
					const int32_t count = QBVHNode::FirstQuadIndex(index) * 11;
					// "/ 11" in order to not waste bits
					const unsigned short x = static_cast<unsigned short>((count % leafWidth) / 11);
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
		cl::Image2D *qbvhBuff = new cl::Image2D(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT32),
			nodeWidth, nodeHeight, 0, inodes);
		device->AllocMemory(qbvhBuff->getInfo<CL_MEM_SIZE>());
		delete[] inodes;

		u_int *iprims = new u_int[leafWidth * leafHeight * 4];
		memcpy(iprims, prims, sizeof(QuadTriangle) * nQuads);
		cl::Image2D *trisBuff = new cl::Image2D(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT32),
			leafWidth, leafHeight, 0, iprims);
		device->AllocMemory(trisBuff->getInfo<CL_MEM_SIZE>());
		delete[] iprims;

		// Setup kernels
		OpenCLQBVHImageKernels *kernels = new OpenCLQBVHImageKernels(device, kernelCount, stackSize);
		kernels->SetBuffers(trisBuff, qbvhBuff);

		return kernels;
	}
}

bool QBVHAccel::CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const {
	const OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();

	// Check if I can allocate enough space for the accelerator data
	if (sizeof(QBVHNode) * nNodes > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because node buffer is too big: " <<
			(sizeof(QBVHNode) * nNodes / 1024) << "Kbytes");
		return false;
	}
	if (sizeof(QuadTriangle) * nQuads > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because triangle buffer is too big: " <<
			(sizeof(QuadTriangle) * nQuads / 1024) << "Kbytes");
		return false;
	}
	return true;
}

#else

OpenCLKernels *QBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device, const u_int kernelCount,
		const u_int stackSize, const bool enableImageStorage) const {
	return NULL;
}

bool QBVHAccel::CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const {
	return false;
}

#endif

QBVHAccel::QBVHAccel(const Context *context,
		u_int mp, u_int fst, u_int sf) : fullSweepThreshold(fst),
		skipFactor(sf), maxPrimsPerLeaf(mp), ctx(context) {
	initialized = false;
	maxDepth = 0;
}

QBVHAccel::~QBVHAccel() {
	if (initialized) {
		FreeAligned(prims);
		FreeAligned(nodes);
	}
}

void QBVHAccel::Init(const std::deque<const Mesh *> &ms, const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	assert (!initialized);

	meshes = ms;

	// Temporary data for building
	std::vector<u_int> meshIndexes(totalTriangleCount + 3);
	std::vector<u_int> triangleIndexes(totalTriangleCount + 3); // For the case where
	// the last quad would begin at the last primitive
	// (or the second or third last primitive)

	// The number of nodes depends on the number of primitives,
	// and is bounded by 2 * nPrims - 1.
	// Even if there will normally have at least 4 primitives per leaf,
	// it is not always the case => continue to use the normal bounds.
	nNodes = 0;
	maxNodes = 1;
	for (u_int layer = ((totalTriangleCount + maxPrimsPerLeaf - 1) / maxPrimsPerLeaf + 3) / 4; layer != 1; layer = (layer + 3) / 4)
		maxNodes += layer;
	nodes = AllocAligned<QBVHNode>(maxNodes);
	for (u_int i = 0; i < maxNodes; ++i)
		nodes[i] = QBVHNode();

	// The arrays that will contain
	// - the bounding boxes for all triangles
	// - the centroids for all triangles
	std::vector<std::vector<BBox> > primsBboxes(totalTriangleCount);
	std::vector<std::vector<Point> > primsCentroids(totalTriangleCount);
	// The bouding volume of all the centroids
	BBox centroidsBbox;

	// Fill each base array
	u_int absoluteIndex = 0;
	for (u_int i = 0; i < meshes.size(); ++i) {
		const Mesh *mesh = meshes[i];
		const Triangle *p = mesh->GetTriangles();
		primsBboxes[i].resize(mesh->GetTotalTriangleCount());
		primsCentroids[i].resize(mesh->GetTotalTriangleCount());

		for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
			// This array will be reorganized during construction.
			meshIndexes[absoluteIndex] = i;
			triangleIndexes[absoluteIndex++] = j;

			// Compute the bounding box for the triangle
			primsBboxes[i][j] = Union(
					BBox(mesh->GetVertex(0.f, p[j].v[0]), mesh->GetVertex(0.f, p[j].v[1])),
					mesh->GetVertex(0.f, p[j].v[2]));
			primsBboxes[i][j].Expand(MachineEpsilon::E(primsBboxes[i][j]));
			primsCentroids[i][j] = (primsBboxes[i][j].pMin + primsBboxes[i][j].pMax) * .5f;

			// Update the global bounding boxes
			worldBound = Union(worldBound, primsBboxes[i][j]);
			centroidsBbox = Union(centroidsBbox, primsCentroids[i][j]);
		}
	}

	// Arbitrarily take the last primitive for the last 3
	meshIndexes[totalTriangleCount] = meshIndexes[totalTriangleCount - 1];
	meshIndexes[totalTriangleCount + 1] = meshIndexes[totalTriangleCount - 1];
	meshIndexes[totalTriangleCount + 2] = meshIndexes[totalTriangleCount - 1];
	triangleIndexes[totalTriangleCount] = triangleIndexes[totalTriangleCount - 1];
	triangleIndexes[totalTriangleCount + 1] = triangleIndexes[totalTriangleCount - 1];
	triangleIndexes[totalTriangleCount + 2] = triangleIndexes[totalTriangleCount - 1];

	// Recursively build the tree
	LR_LOG(ctx, "Building QBVH, primitives: " << totalTriangleCount << ", initial nodes: " << maxNodes);

	nQuads = 0;
	BuildTree(0, totalTriangleCount, meshIndexes, triangleIndexes, primsBboxes, primsCentroids,
			worldBound, centroidsBbox, -1, 0, 0);

	prims = AllocAligned<QuadTriangle>(nQuads);
	nQuads = 0;
	PreSwizzle(0, meshIndexes, triangleIndexes);

	LR_LOG(ctx, "QBVH completed with " << nNodes << "/" << maxNodes << " nodes");
	LR_LOG(ctx, "Total QBVH memory usage: " << nNodes * sizeof(QBVHNode) / 1024 << "Kbytes");
	LR_LOG(ctx, "Total QBVH QuadTriangle count: " << nQuads);
	LR_LOG(ctx, "Max. QBVH Depth: " << maxDepth);

	initialized = true;
}

/***************************************************/

void QBVHAccel::BuildTree(u_int start, u_int end, std::vector<u_int> &meshIndexes, std::vector<u_int> &triangleIndexes,
		std::vector<std::vector<BBox> > &primsBboxes, std::vector<std::vector<Point> > &primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex, int32_t childIndex, int depth) {
	maxDepth = (depth >= maxDepth) ? depth : maxDepth; // Set depth so we know how much stack we need later.

	// Create a leaf ?
	//********
	if (depth > 64 || end - start <= maxPrimsPerLeaf) {
		if (depth > 64) {
			LR_LOG(ctx, "Maximum recursion depth reached while constructing QBVH, forcing a leaf node");
			if (end - start > 64) {
				LR_LOG(ctx, "QBVH unable to handle geometry, too many primitives in leaf");
			}
		}
		CreateTempLeaf(parentIndex, childIndex, start, end, nodeBbox);
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

	// Precompute values that are constant with respect to the current
	// primitive considered.
	const float k0 = centroidsBbox.pMin[axis];
	const float k1 = NB_BINS / (centroidsBbox.pMax[axis] - k0);

	// If the bbox is a point, create a leaf, hoping there are not more
	// than 64 primitives that share the same center.
	if (k1 == INFINITY) {
		if (end - start > 64)
			LR_LOG(ctx, "QBVH unable to handle geometry, too many primitives with the same centroid");
		CreateTempLeaf(parentIndex, childIndex, start, end, nodeBbox);
		return;
	}

	// Create an intermediate node if the depth indicates to do so.
	// Register the split axis.
	if (depth % 2 == 0) {
		currentNode = CreateIntermediateNode(parentIndex, childIndex, nodeBbox);
		leftChildIndex = 0;
		rightChildIndex = 2;
	}

	for (u_int i = start; i < end; i += step) {
		const u_int mIndex = meshIndexes[i];
		const u_int tIndex = triangleIndexes[i];

		// Binning is relative to the centroids bbox and to the
		// primitives' centroid.
		const int binId = Min(NB_BINS - 1, Floor2Int(k1 * (primsCentroids[mIndex][tIndex][axis] - k0)));

		bins[binId]++;
		binsBbox[binId] = Union(binsBbox[binId], primsBboxes[mIndex][tIndex]);
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
		const int rightIndex = NB_BINS - 1 - i;
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
	const float splitPos = centroidsBbox.pMin[axis] + (minBin + 1) *
			(centroidsBbox.pMax[axis] - centroidsBbox.pMin[axis]) / NB_BINS;


	BBox leftChildBbox, rightChildBbox;
	BBox leftChildCentroidsBbox, rightChildCentroidsBbox;

	u_int storeIndex = start;
	for (u_int i = start; i < end; ++i) {
		const u_int mIndex = meshIndexes[i];
		const u_int tIndex = triangleIndexes[i];

		if (primsCentroids[mIndex][tIndex][axis] <= splitPos) {
			// Swap
			meshIndexes[i] = meshIndexes[storeIndex];
			meshIndexes[storeIndex] = mIndex;
			triangleIndexes[i] = triangleIndexes[storeIndex];
			triangleIndexes[storeIndex] = tIndex;
			++storeIndex;

			// Update the bounding boxes,
			// this triangle is on the left side
			leftChildBbox = Union(leftChildBbox, primsBboxes[mIndex][tIndex]);
			leftChildCentroidsBbox = Union(leftChildCentroidsBbox, primsCentroids[mIndex][tIndex]);
		} else {
			// Update the bounding boxes,
			// this triangle is on the right side.
			rightChildBbox = Union(rightChildBbox, primsBboxes[mIndex][tIndex]);
			rightChildCentroidsBbox = Union(rightChildCentroidsBbox, primsCentroids[mIndex][tIndex]);
		}
	}

	// Build recursively
	BuildTree(start, storeIndex, meshIndexes, triangleIndexes, primsBboxes, primsCentroids,
			leftChildBbox, leftChildCentroidsBbox, currentNode,
			leftChildIndex, depth + 1);
	BuildTree(storeIndex, end, meshIndexes, triangleIndexes, primsBboxes, primsCentroids,
			rightChildBbox, rightChildCentroidsBbox, currentNode,
			rightChildIndex, depth + 1);
}

/***************************************************/

void QBVHAccel::CreateTempLeaf(int32_t parentIndex, int32_t childIndex,
		u_int start, u_int end, const BBox &nodeBbox) {
	// The leaf is directly encoded in the intermediate node.
	if (parentIndex < 0) {
		// The entire tree is a leaf
		nNodes = 1;
		parentIndex = 0;
	}

	// Encode the leaf in the original way,
	// it will be transformed to a preswizzled format in a post-process.

	u_int nbPrimsTotal = end - start;

	QBVHNode &node = nodes[parentIndex];

	node.SetBBox(childIndex, nodeBbox);


	// Next multiple of 4, divided by 4
	u_int quads = (nbPrimsTotal + 3) / 4;

	// Use the same encoding as the final one, but with a different meaning.
	node.InitializeLeaf(childIndex, quads, start);

	nQuads += quads;
}

void QBVHAccel::PreSwizzle(int32_t nodeIndex, std::vector<u_int> &meshIndexes, std::vector<u_int> &triangleIndexes) {
	for (int i = 0; i < 4; ++i) {
		if (nodes[nodeIndex].ChildIsLeaf(i))
			CreateSwizzledLeaf(nodeIndex, i, meshIndexes, triangleIndexes);
		else
			PreSwizzle(nodes[nodeIndex].children[i], meshIndexes, triangleIndexes);
	}
}

void QBVHAccel::CreateSwizzledLeaf(int32_t parentIndex, int32_t childIndex,
		std::vector<u_int> &meshIndexes, std::vector<u_int> &triangleIndexes) {
	QBVHNode &node = nodes[parentIndex];
	if (node.LeafIsEmpty(childIndex))
		return;
	const u_int startQuad = nQuads;
	const u_int nbQuads = node.NbQuadsInLeaf(childIndex);

	u_int primOffset = node.FirstQuadIndexForLeaf(childIndex);
	u_int primNum = nQuads;

	for (u_int q = 0; q < nbQuads; ++q) {
		new (&prims[primNum]) QuadTriangle(meshes,
				meshIndexes[primOffset], meshIndexes[primOffset + 1], meshIndexes[primOffset + 2], meshIndexes[primOffset + 3],
				triangleIndexes[primOffset], triangleIndexes[primOffset + 1], triangleIndexes[primOffset + 2], triangleIndexes[primOffset + 3]);

		++primNum;
		primOffset += 4;
	}
	nQuads += nbQuads;
	node.InitializeLeaf(childIndex, nbQuads, startQuad);
}

/***************************************************/

bool QBVHAccel::Intersect(const Ray *initialRay, RayHit *rayHit) const {
	Ray ray(*initialRay);
	rayHit->SetMiss();

	//------------------------------
	// Prepare the ray for intersection
	QuadRay ray4(ray);
	__m128 invDir[3];
	invDir[0] = _mm_set1_ps(1.f / ray.d.x);
	invDir[1] = _mm_set1_ps(1.f / ray.d.y);
	invDir[2] = _mm_set1_ps(1.f / ray.d.z);

	int signs[3];
	ray.GetDirectionSigns(signs);

	//------------------------------
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
			//----------------------
			// It is a leaf,
			// all the informations are encoded in the index
			const int32_t leafData = nodeStack[todoNode];
			--todoNode;

			if (QBVHNode::IsEmpty(leafData))
				continue;

			// Perform intersection
			const u_int nbQuadPrimitives = QBVHNode::NbQuadPrimitives(leafData);

			const u_int offset = QBVHNode::FirstQuadIndex(leafData);

			for (u_int primNumber = offset; primNumber < (offset + nbQuadPrimitives); ++primNumber)
				prims[primNumber].Intersect(ray4, ray, rayHit);
		}//end of the else
	}

	return !rayHit->Miss();
}

}
