#line 2 "ray_funcs.cl"

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
void Ray_Init4_Private(Ray *ray, const float3 orig, const float3 dir,
		const float mint, const float maxt, const float time) {
	ray->o.x = orig.x;
	ray->o.y = orig.y;
	ray->o.z = orig.z;

	ray->d.x = dir.x;
	ray->d.y = dir.y;
	ray->d.z = dir.z;

	ray->mint = mint;
	ray->maxt = maxt;

	ray->time = time;
}

void Ray_Init3_Private(Ray *ray, const float3 orig, const float3 dir,
		const float maxt, const float time) {
	ray->o.x = orig.x;
	ray->o.y = orig.y;
	ray->o.z = orig.z;

	ray->d.x = dir.x;
	ray->d.y = dir.y;
	ray->d.z = dir.z;

	ray->mint = MachineEpsilon_E_Float3(orig);
	ray->maxt = maxt;

	ray->time = time;
}

void Ray_Init2_Private(Ray *ray, const float3 orig, const float3 dir, const float time) {
	ray->o.x = orig.x;
	ray->o.y = orig.y;
	ray->o.z = orig.z;

	ray->d.x = dir.x;
	ray->d.y = dir.y;
	ray->d.z = dir.z;

	ray->mint = MachineEpsilon_E_Float3(orig);
	ray->maxt = INFINITY;

	ray->time = time;
}

void Ray_Init4(__global Ray *ray, const float3 orig, const float3 dir,
		const float mint, const float maxt, const float time) {
	VSTORE3F(orig, &ray->o.x);
	VSTORE3F(dir, &ray->d.x);

	ray->mint = mint;
	ray->maxt = maxt;

	ray->time = time;
}

void Ray_Init3(__global Ray *ray, const float3 orig, const float3 dir,
		const float maxt, const float time) {
	VSTORE3F(orig, &ray->o.x);
	VSTORE3F(dir, &ray->d.x);

	ray->mint = MachineEpsilon_E_Float3(orig);
	ray->maxt = maxt;

	ray->time = time;
}

void Ray_Init2(__global Ray *ray, const float3 orig, const float3 dir, const float time) {
	VSTORE3F(orig, &ray->o.x);
	VSTORE3F(dir, &ray->d.x);

	ray->mint = MachineEpsilon_E_Float3(orig);
	ray->maxt = INFINITY;

	ray->time = time;
}

void Ray_ReadAligned4(__global Ray *ray, float3 *rayOrig, float3 *rayDir,
		float *mint, float *maxt, float *time) {
	__global float4 *basePtr =(__global float4 *)ray;
	const float4 data0 = (*basePtr++);
	const float4 data1 = (*basePtr);

	*rayOrig = (float3)(data0.x, data0.y, data0.z);
	*rayDir = (float3)(data0.w, data1.x, data1.y);

	*mint = data1.z;
	*maxt = data1.w;

	*time = ray->time;
}

void Ray_ReadAligned4_Private(__global Ray *ray, Ray *dstRay) {
	__global float4 *basePtr =(__global float4 *)ray;
	const float4 data0 = (*basePtr++);
	const float4 data1 = (*basePtr);

	dstRay->o.x = data0.x;
	dstRay->o.y = data0.y;
	dstRay->o.z = data0.z;
	dstRay->d.x = data0.w;
	dstRay->d.y = data1.x;
	dstRay->d.z = data1.y;

	dstRay->mint = data1.z;
	dstRay->maxt = data1.w;

	dstRay->time = ray->time;
}
