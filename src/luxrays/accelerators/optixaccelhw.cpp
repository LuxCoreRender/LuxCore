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

#include "luxrays/core/context.h"
#include "luxrays/devices/cudaintersectiondevice.h"
#include "luxrays/accelerators/optixaccel.h"
#include "luxrays/kernels/kernels.h"
#include "luxrays/utils/oclcache.h"
#include "luxrays/core/exttrianglemesh.h"

using namespace std;

namespace luxrays {

//------------------------------------------------------------------------------
// This must match the definition in optixaccel.cl

typedef struct Params {
	OptixTraversableHandle optixHandle;
	CUdeviceptr rayBuff;
	CUdeviceptr rayHitBuff;
} OptixAccelParams;

typedef struct {
	unsigned int meshIndex;
} HitGroupSbtData;

//------------------------------------------------------------------------------

template <typename T>
struct OptixSbtRecord {
    alignas(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

typedef OptixSbtRecord<HitGroupSbtData> HitGroupSbtRecord;

class OptixKernel : public HardwareIntersectionKernel {
public:
	OptixKernel(HardwareIntersectionDevice &dev, const OptixAccel &optixAccel) :
			HardwareIntersectionKernel(dev), 
			optixModule(nullptr), optixRaygenProgGroup(nullptr),
			optixMissProgGroup(nullptr), optixHitProgGroup(nullptr),
			optixPipeline(nullptr), optixAccelParamsBuff(nullptr),
			optixRayGenSbtBuff(nullptr), optixMissSbtBuff(nullptr),
			optixHitSbtBuff(nullptr) {
		CUDAIntersectionDevice *cudaDevice = dynamic_cast<CUDAIntersectionDevice *>(&dev);

		// Safety checks
		if (!cudaDevice)
			throw runtime_error("Used a no CUDA device in OptixKernel::OptixKernel(): " + DeviceDescription::GetDeviceType(dev.GetDeviceDesc()->GetType()));
		if (!cudaDevice->GetOptixContext())
			throw runtime_error("No Optix context in OptixKernel::OptixKernel()");

		// TODO
		// Handle the empty DataSet case
		// TODO

		const double t0 = WallClockTime();

		LR_LOG(device.GetContext(), "Building Optix accelerator");

		OptixDeviceContext optixContext = cudaDevice->GetOptixContext();

		//----------------------------------------------------------------------
		// Build Optix accelerator structure
		//----------------------------------------------------------------------

		// Build all accelerator bottom nodes

		vector<OptixInstance> optixInstances;
		vector<OptixAabb> optixBBs;
		
		auto uniqueMeshTraversableHandle = map<const Mesh *, OptixTraversableHandle,
				function<bool(const Mesh *, const Mesh *)>>{
			[](const Mesh *p0, const Mesh *p1) {
				return p0 < p1;
			}
		};

		bool usesMotionBlur = false;
		for (u_int i = 0; i < optixAccel.meshes.size(); ++i) {
			const Mesh *mesh = optixAccel.meshes[i];
			
			switch (mesh->GetType()) {
				case TYPE_TRIANGLE:
				case TYPE_EXT_TRIANGLE: {
					const TriangleMesh *tm = dynamic_cast<const TriangleMesh *>(mesh);

					OptixTraversableHandle handle;
					HardwareDeviceBuffer *outputBuffer = nullptr;

					BuildTraversable(tm, handle, &outputBuffer);

					optixOutputBuffers.push_back(outputBuffer);

					// Add an instance of this mesh

					optixInstances.resize(optixInstances.size() + 1);
					OptixInstance &optixInstance = optixInstances[optixInstances.size() - 1];
					memset(&optixInstance, 0, sizeof(OptixInstance));

					// Clear transform to identity matrix
					optixInstance.transform[0] = 1.f;
					optixInstance.transform[5] = 1.f;
					optixInstance.transform[10] = 1.f;
					optixInstance.instanceId = i;
					optixInstance.visibilityMask = 1;
					optixInstance.traversableHandle = handle;
					// Disable transformation
					optixInstance.flags = OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM;
					break;
				}
				case TYPE_TRIANGLE_INSTANCE:
				case TYPE_EXT_TRIANGLE_INSTANCE: {
					const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(mesh);

					// Check if a OptixTraversableHandle has already been created
					auto it = uniqueMeshTraversableHandle.find(itm->GetTriangleMesh());

					OptixTraversableHandle instancedMeshHandle;
					if (it == uniqueMeshTraversableHandle.end()) {
						TriangleMesh *instancedMesh = itm->GetTriangleMesh();

						// Create a new OptixTraversableHandle
						HardwareDeviceBuffer *outputBuffer = nullptr;
						BuildTraversable(instancedMesh, instancedMeshHandle, &outputBuffer);
						optixOutputBuffers.push_back(outputBuffer);

						// Add to the listed of created handles
						uniqueMeshTraversableHandle[instancedMesh] = instancedMeshHandle;
					} else
						instancedMeshHandle = it->second;
					
					// Add an instance of this mesh

					optixInstances.resize(optixInstances.size() + 1);
					OptixInstance &optixInstance = optixInstances[optixInstances.size() - 1];
					memset(&optixInstance, 0, sizeof(OptixInstance));

					// Set transform matrix
					Transform local2World;
					itm->GetLocal2World(0.f, local2World);

					optixInstance.transform[0] = local2World.m.m[0][0];
					optixInstance.transform[1] = local2World.m.m[0][1];
					optixInstance.transform[2] = local2World.m.m[0][2];
					optixInstance.transform[3] = local2World.m.m[0][3];
					optixInstance.transform[4] = local2World.m.m[1][0];
					optixInstance.transform[5] = local2World.m.m[1][1];
					optixInstance.transform[6] = local2World.m.m[1][2];
					optixInstance.transform[7] = local2World.m.m[1][3];
					optixInstance.transform[8] = local2World.m.m[2][0];
					optixInstance.transform[9] = local2World.m.m[2][1];
					optixInstance.transform[10] = local2World.m.m[2][2];
					optixInstance.transform[11] = local2World.m.m[2][3];

					optixInstance.instanceId = i;
					optixInstance.visibilityMask = 1;
					optixInstance.traversableHandle = instancedMeshHandle;
					break;
				}
				case TYPE_TRIANGLE_MOTION:
				case TYPE_EXT_TRIANGLE_MOTION: {
					const MotionTriangleMesh *mtm = dynamic_cast<const MotionTriangleMesh *>(mesh);

					// Check if a OptixTraversableHandle has already been created
					auto it = uniqueMeshTraversableHandle.find(mtm->GetTriangleMesh());

					OptixTraversableHandle instancedMeshHandle;
					if (it == uniqueMeshTraversableHandle.end()) {
						TriangleMesh *instancedMesh = mtm->GetTriangleMesh();

						// Create a new OptixTraversableHandle
						HardwareDeviceBuffer *outputBuffer = nullptr;
						BuildTraversable(instancedMesh, instancedMeshHandle, &outputBuffer);
						optixOutputBuffers.push_back(outputBuffer);

						// Add to the listed of created handles
						uniqueMeshTraversableHandle[instancedMesh] = instancedMeshHandle;
					} else
						instancedMeshHandle = it->second;

					// Create the motion blur traversable

					const MotionSystem &ms = mtm->GetMotionSystem();

					const size_t transformSizeInBytes = sizeof(OptixMatrixMotionTransform) +
							(ms.times.size() - 2) * 12 * sizeof(float);
					unique_ptr<OptixMatrixMotionTransform> motionTransform((OptixMatrixMotionTransform *)new char[transformSizeInBytes]);

					motionTransform->child = instancedMeshHandle;
					motionTransform->motionOptions.numKeys = ms.times.size();
					motionTransform->motionOptions.timeBegin = ms.times.front();
					motionTransform->motionOptions.timeEnd = ms.times.back();
					motionTransform->motionOptions.flags = OPTIX_MOTION_FLAG_NONE;

					float *ptr = &motionTransform->transform[0][0];
					for (auto const t : ms.times) {
						const Matrix4x4 m = ms.SampleInverse(t);

						*ptr++ = m.m[0][0];
						*ptr++ = m.m[0][1];
						*ptr++ = m.m[0][2];
						*ptr++ = m.m[0][3];
						*ptr++ = m.m[1][0];
						*ptr++ = m.m[1][1];
						*ptr++ = m.m[1][2];
						*ptr++ = m.m[1][3];
						*ptr++ = m.m[2][0];
						*ptr++ = m.m[2][1];
						*ptr++ = m.m[2][2];
						*ptr++ = m.m[2][3];
					}

					HardwareDeviceBuffer *optixMotionBuff = nullptr;
					cudaDevice->AllocBufferRO(&optixMotionBuff, motionTransform.get(), transformSizeInBytes);
					optixOutputBuffers.push_back(optixMotionBuff);

					OptixTraversableHandle motionMeshHandle;
					CHECK_OPTIX_ERROR(optixConvertPointerToTraversableHandle(
							optixContext,
							((CUDADeviceBuffer *)optixMotionBuff)->GetCUDADevicePointer(),
							OPTIX_TRAVERSABLE_TYPE_MATRIX_MOTION_TRANSFORM,
							&motionMeshHandle));

					// Add an instance of this mesh with motion blur

					optixInstances.resize(optixInstances.size() + 1);
					OptixInstance &optixInstance = optixInstances[optixInstances.size() - 1];
					memset(&optixInstance, 0, sizeof(OptixInstance));

					// Clear transform to identity matrix
					optixInstance.transform[0] = 1.f;
					optixInstance.transform[5] = 1.f;
					optixInstance.transform[10] = 1.f;
					optixInstance.instanceId = i;
					optixInstance.visibilityMask = 1;
					optixInstance.traversableHandle = motionMeshHandle;
					// Disable transformation
					optixInstance.flags = OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM;

					usesMotionBlur = true;
					break;
				}
				default:
					throw runtime_error("Unsupported mesh type in OptixKernel(): " + ToString(mesh->GetType()));
			}
			
			// Add the bounding box

			optixBBs.resize(optixBBs.size() + 1);
			OptixAabb &optixBB = optixBBs[optixBBs.size() - 1];					
			BBox bb = mesh->GetBBox();
			optixBB.minX = bb.pMin.x;
			optixBB.minY = bb.pMin.y;
			optixBB.minZ = bb.pMin.z;
			optixBB.maxX = bb.pMax.x;
			optixBB.maxY = bb.pMax.y;
			optixBB.maxZ = bb.pMax.z;
		}

		LR_LOG(device.GetContext(), "Optix accelerator leafs: " << optixInstances.size());

		// Allocate  optix instances on device
		HardwareDeviceBuffer *optixInstancesBuff = nullptr;
		cudaDevice->AllocBufferRO(&optixInstancesBuff, &optixInstances[0], sizeof(OptixInstance) * optixInstances.size());
		optixOutputBuffers.push_back(optixInstancesBuff);
		
		HardwareDeviceBuffer *optixBBsBuff = nullptr;
		if (usesMotionBlur) {
			// Allocate optix BBs on device
			cudaDevice->AllocBufferRO(&optixBBsBuff, &optixBBs[0], sizeof(OptixAabb) * optixBBs.size());
			optixOutputBuffers.push_back(optixBBsBuff);
		}

		// Build top level acceleration structure
		
		OptixBuildInput buildInput = {};
		buildInput.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
		buildInput.instanceArray.instances = ((CUDADeviceBuffer *)optixInstancesBuff)->GetCUDADevicePointer();
		buildInput.instanceArray.numInstances = optixInstances.size();
		if (usesMotionBlur) {
			buildInput.instanceArray.aabbs = ((CUDADeviceBuffer *)optixBBsBuff)->GetCUDADevicePointer();
			buildInput.instanceArray.numAabbs = optixBBs.size();
		}
		
		OptixTraversableHandle topLevelHandle;
		HardwareDeviceBuffer *topLevelOutputBuffer = nullptr;

		BuildTraversable(buildInput, topLevelHandle, &topLevelOutputBuffer);
		optixOutputBuffers.push_back(topLevelOutputBuffer);

		LR_LOG(device.GetContext(), "Optix total build time: " << int((WallClockTime() - t0) * 1000) << "ms");
// TODO:
//		LR_LOG(device.GetContext(), "Total Optix memory usage: " << (gasOutputBuffer->GetSize() / 1024) << "Kbytes");

		//----------------------------------------------------------------------
		// Build Optix module
		//----------------------------------------------------------------------

		// Build compile options
		vector<string> cudaProgramParameters;
		cudaProgramParameters.push_back("-D LUXRAYS_OPENCL_KERNEL");
		cudaProgramParameters.push_back("-D PARAM_RAY_EPSILON_MIN=" + ToString(MachineEpsilon::GetMin()) + "f");
		cudaProgramParameters.push_back("-D PARAM_RAY_EPSILON_MAX=" + ToString(MachineEpsilon::GetMax()) + "f");
		cudaProgramParameters = cudaDevice->AddKernelOpts(cudaProgramParameters);
		LR_LOG(device.GetContext(), "[OptixAccel] Compiler options: " << oclKernelPersistentCache::ToOptsString(cudaProgramParameters));

		// Set kernel source
		string kernelSource =
				luxrays::ocl::KernelSource_luxrays_types +
				luxrays::ocl::KernelSource_epsilon_types +
				luxrays::ocl::KernelSource_epsilon_funcs +
				luxrays::ocl::KernelSource_point_types +
				luxrays::ocl::KernelSource_vector_types +
				luxrays::ocl::KernelSource_ray_types +
				luxrays::ocl::KernelSource_ray_funcs +
				luxrays::ocl::KernelSource_optixaccel;
		kernelSource = cudaDevice->GetKernelSource(kernelSource);
		
		char *ptx;
		size_t ptxSize;
		bool cached;
		string ptxError;
		if (!cudaDevice->GetCUDAKernelCache()->CompilePTX(cudaProgramParameters,
				kernelSource, "OptixAccel", &ptx, &ptxSize, &cached, &ptxError)) {
			LR_LOG(device.GetContext(), "[OptixAccel] CUDA program compilation error: " << endl << ptxError);

			throw runtime_error("OptixAccel CUDA program compilation error");
		}

		if (cached) {
			LR_LOG(device.GetContext(), "[OptixAccel] Program cached");
		} else {
			LR_LOG(device.GetContext(), "[OptixAccel] Program not cached");
		}

		OptixModuleCompileOptions moduleCompileOptions = {};
		moduleCompileOptions.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
		moduleCompileOptions.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
		moduleCompileOptions.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;

		OptixPipelineCompileOptions pipelineCompileOptions = {};
		pipelineCompileOptions.usesMotionBlur = usesMotionBlur;
		pipelineCompileOptions.traversableGraphFlags = usesMotionBlur ?
			OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY : OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
		pipelineCompileOptions.numPayloadValues = 0;
		pipelineCompileOptions.numAttributeValues = 2;
		pipelineCompileOptions.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
		pipelineCompileOptions.pipelineLaunchParamsVariableName = "optixAccelParams";

		char optixErrLog[4096];
		size_t optixErrLogSize = sizeof(optixErrLog);

		OptixResult optixErr = optixModuleCreateFromPTX(
				optixContext,
				&moduleCompileOptions,
				&pipelineCompileOptions,
				ptx,
				ptxSize,
				optixErrLog,
				&optixErrLogSize,
				&optixModule);

		delete[] ptx;

		if (optixErr != OPTIX_SUCCESS) {
			LR_LOG(device.GetContext(), "Optix optixModuleCreateFromPTX() error: " << endl << optixErrLog);
			CHECK_OPTIX_ERROR(optixErr);
		}

		//----------------------------------------------------------------------
		// Build Optix groups
		//----------------------------------------------------------------------

		OptixProgramGroupOptions programGroupOptions = {};

		// Ray generation
		
		OptixProgramGroupDesc raygenProgGroupDesc = {};
		raygenProgGroupDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
		raygenProgGroupDesc.flags = OPTIX_PROGRAM_GROUP_FLAGS_NONE;
		raygenProgGroupDesc.raygen.module = optixModule;
		raygenProgGroupDesc.raygen.entryFunctionName = "__raygen__OptixAccel";

		optixErrLogSize = sizeof(optixErrLog);
		CHECK_OPTIX_ERROR(optixProgramGroupCreate(
				optixContext,
				&raygenProgGroupDesc,
				1,
				&programGroupOptions,
				optixErrLog,
				&optixErrLogSize,
				&optixRaygenProgGroup));

		// Ray miss

		OptixProgramGroupDesc missProgGroupDesc = {};
		missProgGroupDesc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
		missProgGroupDesc.flags = OPTIX_PROGRAM_GROUP_FLAGS_NONE;
		missProgGroupDesc.miss.module = optixModule;
		missProgGroupDesc.miss.entryFunctionName = "__miss__OptixAccel";

		optixErrLogSize = sizeof(optixErrLog);
		CHECK_OPTIX_ERROR(optixProgramGroupCreate(
				optixContext,
				&missProgGroupDesc,
				1,
				&programGroupOptions,
				optixErrLog,
				&optixErrLogSize,
				&optixMissProgGroup));

		// Ray hit

		OptixProgramGroupDesc hitProgGroupDesc = {};
		hitProgGroupDesc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
		hitProgGroupDesc.flags = OPTIX_PROGRAM_GROUP_FLAGS_NONE;
		hitProgGroupDesc.hitgroup.moduleCH = optixModule;
		hitProgGroupDesc.hitgroup.entryFunctionNameCH = "__closesthit__OptixAccel";

		optixErrLogSize = sizeof(optixErrLog);
		CHECK_OPTIX_ERROR(optixProgramGroupCreate(
				optixContext,
				&hitProgGroupDesc,
				1,
				&programGroupOptions,
				optixErrLog,
				&optixErrLogSize,
				&optixHitProgGroup));
		
		//----------------------------------------------------------------------
		// Build Optix pipeline
		//----------------------------------------------------------------------

		OptixProgramGroup programGroups[] = {
			optixRaygenProgGroup,
			optixMissProgGroup,
			optixHitProgGroup
		};

		OptixPipelineLinkOptions pipelineLinkOptions = {};
		pipelineLinkOptions.maxTraceDepth = 1;
		pipelineLinkOptions.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
		pipelineLinkOptions.overrideUsesMotionBlur = usesMotionBlur;

		optixErrLogSize = sizeof(optixErrLog);
		CHECK_OPTIX_ERROR(optixPipelineCreate(
				optixContext,
				&pipelineCompileOptions,
				&pipelineLinkOptions,
				programGroups,
				sizeof(programGroups) / sizeof(programGroups[0]),
				optixErrLog,
				&optixErrLogSize,
				&optixPipeline));

		//----------------------------------------------------------------------
		// Allocate Optix kernel parameters and Shader binding table
		//----------------------------------------------------------------------

		optixAccelParams.optixHandle = topLevelHandle;
		optixAccelParams.rayBuff = 0;
		optixAccelParams.rayHitBuff = 0;
		cudaDevice->AllocBufferRO(&optixAccelParamsBuff, &optixAccelParams, sizeof(OptixAccelParams));

		// Raygen SBT
		char sbtRayGenBuff[OPTIX_SBT_RECORD_HEADER_SIZE];
		CHECK_OPTIX_ERROR(optixSbtRecordPackHeader(optixRaygenProgGroup, sbtRayGenBuff));
		cudaDevice->AllocBufferRW(&optixRayGenSbtBuff, sbtRayGenBuff, OPTIX_SBT_RECORD_HEADER_SIZE);

		// Hit SBT
		HitGroupSbtRecord *hitGroupSbtRecords = new HitGroupSbtRecord[optixAccel.meshes.size()];
		for (u_int i = 0; i < optixAccel.meshes.size(); ++i) {
			hitGroupSbtRecords[i].data.meshIndex = i;
			CHECK_OPTIX_ERROR(optixSbtRecordPackHeader(optixHitProgGroup, &hitGroupSbtRecords[i]));
		}
		cudaDevice->AllocBufferRW(&optixHitSbtBuff, hitGroupSbtRecords, sizeof(HitGroupSbtRecord) * optixAccel.meshes.size());
		delete[] hitGroupSbtRecords;

		// Miss SBT
		char sbtHitBuff[OPTIX_SBT_RECORD_HEADER_SIZE];
		CHECK_OPTIX_ERROR(optixSbtRecordPackHeader(optixHitProgGroup, sbtHitBuff));
		cudaDevice->AllocBufferRW(&optixMissSbtBuff, sbtHitBuff, OPTIX_SBT_RECORD_HEADER_SIZE);

		memset(&optixSbt, 0, sizeof(OptixShaderBindingTable));
		optixSbt.raygenRecord = ((CUDADeviceBuffer *)optixRayGenSbtBuff)->GetCUDADevicePointer();
		optixSbt.missRecordBase = ((CUDADeviceBuffer *)optixMissSbtBuff)->GetCUDADevicePointer();
		optixSbt.missRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
		optixSbt.missRecordCount = 1;
		optixSbt.hitgroupRecordBase = ((CUDADeviceBuffer *)optixHitSbtBuff)->GetCUDADevicePointer();
		optixSbt.hitgroupRecordStrideInBytes = sizeof(HitGroupSbtRecord);
		optixSbt.hitgroupRecordCount = optixAccel.meshes.size();
	}

	virtual ~OptixKernel() {
		CUDAIntersectionDevice *cudaDevice = dynamic_cast<CUDAIntersectionDevice *>(&device);

		if (optixPipeline) {
			CHECK_OPTIX_ERROR(optixPipelineDestroy(optixPipeline));
		}
		if (optixRaygenProgGroup) {
			CHECK_OPTIX_ERROR(optixProgramGroupDestroy(optixRaygenProgGroup));
		}
		if (optixRaygenProgGroup) {
			CHECK_OPTIX_ERROR(optixProgramGroupDestroy(optixMissProgGroup));
		}
		if (optixRaygenProgGroup) {
			CHECK_OPTIX_ERROR(optixProgramGroupDestroy(optixHitProgGroup));
		}
		if (optixModule) {
			CHECK_OPTIX_ERROR(optixModuleDestroy(optixModule));
		}

		for (u_int i = 0; i < optixOutputBuffers.size(); ++i)
			cudaDevice->FreeBuffer(&optixOutputBuffers[i]);

		cudaDevice->FreeBuffer(&optixAccelParamsBuff);

		cudaDevice->FreeBuffer(&optixRayGenSbtBuff);
		cudaDevice->FreeBuffer(&optixMissSbtBuff);
		cudaDevice->FreeBuffer(&optixHitSbtBuff);
	}

	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount);

private:
	void BuildTraversable(const TriangleMesh *mesh,
			OptixTraversableHandle &handle,
			HardwareDeviceBuffer **outputBuffer) {
		CUDAIntersectionDevice *cudaDevice = dynamic_cast<CUDAIntersectionDevice *>(&device);

		// Allocate CUDA vertices buffer
		HardwareDeviceBuffer *vertsBuff = nullptr;
		cudaDevice->AllocBufferRO(&vertsBuff, mesh->GetVertices(), sizeof(Point) * mesh->GetTotalVertexCount());

		// Allocate CUDA triangle vertices indices buffer
		HardwareDeviceBuffer *trisBuff = nullptr;
		cudaDevice->AllocBufferRO(&trisBuff, mesh->GetTriangles(), sizeof(Triangle) * mesh->GetTotalTriangleCount());

		const u_int triangleInputFlags[1] = { OPTIX_GEOMETRY_FLAG_NONE };

		// Initialize OptixBuildInput for each mesh
		OptixBuildInput buildInput = {};
		buildInput.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
		buildInput.triangleArray.vertexBuffers = &(((CUDADeviceBuffer *)vertsBuff)->GetCUDADevicePointer());
		buildInput.triangleArray.numVertices = mesh->GetTotalVertexCount();
		buildInput.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
		buildInput.triangleArray.vertexStrideInBytes = sizeof(Point);
		buildInput.triangleArray.indexBuffer = ((CUDADeviceBuffer *)trisBuff)->GetCUDADevicePointer();
		buildInput.triangleArray.numIndexTriplets = mesh->GetTotalTriangleCount();
		buildInput.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
		buildInput.triangleArray.indexStrideInBytes = sizeof(Triangle);
		buildInput.triangleArray.preTransform = 0;
		buildInput.triangleArray.flags = triangleInputFlags;
		buildInput.triangleArray.numSbtRecords = 1;
		buildInput.triangleArray.sbtIndexOffsetBuffer = 0;
		buildInput.triangleArray.sbtIndexOffsetSizeInBytes = 0;
		buildInput.triangleArray.sbtIndexOffsetStrideInBytes = 0;
		buildInput.triangleArray.primitiveIndexOffset = 0;
		
		BuildTraversable(buildInput, handle, outputBuffer);
		
		// Free temporary memory

		cudaDevice->FreeBuffer(&vertsBuff);
		cudaDevice->FreeBuffer(&trisBuff);
	}

	void BuildTraversable(const OptixBuildInput &buildInput,
			OptixTraversableHandle &handle,
			HardwareDeviceBuffer **outputBuffer) {
		CUDAIntersectionDevice *cudaDevice = dynamic_cast<CUDAIntersectionDevice *>(&device);
		OptixDeviceContext optixContext = cudaDevice->GetOptixContext();

		// Allocate temporary build buffers

		OptixAccelBuildOptions accelOptions = {};
		accelOptions.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
		accelOptions.operation = OPTIX_BUILD_OPERATION_BUILD;
		accelOptions.motionOptions.numKeys = 0;

		OptixAccelBufferSizes gasBufferSizes;
		CHECK_OPTIX_ERROR(optixAccelComputeMemoryUsage(optixContext,
				&accelOptions, &buildInput, 1,
				&gasBufferSizes));

		HardwareDeviceBuffer *tmpBufferGas = nullptr;
		cudaDevice->AllocBufferRW(&tmpBufferGas, nullptr, gasBufferSizes.tempSizeInBytes);

		HardwareDeviceBuffer *bufferTempOutputGasAndCompactedSize = nullptr;
		const size_t compactedSizeOffset = RoundUp<size_t>(gasBufferSizes.outputSizeInBytes, 8ull);
		cudaDevice->AllocBufferRW(&bufferTempOutputGasAndCompactedSize, nullptr, compactedSizeOffset + 8);

		// Build the accelerator structure (GAS)

		OptixAccelEmitDesc emitProperty;
		emitProperty.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
		emitProperty.result = (CUdeviceptr)((char*)((CUDADeviceBuffer *)bufferTempOutputGasAndCompactedSize)->GetCUDADevicePointer() + compactedSizeOffset);

		CHECK_OPTIX_ERROR(optixAccelBuild(
				optixContext,
				0, // CUDA stream
				&accelOptions,
				&buildInput,
				1,
				((CUDADeviceBuffer *)tmpBufferGas)->GetCUDADevicePointer(),
				gasBufferSizes.tempSizeInBytes,
				((CUDADeviceBuffer *)bufferTempOutputGasAndCompactedSize)->GetCUDADevicePointer(),
				gasBufferSizes.outputSizeInBytes,
				&handle,
				&emitProperty,
				1));

		// Free temporary memory

		cudaDevice->FreeBuffer(&tmpBufferGas);

		// Compact the accelerator structure 

		size_t compactedGasSize;
		CHECK_CUDA_ERROR(cuMemcpyDtoH(&compactedGasSize, emitProperty.result, sizeof(size_t)));

		if (compactedGasSize < gasBufferSizes.outputSizeInBytes) {
			cudaDevice->AllocBufferRW(outputBuffer, nullptr, compactedGasSize);

			// Use handle as input and output
			OptixTraversableHandle newHandle;
			CHECK_OPTIX_ERROR(optixAccelCompact(optixContext,
					0,
					handle,
					((CUDADeviceBuffer *)(*outputBuffer))->GetCUDADevicePointer(),
					compactedGasSize,
					&newHandle));
			handle = newHandle;

			cudaDevice->FreeBuffer(&bufferTempOutputGasAndCompactedSize);
		} else
			*outputBuffer = bufferTempOutputGasAndCompactedSize;
	}

	vector<HardwareDeviceBuffer *> optixOutputBuffers;

	OptixModule optixModule;
	OptixProgramGroup optixRaygenProgGroup, optixMissProgGroup, optixHitProgGroup;
	OptixPipeline optixPipeline;

	OptixAccelParams optixAccelParams;
	HardwareDeviceBuffer *optixAccelParamsBuff;

	OptixShaderBindingTable optixSbt;
	HardwareDeviceBuffer *optixRayGenSbtBuff;
	HardwareDeviceBuffer *optixMissSbtBuff;
	HardwareDeviceBuffer *optixHitSbtBuff;
};

void OptixKernel::EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount) {
	CUDAIntersectionDevice *cudaDevice = dynamic_cast<CUDAIntersectionDevice *>(&device);
	// TODO: optixAccelParams async. copy fix
	optixAccelParams.rayBuff = ((CUDADeviceBuffer *)rayBuff)->GetCUDADevicePointer();
	optixAccelParams.rayHitBuff = ((CUDADeviceBuffer *)rayHitBuff)->GetCUDADevicePointer();
	cudaDevice->EnqueueWriteBuffer(optixAccelParamsBuff, false, sizeof(OptixAccelParams), &optixAccelParams);

	CHECK_OPTIX_ERROR(optixLaunch(
			optixPipeline,
			0,
			((CUDADeviceBuffer *)optixAccelParamsBuff)->GetCUDADevicePointer(),
			sizeof(OptixAccelParams),
			&optixSbt,
			rayCount, 1, 1));
}

bool OptixAccel::HasDataParallelSupport(const IntersectionDevice &device) const {
	return device.HasDataParallelSupport();
}

HardwareIntersectionKernel *OptixAccel::NewHardwareIntersectionKernel(HardwareIntersectionDevice &device) const {
	// Setup the kernel
	return new OptixKernel(device, *this);
}

}
