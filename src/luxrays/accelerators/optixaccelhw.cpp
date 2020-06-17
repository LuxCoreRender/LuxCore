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
#include "luxrays/utils/utils.h"
#include "luxrays/kernels/kernels.h"

using namespace std;

namespace luxrays {

class OptixKernel : public HardwareIntersectionKernel {
public:
	OptixKernel(HardwareIntersectionDevice &dev, const OptixAccel &optixAccel) :
		HardwareIntersectionKernel(dev), gasOutputBuffer(nullptr) {
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
	
			vector<HardwareDeviceBuffer *> vertsBuffs(optixAccel.meshes.size());
			vector<HardwareDeviceBuffer *> trisBuffs(optixAccel.meshes.size());
			vector<OptixBuildInput> buildInputs(optixAccel.meshes.size());
			const u_int triangleInputFlags[1] = { OPTIX_GEOMETRY_FLAG_NONE };
			for (u_int i = 0; i < optixAccel.meshes.size(); ++i) {
				OptixBuildInput &buildInput = buildInputs[i];
				const Mesh *mesh = optixAccel.meshes[i];

				// Allocate CUDA vertices buffer
				cudaDevice->AllocBufferRW(&vertsBuffs[i], mesh->GetVertices(), sizeof(Point) * mesh->GetTotalVertexCount());
				// Allocate CUDA triangle vertices indices buffer
				cudaDevice->AllocBufferRW(&trisBuffs[i], mesh->GetTriangles(), sizeof(Triangle) * mesh->GetTotalTriangleCount());
				
				// Initialize OptixBuildInput for each mesh
				buildInput.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
				buildInput.triangleArray.vertexBuffers = &(((CUDADeviceBuffer *)vertsBuffs[i])->GetCudaDevicePointer());
				buildInput.triangleArray.numVertices = mesh->GetTotalVertexCount();
				buildInput.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
				buildInput.triangleArray.vertexStrideInBytes = sizeof(Point);
				buildInput.triangleArray.indexBuffer = ((CUDADeviceBuffer *)trisBuffs[i])->GetCudaDevicePointer();
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
			}

			// Allocate temporary build buffers

			OptixAccelBuildOptions accelOptions;
			accelOptions.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
			accelOptions.operation = OPTIX_BUILD_OPERATION_BUILD;
			accelOptions.motionOptions.numKeys = 0;
			
			OptixAccelBufferSizes gasBufferSizes;
            CHECK_OPTIX_ERROR(optixAccelComputeMemoryUsage(optixContext,
					&accelOptions, &buildInputs[0], optixAccel.meshes.size(),
					&gasBufferSizes));

			HardwareDeviceBuffer *tmpBufferGas = nullptr;
			cudaDevice->AllocBufferRW(&tmpBufferGas, nullptr, gasBufferSizes.tempSizeInBytes);

			HardwareDeviceBuffer *bufferTempOutputGasAndCompactedSize = nullptr;
            const size_t compactedSizeOffset = RoundUp<size_t>(gasBufferSizes.outputSizeInBytes, 8ull);
			cudaDevice->AllocBufferRW(&bufferTempOutputGasAndCompactedSize, nullptr, compactedSizeOffset + 8);
			
			// Build the accelerator structure (GAS)

			OptixAccelEmitDesc emitProperty;
            emitProperty.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
            emitProperty.result = (CUdeviceptr)((char*)((CUDADeviceBuffer *)bufferTempOutputGasAndCompactedSize)->GetCudaDevicePointer() + compactedSizeOffset);
			
			OptixTraversableHandle gasHandle;
			CHECK_OPTIX_ERROR(optixAccelBuild(
					optixContext,
					0, // CUDA stream
					&accelOptions,
					&buildInputs[0],
					buildInputs.size(),
					((CUDADeviceBuffer *)tmpBufferGas)->GetCudaDevicePointer(),
					gasBufferSizes.tempSizeInBytes,
					((CUDADeviceBuffer *)bufferTempOutputGasAndCompactedSize)->GetCudaDevicePointer(),
					gasBufferSizes.outputSizeInBytes,
					&gasHandle,
					&emitProperty,
					1));

			// Free temporary memory

			cudaDevice->FreeBuffer(&tmpBufferGas);
			for (u_int i = 0; i < optixAccel.meshes.size(); ++i) {
				cudaDevice->FreeBuffer(&vertsBuffs[i]);
				cudaDevice->FreeBuffer(&trisBuffs[i]);
			}
			
			size_t compactedGasSize;
			CHECK_CUDA_ERROR(cuMemcpyDtoH(&compactedGasSize, emitProperty.result, sizeof(size_t)));

            if (compactedGasSize < gasBufferSizes.outputSizeInBytes) {
				cudaDevice->AllocBufferRW(&gasOutputBuffer, nullptr, compactedGasSize);

                // Use handle as input and output
                CHECK_OPTIX_ERROR(optixAccelCompact(optixContext,
						0,
						gasHandle,
						((CUDADeviceBuffer *)gasOutputBuffer)->GetCudaDevicePointer(),
						compactedGasSize,
						&gasHandle));

				cudaDevice->FreeBuffer(&bufferTempOutputGasAndCompactedSize);
            } else
                gasOutputBuffer = bufferTempOutputGasAndCompactedSize;
			
			LR_LOG(device.GetContext(), "Optix total build time: " << int((WallClockTime() - t0) * 1000) << "ms");
			LR_LOG(device.GetContext(), "Total Optix memory usage: " << (gasOutputBuffer->GetSize() / 1024) << "Kbytes");
	}

	virtual ~OptixKernel() {
		CUDAIntersectionDevice *cudaDevice = dynamic_cast<CUDAIntersectionDevice *>(&device);
		cudaDevice->FreeBuffer(&gasOutputBuffer);
	}

	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount);

private:
	HardwareDeviceBuffer *gasOutputBuffer;
};

void OptixKernel::EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount) {
}

bool OptixAccel::HasDataParallelSupport(const IntersectionDevice &device) const {
	return device.HasDataParallelSupport();
}

HardwareIntersectionKernel *OptixAccel::NewHardwareIntersectionKernel(HardwareIntersectionDevice &device) const {
	// Setup the kernel
	return new OptixKernel(device, *this);
}

}
