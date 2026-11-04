#line 2 "bvhbuild_types.cl"

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

typedef struct {
	// Note: this is not a union (anymore) because the unions in buffer
	// layouts are not supported by all the OpenCL compilers (e.g. clspv,
	// used to run the kernels over Vulkan, doesn't). The views over the
	// raw storage are provided by the BVHArrayNode_* macros below and the
	// layout is the same as the one of the host side (see
	// include/luxrays/core/bvh/bvhbuild.h)
	float data[6];

	// Most significant bit is used to mark leafs
	unsigned int nodeData;
	int pad0; // To align to float4
} BVHArrayNode;

// Views over BVHArrayNode.data (see the note above)
#define BVHArrayNode_BBoxMin(node, i) ((node).data[i])
#define BVHArrayNode_BBoxMax(node, i) ((node).data[3 + (i)])
#define BVHArrayNode_TriangleV(node, i) (as_uint((node).data[i]))
#define BVHArrayNode_TriangleMeshIndex(node) (as_uint((node).data[3]))
#define BVHArrayNode_TriangleIndex(node) (as_uint((node).data[4]))
#define BVHArrayNode_LeafIndex(node) (as_uint((node).data[0]))
#define BVHArrayNode_TransformIndex(node) (as_uint((node).data[1]))
#define BVHArrayNode_MotionIndex(node) (as_uint((node).data[2]))
#define BVHArrayNode_MeshOffsetIndex(node) (as_uint((node).data[3]))

typedef struct {
	// Note: this is not a union (anymore) because the unions in buffer
	// layouts are not supported by all the OpenCL compilers (e.g. clspv,
	// used to run the kernels over Vulkan, doesn't). The views over the
	// raw storage are provided by the IndexBVHArrayNode_* macros below
	// and the layout is the same as the one of the host side (see
	// include/slg/core/indexbvh.h)
	float data[6];

	// Most significant bit is used to mark leafs
	unsigned int nodeData;
	int pad; // To align to float4
} IndexBVHArrayNode;

// Views over IndexBVHArrayNode.data (see the note above)
#define IndexBVHArrayNode_BBoxMin(node, i) ((node).data[i])
#define IndexBVHArrayNode_BBoxMax(node, i) ((node).data[3 + (i)])
#define IndexBVHArrayNode_EntryIndex(node) (as_uint((node).data[0]))

#define IndexBVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define IndexBVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
