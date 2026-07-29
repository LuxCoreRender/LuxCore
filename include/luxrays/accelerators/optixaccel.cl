#line 2 "optixaccel.cl"

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

//------------------------------------------------------------------------------

#include <optix_device.h>

//------------------------------------------------------------------------------
// This must match the definition in optixaccel.cpp

typedef struct Params {
	OptixTraversableHandle optixHandle;
	CUdeviceptr rayBuff;
	CUdeviceptr rayHitBuff;
} OptixAccelParams;

//------------------------------------------------------------------------------

extern "C" {
__constant__ OptixAccelParams optixAccelParams;
}

extern "C" __global__ void __raygen__OptixAccel() {
	const uint3 launchIndex = optixGetLaunchIndex();

	Ray *rayBuff = (Ray *)optixAccelParams.rayBuff;
	Ray *ray = &rayBuff[launchIndex.x];

	if (ray->flags & RAY_FLAGS_MASKED)
		return;

	optixTrace(
            optixAccelParams.optixHandle,
            make_float3(ray->o.x, ray->o.y, ray->o.z),
            make_float3(ray->d.x, ray->d.y, ray->d.z),
            ray->mint,
            ray->maxt,
            ray->time,
            OptixVisibilityMask(1),
            0,
            0, 1, 0);
}

extern "C" __global__ void __closesthit__OptixAccel() {
	const uint3 launchIndex = optixGetLaunchIndex();

	RayHit *rayHitBuff = (RayHit *)optixAccelParams.rayHitBuff;
	RayHit *rayHit = &rayHitBuff[launchIndex.x];

	const uint triangleIndex = optixGetPrimitiveIndex();

	if (triangleIndex == NULL_INDEX) {
		rayHit->meshIndex = NULL_INDEX;
		rayHit->triangleIndex = NULL_INDEX;
	} else {
		rayHit->t = optixGetRayTmax();

		const float2 barycentrics = optixGetTriangleBarycentrics();
		rayHit->b1 = barycentrics.x;
		rayHit->b2 = barycentrics.y;

		rayHit->meshIndex = optixGetInstanceId();
		rayHit->triangleIndex = triangleIndex;
	}
}

extern "C" __global__ void __miss__OptixAccel() {
	const uint3 launchIndex = optixGetLaunchIndex();

	RayHit *rayHitBuff = (RayHit *)optixAccelParams.rayHitBuff;
	RayHit *rayHit = &rayHitBuff[launchIndex.x];
	
	rayHit->meshIndex = NULL_INDEX;
	rayHit->triangleIndex = NULL_INDEX;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
