/***************************************************************************
 *   Copyright (C) 2007 by Anthony Pajot   
 *   anthony.pajot@etu.enseeiht.fr
 *
 * This file is part of FlexRay
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/

#include "luxrays/accelerators/qbvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"

using namespace luxrays;

/***************************************************/

QBVHAccel::QBVHAccel(const Context *context,
		u_int mp, u_int fst, u_int sf) : fullSweepThreshold(fst),
		skipFactor(sf), maxPrimsPerLeaf(mp), ctx(context) {
	initialized = false;
	preprocessedMesh = NULL;
	mesh = NULL;
	meshIDs = NULL;
	meshTriangleIDs = NULL;
}

QBVHAccel::~QBVHAccel() {
	if (initialized) {
		FreeAligned(prims);
		FreeAligned(nodes);

		if (preprocessedMesh) {
			preprocessedMesh->Delete();
			delete preprocessedMesh;
		}
		delete[] meshIDs;
		delete[] meshTriangleIDs;
	}
}


void QBVHAccel::Init(const std::deque<Mesh *> &meshes, const unsigned int totalVertexCount,
		const unsigned int totalTriangleCount) {
	assert (!initialized);

	TriangleMesh *mesh = TriangleMesh::Merge(totalVertexCount, totalTriangleCount,
			meshes, &meshIDs, &meshTriangleIDs);
	assert (mesh->GetTotalVertexCount() == totalVertexCount);
	assert (mesh->GetTotalTriangleCount() == totalTriangleCount);

	LR_LOG(ctx, "Total vertices memory usage: " << totalVertexCount * sizeof(Point) / 1024 << "Kbytes");
	LR_LOG(ctx, "Total triangles memory usage: " << totalTriangleCount * sizeof(Triangle) / 1024 << "Kbytes");

	Init(mesh);
}

void QBVHAccel::Init(const Mesh *m) {
	assert (!initialized);

	mesh = m;
	const unsigned int totalTriangleCount = mesh->GetTotalTriangleCount();

	// Temporary data for building
	u_int *primsIndexes = new u_int[totalTriangleCount + 3]; // For the case where
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
	BBox *primsBboxes = new BBox[totalTriangleCount];
	Point *primsCentroids = new Point[totalTriangleCount];
	// The bouding volume of all the centroids
	BBox centroidsBbox;

	const Point *verts = mesh->GetVertices();
	const Triangle *triangles = mesh->GetTriangles();

	// Fill each base array
	for (u_int i = 0; i < totalTriangleCount; ++i) {
		// This array will be reorganized during construction.
		primsIndexes[i] = i;

		// Compute the bounding box for the triangle
		primsBboxes[i] = triangles[i].WorldBound(verts);
		primsBboxes[i].Expand(RAY_EPSILON);
		primsCentroids[i] = (primsBboxes[i].pMin + primsBboxes[i].pMax) * .5f;

		// Update the global bounding boxes
		worldBound = Union(worldBound, primsBboxes[i]);
		centroidsBbox = Union(centroidsBbox, primsCentroids[i]);
	}

	// Arbitrarily take the last primitive for the last 3
	primsIndexes[totalTriangleCount] = totalTriangleCount - 1;
	primsIndexes[totalTriangleCount + 1] = totalTriangleCount - 1;
	primsIndexes[totalTriangleCount + 2] = totalTriangleCount - 1;

	// Recursively build the tree
	LR_LOG(ctx, "Building QBVH, primitives: " << totalTriangleCount << ", initial nodes: " << maxNodes);

	nQuads = 0;
	BuildTree(0, totalTriangleCount, primsIndexes, primsBboxes, primsCentroids,
			worldBound, centroidsBbox, -1, 0, 0);

	prims = AllocAligned<QuadTriangle>(nQuads);
	nQuads = 0;
	PreSwizzle(0, primsIndexes);

	LR_LOG(ctx, "QBVH completed with " << nNodes << "/" << maxNodes << " nodes");
	LR_LOG(ctx, "Total QBVH memory usage: " << nNodes * sizeof(QBVHNode) / 1024 << "Kbytes");
	LR_LOG(ctx, "Total QBVH QuadTriangle count: " << nQuads);

	// Release temporary memory
	delete[] primsBboxes;
	delete[] primsCentroids;
	delete[] primsIndexes;
}

/***************************************************/

void QBVHAccel::BuildTree(u_int start, u_int end, u_int *primsIndexes,
		BBox *primsBboxes, Point *primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex, int32_t childIndex, int depth) {
	// Create a leaf ?
	//********
	if (end - start <= maxPrimsPerLeaf) {
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

	u_int step = (end - start < fullSweepThreshold) ? 1 : skipFactor;

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
		u_int primIndex = primsIndexes[i];

		// Binning is relative to the centroids bbox and to the
		// primitives' centroid.
		const int binId = Min(NB_BINS - 1, Floor2Int(k1 * (primsCentroids[primIndex][axis] - k0)));

		bins[binId]++;
		binsBbox[binId] = Union(binsBbox[binId], primsBboxes[primIndex]);
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
		int rightIndex = NB_BINS - 1 - i;
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
	float splitPos = centroidsBbox.pMin[axis] + (minBin + 1) *
			(centroidsBbox.pMax[axis] - centroidsBbox.pMin[axis]) / NB_BINS;


	BBox leftChildBbox, rightChildBbox;
	BBox leftChildCentroidsBbox, rightChildCentroidsBbox;

	u_int storeIndex = start;
	for (u_int i = start; i < end; ++i) {
		u_int primIndex = primsIndexes[i];

		if (primsCentroids[primIndex][axis] <= splitPos) {
			// Swap
			primsIndexes[i] = primsIndexes[storeIndex];
			primsIndexes[storeIndex] = primIndex;
			++storeIndex;

			// Update the bounding boxes,
			// this triangle is on the left side
			leftChildBbox = Union(leftChildBbox, primsBboxes[primIndex]);
			leftChildCentroidsBbox = Union(leftChildCentroidsBbox, primsCentroids[primIndex]);
		} else {
			// Update the bounding boxes,
			// this triangle is on the right side.
			rightChildBbox = Union(rightChildBbox, primsBboxes[primIndex]);
			rightChildCentroidsBbox = Union(rightChildCentroidsBbox, primsCentroids[primIndex]);
		}
	}

	// Build recursively
	BuildTree(start, storeIndex, primsIndexes, primsBboxes, primsCentroids,
			leftChildBbox, leftChildCentroidsBbox, currentNode,
			leftChildIndex, depth + 1);
	BuildTree(storeIndex, end, primsIndexes, primsBboxes, primsCentroids,
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

void QBVHAccel::PreSwizzle(int32_t nodeIndex, u_int *primsIndexes) {
	for (int i = 0; i < 4; ++i) {
		if (nodes[nodeIndex].ChildIsLeaf(i))
			CreateSwizzledLeaf(nodeIndex, i, primsIndexes);
		else
			PreSwizzle(nodes[nodeIndex].children[i], primsIndexes);
	}
}

void QBVHAccel::CreateSwizzledLeaf(int32_t parentIndex, int32_t childIndex,
		u_int *primsIndexes) {
	QBVHNode &node = nodes[parentIndex];
	if (node.LeafIsEmpty(childIndex))
		return;
	const u_int startQuad = nQuads;
	const u_int nbQuads = node.NbQuadsInLeaf(childIndex);

	u_int primOffset = node.FirstQuadIndexForLeaf(childIndex);
	u_int primNum = nQuads;

	const Point *vertices = mesh->GetVertices();
	const Triangle *triangles = mesh->GetTriangles();

	for (u_int q = 0; q < nbQuads; ++q) {
		new (&prims[primNum]) QuadTriangle(triangles, vertices, primsIndexes[primOffset],
				primsIndexes[primOffset + 1], primsIndexes[primOffset + 2], primsIndexes[primOffset + 3]);

		++primNum;
		primOffset += 4;
	}
	nQuads += nbQuads;
	node.InitializeLeaf(childIndex, nbQuads, startQuad);
}

/***************************************************/

bool QBVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
	//------------------------------
	// Prepare the ray for intersection
	QuadRay ray4(*ray);
	__m128 invDir[3];
	invDir[0] = _mm_set1_ps(1.f / ray->d.x);
	invDir[1] = _mm_set1_ps(1.f / ray->d.y);
	invDir[2] = _mm_set1_ps(1.f / ray->d.z);

	int signs[3];
	ray->GetDirectionSigns(signs);

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
				prims[primNumber].Intersect(ray4, *ray, rayHit);
		}//end of the else
	}

	return !rayHit->Miss();
}
