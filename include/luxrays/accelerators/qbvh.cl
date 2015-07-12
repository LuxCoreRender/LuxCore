#line 2 "qbvh_kernel.cl"

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

#if defined(QBVH_USE_LOCAL_MEMORY)
#define QBVH_LOCAL_MEMORY_PARAM_DECL , __local int *nodeStacks
#define QBVH_LOCAL_MEMORY_PARAM , nodeStacks
#else
#define QBVH_LOCAL_MEMORY_PARAM_DECL
#define QBVH_LOCAL_MEMORY_PARAM
#endif

#ifdef USE_IMAGE_STORAGE
#define ACCELERATOR_INTERSECT_PARAM_DECL , __read_only image2d_t nodes, __read_only image2d_t quadTris QBVH_LOCAL_MEMORY_PARAM_DECL
#define ACCELERATOR_INTERSECT_PARAM , nodes, quadTris QBVH_LOCAL_MEMORY_PARAM
#else
#define ACCELERATOR_INTERSECT_PARAM_DECL ,__global const QBVHNode* restrict nodes, __global const QuadTiangle* restrict quadTris QBVH_LOCAL_MEMORY_PARAM_DECL
#define ACCELERATOR_INTERSECT_PARAM , nodes, quadTris QBVH_LOCAL_MEMORY_PARAM
#endif

void Accelerator_Intersect(
		const Ray *ray,
		RayHit *rayHit
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	// Prepare the ray for intersection
	QuadRay ray4;
	ray4.ox = (float4)ray->o.x;
	ray4.oy = (float4)ray->o.y;
	ray4.oz = (float4)ray->o.z;

	ray4.dx = (float4)ray->d.x;
	ray4.dy = (float4)ray->d.y;
	ray4.dz = (float4)ray->d.z;

	ray4.mint = (float4)ray->mint;
	ray4.maxt = (float4)ray->maxt;

	const float4 invDir0 = (float4)(1.f / ray4.dx.s0);
	const float4 invDir1 = (float4)(1.f / ray4.dy.s0);
	const float4 invDir2 = (float4)(1.f / ray4.dz.s0);

	const int signs0 = signbit(ray4.dx.s0);
	const int signs1 = signbit(ray4.dy.s0);
	const int signs2 = signbit(ray4.dz.s0);

	const int isigns0 = 1 - signs0;
	const int isigns1 = 1 - signs1;
	const int isigns2 = 1 - signs2;

	rayHit->meshIndex = NULL_INDEX;
	rayHit->triangleIndex = NULL_INDEX;

	//------------------------------
	// Main loop
	int todoNode = 0; // the index in the stack
	// nodeStack leads to a lot of local memory banks conflicts however it has not real
	// impact on performances (I guess access latency is hidden by other stuff).
	// Avoiding conflicts is easy to do but it requires to know the work group
	// size (not worth doing if there are not performance benefits).
#if defined(QBVH_USE_LOCAL_MEMORY)
	__local int *nodeStack = &nodeStacks[QBVH_STACK_SIZE * get_local_id(0)];
#else
	int nodeStack[QBVH_STACK_SIZE];
#endif
	nodeStack[0] = 0; // first node to handle: root node

#ifdef USE_IMAGE_STORAGE
    const int quadTrisImageWidth = get_image_width(quadTris);

    const int bboxes_minXIndex = (signs0 * 3);
    const int bboxes_maxXIndex = (isigns0 * 3);
    const int bboxes_minYIndex = (signs1 * 3) + 1;
    const int bboxes_maxYIndex = (isigns1 * 3) + 1;
    const int bboxes_minZIndex = (signs2 * 3) + 2;
    const int bboxes_maxZIndex = (isigns2 * 3) + 2;

    const sampler_t imageSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
#endif

	//int maxDepth = 0;
	while (todoNode >= 0) {
		const int nodeData = nodeStack[todoNode];
		--todoNode;

		// Leaves are identified by a negative index
		if (!QBVHNode_IsLeaf(nodeData)) {
#ifdef USE_IMAGE_STORAGE
            // Read the node information from the image storage

			// 7 pixels required for the storage of a QBVH node
            const ushort inx = (nodeData >> 16) * 7;
            const ushort iny = (nodeData & 0xffff);
            const float4 bboxes_minX = as_float4(read_imageui(nodes, imageSampler, (int2)(inx + bboxes_minXIndex, iny)));
            const float4 bboxes_maxX = as_float4(read_imageui(nodes, imageSampler, (int2)(inx + bboxes_maxXIndex, iny)));
            const float4 bboxes_minY = as_float4(read_imageui(nodes, imageSampler, (int2)(inx + bboxes_minYIndex, iny)));
            const float4 bboxes_maxY = as_float4(read_imageui(nodes, imageSampler, (int2)(inx + bboxes_maxYIndex, iny)));
            const float4 bboxes_minZ = as_float4(read_imageui(nodes, imageSampler, (int2)(inx + bboxes_minZIndex, iny)));
            const float4 bboxes_maxZ = as_float4(read_imageui(nodes, imageSampler, (int2)(inx + bboxes_maxZIndex, iny)));
            const int4 children = as_int4(read_imageui(nodes, imageSampler, (int2)(inx + 6, iny)));

			const int4 visit = QBVHNode_BBoxIntersect(
                bboxes_minX, bboxes_maxX,
                bboxes_minY, bboxes_maxY,
                bboxes_minZ, bboxes_maxZ,
                &ray4,
				invDir0, invDir1, invDir2);
#else
			__global const QBVHNode* restrict node = &nodes[nodeData];
            const int4 visit = QBVHNode_BBoxIntersect(
                node->bboxes[signs0][0], node->bboxes[isigns0][0],
                node->bboxes[signs1][1], node->bboxes[isigns1][1],
                node->bboxes[signs2][2], node->bboxes[isigns2][2],
                &ray4,
				invDir0, invDir1, invDir2);

			const int4 children = node->children;
#endif

			// For some reason doing logic operations with int4 is very slow
			nodeStack[todoNode + 1] = children.s3;
			todoNode += (visit.s3 && !QBVHNode_IsEmpty(children.s3)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s2;
			todoNode += (visit.s2 && !QBVHNode_IsEmpty(children.s2)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s1;
			todoNode += (visit.s1 && !QBVHNode_IsEmpty(children.s1)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s0;
			todoNode += (visit.s0 && !QBVHNode_IsEmpty(children.s0)) ? 1 : 0;

			//maxDepth = max(maxDepth, todoNode);
		} else {
			// Perform intersection
			const uint nbQuadPrimitives = QBVHNode_NbQuadPrimitives(nodeData);
			const uint offset = QBVHNode_FirstQuadIndex(nodeData);

#ifdef USE_IMAGE_STORAGE
			// 11 pixels required for the storage of QBVH Triangles
            ushort inx = (offset >> 16) * 11;
            ushort iny = (offset & 0xffff);
#endif

			for (uint primNumber = offset; primNumber < (offset + nbQuadPrimitives); ++primNumber) {
#ifdef USE_IMAGE_STORAGE
                const float4 origx = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 origy = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 origz = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 edge1x = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 edge1y = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 edge1z = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 edge2x = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 edge2y = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const float4 edge2z = as_float4(read_imageui(quadTris, imageSampler, (int2)(inx++, iny)));
                const uint4 meshIndex = read_imageui(quadTris, imageSampler, (int2)(inx++, iny));
				const uint4 triangleIndex = read_imageui(quadTris, imageSampler, (int2)(inx++, iny));

                if (inx >= quadTrisImageWidth) {
                    inx = 0;
                    iny++;
                }
#else
                __global const QuadTiangle* restrict quadTri = &quadTris[primNumber];
                const float4 origx = quadTri->origx;
                const float4 origy = quadTri->origy;
                const float4 origz = quadTri->origz;
                const float4 edge1x = quadTri->edge1x;
                const float4 edge1y = quadTri->edge1y;
                const float4 edge1z = quadTri->edge1z;
                const float4 edge2x = quadTri->edge2x;
                const float4 edge2y = quadTri->edge2y;
                const float4 edge2z = quadTri->edge2z;
                const uint4 meshIndex = quadTri->meshIndex;
				const uint4 triangleIndex = quadTri->triangleIndex;
#endif
				QuadTriangle_Intersect(
                    origx, origy, origz,
                    edge1x, edge1y, edge1z,
                    edge2x, edge2y, edge2z,
                    meshIndex, triangleIndex,
                    &ray4, rayHit);
            }
		}
	}

	//printf("MaxDepth=%02d\n", maxDepth);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Accelerator_Intersect_RayBuffer(
		__global const Ray* restrict rays,
		__global RayHit *rayHits,
		const uint rayCount
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	Ray ray;
	Ray_ReadAligned4_Private(&rays[gid], &ray);

	RayHit rayHit;
	Accelerator_Intersect(
		&ray,
		&rayHit
		ACCELERATOR_INTERSECT_PARAM
		);

	// Write result
	__global RayHit *memRayHit = &rayHits[gid];
	memRayHit->t = rayHit.t;
	memRayHit->b1 = rayHit.b1;
	memRayHit->b2 = rayHit.b2;
	memRayHit->meshIndex = rayHit.meshIndex;
	memRayHit->triangleIndex = rayHit.triangleIndex;
}
