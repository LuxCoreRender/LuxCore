#line 1 "patchocl_kernels.cl"

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

//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

__kernel void Init(
		uint seedBase,
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global Camera *camera
		) {
//	const size_t gid = get_global_id(0);
//
//	//if (gid == 0)
//	//	printf("GPUTask: %d\n", sizeof(GPUTask));
//
//	// Initialize the task
//	__global GPUTask *task = &tasks[gid];
//
//	// Initialize random number generator
//	Seed seed;
//	InitRandomGenerator(seedBase + gid, &seed);
//
//	// Initialize the sample
//	Sampler_Init(gid,
//#if (PARAM_SAMPLER_TYPE == 3)
//			localMemTempBuff,
//#endif
//			&seed, &task->sample);
//
//	// Initialize the path
//	GenerateCameraPath(task, &rays[gid], &seed, camera);
//
//	// Save the seed
//	task->seed.s1 = seed.s1;
//	task->seed.s2 = seed.s2;
//	task->seed.s3 = seed.s3;
//
//	__global GPUTaskStats *taskStat = &taskStats[gid];
//	taskStat->sampleCount = 0;
}

//------------------------------------------------------------------------------
// InitFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel void InitFrameBuffer(
		__global Pixel *frameBuffer
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid >= (PARAM_IMAGE_WIDTH + 2) * (PARAM_IMAGE_HEIGHT + 2))
		return;

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0.f;

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	__global AlphaPixel *ap = &alphaFrameBuffer[gid];
	ap->alpha = 0.f;
#endif
}

//------------------------------------------------------------------------------
// AdvancePaths Kernel
//------------------------------------------------------------------------------

//#if defined(PARAM_HAS_TEXTUREMAPS)
//
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_0)
//__global Spectrum *GetRGBAddress(const uint page, const uint offset
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_0)
//                , __global Spectrum *texMapRGBBuff0
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_1)
//                , __global Spectrum *texMapRGBBuff1
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_2)
//                , __global Spectrum *texMapRGBBuff2
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_3)
//                , __global Spectrum *texMapRGBBuff3
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_4)
//                , __global Spectrum *texMapRGBBuff4
//#endif
//    ) {
//    switch (page) {
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_1)
//        case 1:
//            return &texMapRGBBuff1[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_2)
//        case 2:
//            return &texMapRGBBuff2[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_3)
//        case 3:
//            return &texMapRGBBuff3[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_RGB_PAGE_4)
//        case 4:
//            return &texMapRGBBuff4[offset];
//#endif
//        default:
//        case 0:
//            return &texMapRGBBuff0[offset];
//    }
//}
//#endif
//
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_0)
//__global float *GetAlphaAddress(const uint page, const uint offset
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_0)
//                , __global float *texMapAlphaBuff0
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_1)
//                , __global float *texMapAlphaBuff1
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_2)
//                , __global float *texMapAlphaBuff2
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_3)
//                , __global float *texMapAlphaBuff3
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_4)
//                , __global float *texMapAlphaBuff4
//#endif
//    ) {
//    switch (page) {
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_1)
//        case 1:
//            return &texMapAlphaBuff1[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_2)
//        case 2:
//            return &texMapAlphaBuff2[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_3)
//        case 3:
//            return &texMapAlphaBuff3[offset];
//#endif
//#if defined(PARAM_TEXTUREMAPS_ALPHA_PAGE_4)
//        case 4:
//            return &texMapAlphaBuff4[offset];
//#endif
//        default:
//        case 0:
//            return &texMapAlphaBuff0[offset];
//    }
//}
//#endif
//
//#endif

__kernel void AdvancePaths(
		__global GPUTask *tasks,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
		__global Material *mats,
		__global uint *meshMats,
		__global uint *meshIDs,
#if defined(PARAM_ACCEL_MQBVH)
		__global uint *meshFirstTriangleOffset,
		__global Mesh *meshDescs,
#endif
		__global Vector *vertNormals,
		__global Point *vertices,
		__global Triangle *triangles,
		__global Camera *camera
		) {
}
