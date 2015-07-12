#line 2 "mqbvh_kernel.cl"

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

// Using a large stack size to avoid the allocation of the array on
// GPU registers (otherwise the GPU can easily run out of registers)
#define STACK_SIZE 64

void LeafIntersect(
		const Ray *ray,
		RayHit *rayHit,
		__global const QBVHNode* restrict nodes,
		__global const QuadTiangle* restrict quadTris) {
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

	rayHit->meshIndex = NULL_INDEX;
	rayHit->triangleIndex = NULL_INDEX;

	//------------------------------
	// Main loop
	int todoNode = 0; // the index in the stack
	int nodeStack[STACK_SIZE];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		const int nodeData = nodeStack[todoNode];
		--todoNode;

		// Leaves are identified by a negative index
		if (!QBVHNode_IsLeaf(nodeData)) {
			__global const QBVHNode* restrict node = &nodes[nodeData];
            const int4 visit = QBVHNode_BBoxIntersect(
                node->bboxes[signs0][0], node->bboxes[1 - signs0][0],
                node->bboxes[signs1][1], node->bboxes[1 - signs1][1],
                node->bboxes[signs2][2], node->bboxes[1 - signs2][2],
                &ray4,
				invDir0, invDir1, invDir2);

			const int4 children = node->children;

			// For some reason doing logic operations with int4 is very slow
			nodeStack[todoNode + 1] = children.s3;
			todoNode += (visit.s3 && !QBVHNode_IsEmpty(children.s3)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s2;
			todoNode += (visit.s2 && !QBVHNode_IsEmpty(children.s2)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s1;
			todoNode += (visit.s1 && !QBVHNode_IsEmpty(children.s1)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s0;
			todoNode += (visit.s0 && !QBVHNode_IsEmpty(children.s0)) ? 1 : 0;
		} else {
			// Perform intersection
			const uint nbQuadPrimitives = QBVHNode_NbQuadPrimitives(nodeData);
			const uint offset = QBVHNode_FirstQuadIndex(nodeData);

			for (uint primNumber = offset; primNumber < (offset + nbQuadPrimitives); ++primNumber) {
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

				QuadTriangle_Intersect(
                    origx, origy, origz,
                    edge1x, edge1y, edge1z,
                    edge2x, edge2y, edge2z,
                    meshIndex, triangleIndex,
                    &ray4, rayHit);
            }
		}
	}
}

#define MQBVH_TRANSFORMATIONS_PARAM_DECL , __global const uint* restrict leafTransformationIndex , __global const Matrix4x4* restrict leafTransformations
#define MQBVH_TRANSFORMATIONS_PARAM , leafTransformationIndex, leafTransformations

#define MQBVH_MOTIONSYSTEMS_PARAM_DECL , __global const MotionSystem* restrict leafMotionSystems , __global const InterpolatedTransform* restrict leafInterpolatedTransforms
#define MQBVH_MOTIONSYSTEMS_PARAM , leafMotionSystems, leafInterpolatedTransforms

#define ACCELERATOR_INTERSECT_PARAM_DECL ,__global const QBVHNode* restrict nodes, __global const uint* restrict qbvhMemMap, __global const QBVHNode *leafNodes, __global const QuadTiangle* const leafQuadTris MQBVH_TRANSFORMATIONS_PARAM_DECL MQBVH_MOTIONSYSTEMS_PARAM_DECL
#define ACCELERATOR_INTERSECT_PARAM ,nodes, qbvhMemMap, leafNodes, leafQuadTris MQBVH_TRANSFORMATIONS_PARAM MQBVH_MOTIONSYSTEMS_PARAM

void Accelerator_Intersect(
		const Ray *ray,
		RayHit *rayHit
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	// Prepare the ray for intersection
    const float3 rayOrig = VLOAD3F_Private(&ray->o.x);
    const float3 rayDir = VLOAD3F_Private(&ray->d.x);

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

	const float rayTime = ray->time;

	rayHit->meshIndex = NULL_INDEX;
	rayHit->triangleIndex = NULL_INDEX;

	//------------------------------
	// Main loop
	int todoNode = 0; // the index in the stack
	int nodeStack[STACK_SIZE];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		const int nodeData = nodeStack[todoNode];
		--todoNode;

		// Leaves are identified by a negative index
		if (!QBVHNode_IsLeaf(nodeData)) {
			__global const QBVHNode* restrict node = &nodes[nodeData];
            const int4 visit = QBVHNode_BBoxIntersect(
                node->bboxes[signs0][0], node->bboxes[1 - signs0][0],
                node->bboxes[signs1][1], node->bboxes[1 - signs1][1],
                node->bboxes[signs2][2], node->bboxes[1 - signs2][2],
                &ray4,
				invDir0, invDir1, invDir2);

			const int4 children = node->children;

			// For some reason doing logic operations with int4 are very slow
			nodeStack[todoNode + 1] = children.s3;
			todoNode += (visit.s3 && !QBVHNode_IsEmpty(children.s3)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s2;
			todoNode += (visit.s2 && !QBVHNode_IsEmpty(children.s2)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s1;
			todoNode += (visit.s1 && !QBVHNode_IsEmpty(children.s1)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s0;
			todoNode += (visit.s0 && !QBVHNode_IsEmpty(children.s0)) ? 1 : 0;
		} else {
			// Perform intersection with QBVH leaf
			const uint leafIndex = QBVHNode_FirstQuadIndex(nodeData);

            Ray tray;
			float3 newRayOrig = rayOrig;
			float3 newRayDir = rayDir;

			if (leafTransformationIndex[leafIndex] != NULL_INDEX) {
				__global const Matrix4x4* restrict m = &leafTransformations[leafTransformationIndex[leafIndex]];
				newRayOrig = Matrix4x4_ApplyPoint(m, newRayOrig);
				newRayDir = Matrix4x4_ApplyVector(m, newRayDir);
			}

			if (leafMotionSystems[leafIndex].interpolatedTransformFirstIndex != NULL_INDEX) {
				// Transform ray origin and direction
				Matrix4x4 m;
				MotionSystem_Sample(&leafMotionSystems[leafIndex], rayTime, leafInterpolatedTransforms, &m);
				newRayOrig = Matrix4x4_ApplyPoint_Private(&m, newRayOrig);
				newRayDir = Matrix4x4_ApplyVector_Private(&m, newRayDir);
			}

			tray.o.x = newRayOrig.x;
			tray.o.y = newRayOrig.y;
			tray.o.z = newRayOrig.z;
			tray.d.x = newRayDir.x;
			tray.d.y = newRayDir.y;
			tray.d.z = newRayDir.z;
			tray.mint = ray4.mint.s0;
			tray.maxt = ray4.maxt.s0;
			tray.time = rayTime;

            const uint memIndex = leafIndex * 2;
            const uint leafNodeOffset = qbvhMemMap[memIndex];
            __global const QBVHNode* restrict n = &leafNodes[leafNodeOffset];
            const uint leafQuadTriOffset = qbvhMemMap[memIndex + 1];
            __global const QuadTiangle* restrict qt = &leafQuadTris[leafQuadTriOffset];

            RayHit tmpRayHit;
            LeafIntersect(&tray, &tmpRayHit, n, qt);

            if (tmpRayHit.meshIndex != NULL_INDEX) {
                rayHit->t = tmpRayHit.t;
                rayHit->b1 = tmpRayHit.b1;
                rayHit->b2 = tmpRayHit.b2;
                rayHit->meshIndex = leafIndex;
				rayHit->triangleIndex = tmpRayHit.triangleIndex;

                ray4.maxt = (float4)tmpRayHit.t;
            }
		}
	}
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
