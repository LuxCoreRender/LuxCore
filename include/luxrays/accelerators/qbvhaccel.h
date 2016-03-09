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

#ifndef _LUXRAYS_QBVHACCEL_H
#define	_LUXRAYS_QBVHACCEL_H

#include <xmmintrin.h>

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"
#include "luxrays/utils/memory.h"

namespace luxrays {

#if defined(WIN32) && !defined(__CYGWIN__)
class __declspec(align(16)) QuadRay {
#else
class QuadRay {
#endif
public:
	QuadRay(const Ray &ray)
	{
		ox = _mm_set1_ps(ray.o.x);
		oy = _mm_set1_ps(ray.o.y);
		oz = _mm_set1_ps(ray.o.z);

		dx = _mm_set1_ps(ray.d.x);
		dy = _mm_set1_ps(ray.d.y);
		dz = _mm_set1_ps(ray.d.z);

		mint = _mm_set1_ps(ray.mint);
		maxt = _mm_set1_ps(ray.maxt);
	}

	__m128 ox, oy, oz;
	__m128 dx, dy, dz;
	mutable __m128 mint, maxt;
#if defined(WIN32) && !defined(__CYGWIN__)
};
#else
} __attribute__ ((aligned(16)));
#endif

class QuadTriangle : public Aligned16 {
public:

	QuadTriangle() { };

	QuadTriangle(const std::deque<const Mesh *> &meshes,
			const unsigned int m0, const unsigned int m1, const unsigned int m2, const unsigned int m3,
			const unsigned int i0, const unsigned int i1, const unsigned int i2, const unsigned int i3) {

		meshIndex[0] = m0;
		meshIndex[1] = m1;
		meshIndex[2] = m2;
		meshIndex[3] = m3;

		triangleIndex[0] = i0;
		triangleIndex[1] = i1;
		triangleIndex[2] = i2;
		triangleIndex[3] = i3;

		for (u_int i = 0; i < 4; ++i) {
			const Mesh *mesh = meshes[meshIndex[i]];
			const Triangle *t = &(mesh->GetTriangles()[triangleIndex[i]]);

			const Point p0 = mesh->GetVertex(0.f, t->v[0]);
			const Point p1 = mesh->GetVertex(0.f, t->v[1]);
			const Point p2 = mesh->GetVertex(0.f, t->v[2]);
			reinterpret_cast<float *> (&origx)[i] = p0.x;
			reinterpret_cast<float *> (&origy)[i] = p0.y;
			reinterpret_cast<float *> (&origz)[i] = p0.z;

			reinterpret_cast<float *> (&edge1x)[i] = p1.x - p0.x;
			reinterpret_cast<float *> (&edge1y)[i] = p1.y - p0.y;
			reinterpret_cast<float *> (&edge1z)[i] = p1.z - p0.z;

			reinterpret_cast<float *> (&edge2x)[i] = p2.x - p0.x;
			reinterpret_cast<float *> (&edge2y)[i] = p2.y - p0.y;
			reinterpret_cast<float *> (&edge2z)[i] = p2.z - p0.z;
		}
	}

	~QuadTriangle() {
	}

	bool Intersect(const QuadRay &ray4, const Ray &ray, RayHit *rayHit) const {
		const __m128 zero = _mm_setzero_ps();
		const __m128 s1x = _mm_sub_ps(_mm_mul_ps(ray4.dy, edge2z),
				_mm_mul_ps(ray4.dz, edge2y));
		const __m128 s1y = _mm_sub_ps(_mm_mul_ps(ray4.dz, edge2x),
				_mm_mul_ps(ray4.dx, edge2z));
		const __m128 s1z = _mm_sub_ps(_mm_mul_ps(ray4.dx, edge2y),
				_mm_mul_ps(ray4.dy, edge2x));
		const __m128 divisor = _mm_add_ps(_mm_mul_ps(s1x, edge1x),
				_mm_add_ps(_mm_mul_ps(s1y, edge1y),
				_mm_mul_ps(s1z, edge1z)));
		__m128 test = _mm_cmpneq_ps(divisor, zero);
		const __m128 inverse = _mm_div_ps(_mm_set_ps1(1.f), divisor);
		const __m128 dx = _mm_sub_ps(ray4.ox, origx);
		const __m128 dy = _mm_sub_ps(ray4.oy, origy);
		const __m128 dz = _mm_sub_ps(ray4.oz, origz);
		const __m128 b1 = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(dx, s1x),
				_mm_add_ps(_mm_mul_ps(dy, s1y), _mm_mul_ps(dz, s1z))),
				inverse);
		test = _mm_and_ps(test, _mm_cmpge_ps(b1, zero));
		const __m128 s2x = _mm_sub_ps(_mm_mul_ps(dy, edge1z),
				_mm_mul_ps(dz, edge1y));
		const __m128 s2y = _mm_sub_ps(_mm_mul_ps(dz, edge1x),
				_mm_mul_ps(dx, edge1z));
		const __m128 s2z = _mm_sub_ps(_mm_mul_ps(dx, edge1y),
				_mm_mul_ps(dy, edge1x));
		const __m128 b2 = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(ray4.dx, s2x),
				_mm_add_ps(_mm_mul_ps(ray4.dy, s2y), _mm_mul_ps(ray4.dz, s2z))),
				inverse);
		const __m128 b0 = _mm_sub_ps(_mm_set1_ps(1.f),
				_mm_add_ps(b1, b2));
		test = _mm_and_ps(test, _mm_and_ps(_mm_cmpge_ps(b2, zero),
				_mm_cmpge_ps(b0, zero)));
		const __m128 t = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(edge2x, s2x),
				_mm_add_ps(_mm_mul_ps(edge2y, s2y),
				_mm_mul_ps(edge2z, s2z))), inverse);
		test = _mm_and_ps(test,
				_mm_and_ps(_mm_cmpgt_ps(t, ray4.mint),
				_mm_cmplt_ps(t, ray4.maxt)));

		const int testmask = _mm_movemask_ps(test);		
		if (testmask == 0) return false;

		u_int hit = 0; // Must be initialized because next block might not initialize it
		if ((testmask & (testmask - 1)) == 0) {
			hit = UIntLog2(testmask);
			ray.maxt = reinterpret_cast<const float *> (&t)[hit];
		} else {
			for (u_int i = 0; i < 4; ++i) {
				if (reinterpret_cast<const int *> (&test)[i] && reinterpret_cast<const float *> (&t)[i] < ray.maxt) {
					hit = i;
					ray.maxt = reinterpret_cast<const float *> (&t)[i];
				}
			}
		}

		ray4.maxt = _mm_set1_ps(ray.maxt);

		rayHit->t = ray.maxt;
		rayHit->b1 = reinterpret_cast<const float *> (&b1)[hit];
		rayHit->b2 = reinterpret_cast<const float *> (&b2)[hit];
		rayHit->meshIndex = meshIndex[hit];
		rayHit->triangleIndex = triangleIndex[hit];

		return true;
	}

	__m128 origx, origy, origz;
	__m128 edge1x, edge1y, edge1z;
	__m128 edge2x, edge2y, edge2z;
	unsigned int meshIndex[4], triangleIndex[4];
};

// This code is based on Flexray by Anthony Pajot (anthony.pajot@alumni.enseeiht.fr)

/**
   QBVH accelerator, using the EGSR08 paper as base.
   need SSE !
*/

/**
   the number of bins for construction
*/
#define NB_BINS 8

/**
   The QBVH node structure, 128 bytes long (perfect for cache)
*/
class QBVHNode {
public:	
	// The constant used to represent empty leaves. there would have been
	// a conflict with a normal leaf if there were 16 quads,
	// starting at 2^27 in the quads array... very improbable.
	// using MININT (0x80000000) can produce conflict when initializing a
	// QBVH with less than 4 vertices at the beginning :
	// the number of quads - 1 would give 0, and it would start at 0
	// in the quads array
	static const int32_t emptyLeafNode = 0xffffffff;
	
	/**
	   The 4 bounding boxes, in SoA form, for direct SIMD use
	   (one __m128 for each coordinate)
	*/
	__m128 bboxes[2][3];

	/**
	   The 4 children. If a child is a leaf, its index will be negative,
	   the 4 next bits will code the number of primitives in the leaf
	   (more exactly, nbPrimitives = 4 * (p + 1), where p is the integer
	   interpretation of the 4 bits), and the 27 remaining bits the index
	   of the first quad of the node
	*/
	int32_t children[4];
	
	/**
	   Base constructor, init correct bounding boxes and a "root" node
	   (parentNodeIndex == -1)
	*/
	inline QBVHNode() {
		for (int i = 0; i < 3; ++i) {
			bboxes[0][i] = _mm_set1_ps(INFINITY);
			bboxes[1][i] = _mm_set1_ps(-INFINITY);
		}
		
		// All children are empty leaves by default
		for (int i = 0; i < 4; ++i)
			children[i] = emptyLeafNode;
	}

	/**
	   Indicate whether the ith child is a leaf.
	   @param i
	   @return
	*/
	inline bool ChildIsLeaf(int i) const {
		return (children[i] < 0);
	}

	/**
	   Same thing, directly from the index.
	   @param index
	*/
	inline static bool IsLeaf(int32_t index) {
		return (index < 0);
	}

	/**
	   Indicates whether the ith child is an empty leaf.
	   @param i
	*/
	inline bool LeafIsEmpty(int i) const {
		return (children[i] == emptyLeafNode);
	}

	/**
	   Same thing, directly from the index.
	   @param index
	*/
	inline static bool IsEmpty(int32_t index) {
		return (index == emptyLeafNode);
	}
	
	/**
	   Indicate the number of quads in the ith child, which must be
	   a leaf.
	   @param i
	   @return
	*/
	inline u_int NbQuadsInLeaf(int i) const {
		return static_cast<u_int>((children[i] >> 27) & 0xf) + 1;
	}

	/**
	   Return the number of group of 4 primitives, directly from the index.
	   @param index
	*/
	inline static u_int NbQuadPrimitives(int32_t index) {
		return static_cast<u_int>((index >> 27) & 0xf) + 1;
	}
	
	/**
	   Indicate the number of primitives in the ith child, which must be
	   a leaf.
	   @param i
	   @return
	*/
	inline u_int NbPrimitivesInLeaf(int i) const {
		return NbQuadsInLeaf(i) * 4;
	}

	/**
	   Indicate the index in the quads array of the first quad contained
	   by the the ith child, which must be a leaf.
	   @param i
	   @return
	*/
	inline u_int FirstQuadIndexForLeaf(int i) const {
		return children[i] & 0x07ffffff;
	}
	
	/**
	   Same thing, directly from the index.
	   @param index
	*/
	inline static u_int FirstQuadIndex(int32_t index) {
		return index & 0x07ffffff;
	}

	/**
	   Initialize the ith child as a leaf
	   @param i
 	   @param nbQuads
	   @param firstQuadIndex
	*/
	inline void InitializeLeaf(int i, u_int nbQuads, u_int firstQuadIndex) {
		// Take care to make a valid initialisation of the leaf.
		if (nbQuads == 0) {
			children[i] = emptyLeafNode;
		} else {
			// Put the negative sign in a plateform independent way
			children[i] = 0x80000000;//-1L & ~(-1L >> 1L);
			
			children[i] |=  ((static_cast<int32_t>(nbQuads) - 1) & 0xf) << 27;

			children[i] |= static_cast<int32_t>(firstQuadIndex) & 0x07ffffff;
		}
	}

	/**
	   Set the bounding box for the ith child.
	   @param i
	   @param bbox
	*/
	inline void SetBBox(int i, const BBox &bbox) {
		for (int axis = 0; axis < 3; ++axis) {
			reinterpret_cast<float *>(&(bboxes[0][axis]))[i] = bbox.pMin[axis];
			reinterpret_cast<float *>(&(bboxes[1][axis]))[i] = bbox.pMax[axis];
		}
	}

	
	/**
	   Intersect a ray described by sse variables with the 4 bounding boxes
	   of the node.
	   (the visit array)
	*/
	inline int32_t BBoxIntersect(const QuadRay &ray4, const __m128 invDir[3],
		const int sign[3]) const {
		__m128 tMin = ray4.mint;
		__m128 tMax = ray4.maxt;

		// X coordinate
		tMin = _mm_max_ps(tMin, _mm_mul_ps(_mm_sub_ps(bboxes[sign[0]][0],
				ray4.ox), invDir[0]));
		tMax = _mm_min_ps(tMax, _mm_mul_ps(_mm_sub_ps(bboxes[1 - sign[0]][0],
				ray4.ox), invDir[0]));

		// Y coordinate
		tMin = _mm_max_ps(tMin, _mm_mul_ps(_mm_sub_ps(bboxes[sign[1]][1],
				ray4.oy), invDir[1]));
		tMax = _mm_min_ps(tMax, _mm_mul_ps(_mm_sub_ps(bboxes[1 - sign[1]][1],
				ray4.oy), invDir[1]));

		// Z coordinate
		tMin = _mm_max_ps(tMin, _mm_mul_ps(_mm_sub_ps(bboxes[sign[2]][2],
				ray4.oz), invDir[2]));
		tMax = _mm_min_ps(tMax, _mm_mul_ps(_mm_sub_ps(bboxes[1 - sign[2]][2],
				ray4.oz), invDir[2]));

		//return the visit flags
		return _mm_movemask_ps(_mm_cmpge_ps(tMax, tMin));
	}
};

/***************************************************/
class QBVHAccel : public Accelerator {
public:
	/**
	   Normal constructor.
	*/
	QBVHAccel(const Context *context, u_int mp, u_int fst, u_int sf);

	/**
	   to free the memory.
	*/
	virtual ~QBVHAccel();

	virtual AcceleratorType GetType() const { return ACCEL_QBVH; }
	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const;
	virtual bool CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const;
	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount);

	/**
	   Intersect a ray in world space against the
	   primitive and fills in an Intersection object.
	*/
	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

	friend class MQBVHAccel;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLQBVHKernels;
	friend class OpenCLMQBVHKernels;
#endif

private:
	// A special initialization method used only by MQBVHAccel
	void Init(const Mesh *m, const TriangleMeshID *preprocessedMeshIDs);

	/**
	   Build the tree that will contain the primitives indexed from start
	   to end in the primsIndexes array.
	*/
	void BuildTree(u_int start, u_int end, std::vector<u_int> &meshIndexes, std::vector<u_int> &triangleIndexes,
		std::vector<std::vector<BBox> > &primsBboxes, std::vector<std::vector<Point> > &primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex,
		int32_t childIndex, int depth);
	
	/**
	   Create a leaf using the traditional QBVH layout
	*/
	void CreateTempLeaf(int32_t parentIndex, int32_t childIndex,
		u_int start, u_int end, const BBox &nodeBbox);

	/**
	   Create an intermediate node
	*/
	inline int32_t CreateIntermediateNode(int32_t parentIndex,
		int32_t childIndex, const BBox &nodeBbox) {
		int32_t index = nNodes++; // increment after assignment
		if (nNodes >= maxNodes) {
			QBVHNode *newNodes = AllocAligned<QBVHNode>(2 * maxNodes);
			memcpy(newNodes, nodes, sizeof(QBVHNode) * maxNodes);
			for (u_int i = 0; i < maxNodes; ++i)
				newNodes[maxNodes + i] = QBVHNode();
			FreeAligned(nodes);
			nodes = newNodes;
			maxNodes *= 2;
		}

		if (parentIndex >= 0) {
			nodes[parentIndex].children[childIndex] = index;
			nodes[parentIndex].SetBBox(childIndex, nodeBbox);
		}
		return index;
	}

	/**
	   switch a node and its subnodes from the
	   traditional form of QBVH to the pre-swizzled one.
	*/
	void PreSwizzle(int32_t nodeIndex, std::vector<u_int> &meshIndexes, std::vector<u_int> &triangleIndexes);

	/**
	   Create a leaf using the pre-swizzled layout,
	   using the informations stored in the node that
	   are organized following the traditional layout
	*/
	void CreateSwizzledLeaf(int32_t parentIndex, int32_t childIndex, 
		std::vector<u_int> &meshIndexes, std::vector<u_int> &triangleIndexes);

	/**
	   the actual number of quads
	*/
	u_int nQuads;

	/**
	   The primitive associated with each triangle. indexed by the number of quad
	   and the number of triangle in the quad (thus, there might be holes).
	   no need to be a tesselated primitive, the intersection
	   test will be redone for the nearest triangle found, to
	   fill the Intersection structure.
	*/
	QuadTriangle *prims;

	/**
	   The nodes of the QBVH.
	*/
	QBVHNode *nodes;

	/**
	   The number of nodes really used.
	*/
	u_int nNodes, maxNodes;

	/**
	   The world bounding box of the QBVH.
	*/
	BBox worldBound;

	/**
	   The number of primitives in the node that makes switch
	   to full sweep for binning
	*/
	const u_int fullSweepThreshold;

	/**
	   The skip factor for binning
	*/
	const u_int skipFactor;

	/**
	   The maximum number of primitives per leaf
	*/
	const u_int maxPrimsPerLeaf;

	const Context *ctx;
	std::deque<const Mesh *> meshes;

	int maxDepth;

	bool initialized;
};

}

#endif	/* _LUXRAYS_QBVHACCEL_H */
