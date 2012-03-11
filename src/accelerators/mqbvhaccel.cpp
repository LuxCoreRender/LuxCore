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

#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/core/exttrianglemesh.h"

using namespace luxrays;

MQBVHAccel::MQBVHAccel(const Context *context,
		u_int fst, u_int sf) : fullSweepThreshold(fst),
		skipFactor(sf), accels(MeshPtrCompare), ctx(context) {
	initialized = false;
}

MQBVHAccel::~MQBVHAccel() {
	if (initialized) {
		FreeAligned(nodes);

		delete[] meshTriangleIDs;
		delete[] meshIDs;
		delete[] leafsOffset;
		delete[] leafsInvTransform;
		delete[] leafs;

		for (std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::iterator it = accels.begin(); it != accels.end(); it++)
			delete it->second;
	}
}

bool MQBVHAccel::MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void MQBVHAccel::Init(const std::deque<Mesh *> &meshes, const unsigned int totalVertexCount,
		const unsigned int totalTriangleCount) {
	assert (!initialized);

	meshList = meshes;

	// Build a QBVH for each mesh
	nLeafs = meshList.size();
	LR_LOG(ctx, "MQBVH leaf count: " << nLeafs);

	leafs = new QBVHAccel*[nLeafs];
	leafsInvTransform = new const Transform*[nLeafs];
	leafsOffset = new unsigned int[nLeafs];
	meshIDs = new TriangleMeshID[totalTriangleCount];
	meshTriangleIDs = new TriangleID[totalTriangleCount];
	unsigned int currentOffset = 0;
	double lastPrint = WallClockTime();
	for (unsigned int i = 0; i < nLeafs; ++i) {
		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			LR_LOG(ctx, "Building QBVH for MQBVH leaf: " << i);
			lastPrint = now;
		}

		switch (meshList[i]->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE: {
				leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
				leafs[i]->Init(meshList[i]);
				accels[meshList[i]] = leafs[i];

				leafsInvTransform[i] = NULL;
				break;
			}
			case TYPE_TRIANGLE_INSTANCE: {
				InstanceTriangleMesh *itm = (InstanceTriangleMesh *)meshList[i];

				// Check if a QBVH has already been created
				std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::iterator it = accels.find(itm->GetTriangleMesh());

				if (it == accels.end()) {
					// Create a new QBVH
					leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
					leafs[i]->Init(itm);
					accels[itm->GetTriangleMesh()] = leafs[i];
				} else {
					//LR_LOG(ctx, "Cached QBVH leaf");
					leafs[i] = it->second;
				}

				leafsInvTransform[i] = &itm->GetInvTransformation();
				break;
			}
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				ExtInstanceTriangleMesh *eitm = (ExtInstanceTriangleMesh *)meshList[i];

				// Check if a QBVH has already been created
				std::map<Mesh *, QBVHAccel *, bool (*)(Mesh *, Mesh *)>::iterator it = accels.find(eitm->GetExtTriangleMesh());
				if (it == accels.end()) {
					// Create a new QBVH
					leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
					leafs[i]->Init(eitm);
					accels[eitm->GetExtTriangleMesh()] = leafs[i];
				} else {
					//LR_LOG(ctx, "Cached QBVH leaf");
					leafs[i] = it->second;
				}

				leafsInvTransform[i] = &eitm->GetInvTransformation();
				break;
			}
			default:
				assert (false);
				break;
		}

		leafsOffset[i] = currentOffset;

		for (unsigned int j = 0; j < meshList[i]->GetTotalTriangleCount(); ++j) {
			meshIDs[currentOffset + j] = i;
			meshTriangleIDs[currentOffset + j] = j;
		}

		currentOffset += meshList[i]->GetTotalTriangleCount();
	}

	maxNodes = 2 * nLeafs - 1;
	nodes = AllocAligned<QBVHNode>(maxNodes);

	Update();
	
	initialized = true;
}

void MQBVHAccel::Update() {
	// Temporary data for building
	u_int *primsIndexes = new u_int[nLeafs];

	nNodes = 0;
	for (u_int i = 0; i < maxNodes; ++i)
		nodes[i] = QBVHNode();

	// The arrays that will contain
	// - the bounding boxes for all leafs
	// - the centroids for all leafs
	BBox *primsBboxes = new BBox[nLeafs];
	Point *primsCentroids = new Point[nLeafs];
	// The bouding volume of all the centroids
	BBox centroidsBbox;

	// Fill each base array
	for (u_int i = 0; i < nLeafs; ++i) {
		// This array will be reorganized during construction.
		primsIndexes[i] = i;

		// Compute the bounding box for the triangle
		primsBboxes[i] = meshList[i]->GetBBox();
		primsBboxes[i].Expand(RAY_EPSILON);
		primsCentroids[i] = (primsBboxes[i].pMin + primsBboxes[i].pMax) * .5f;

		// Update the global bounding boxes
		worldBound = Union(worldBound, primsBboxes[i]);
		centroidsBbox = Union(centroidsBbox, primsCentroids[i]);
	}

	// Recursively build the tree
	LR_LOG(ctx, "Building MQBVH, leafs: " << nLeafs << ", initial nodes: " << maxNodes);

	BuildTree(0, nLeafs, primsIndexes, primsBboxes, primsCentroids,
			worldBound, centroidsBbox, -1, 0, 0);

	LR_LOG(ctx, "MQBVH completed with " << nNodes << "/" << maxNodes << " nodes");
	LR_LOG(ctx, "Total MQBVH memory usage: " << nNodes * sizeof(QBVHNode) / 1024 << "Kbytes");

	// Release temporary memory
	delete[] primsBboxes;
	delete[] primsCentroids;
	delete[] primsIndexes;
}

void MQBVHAccel::BuildTree(u_int start, u_int end, u_int *primsIndexes,
		BBox *primsBboxes, Point *primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex, int32_t childIndex, int depth) {
	// Create a leaf ?
	//********
	if (end - start == 1) {
		CreateLeaf(parentIndex, childIndex, primsIndexes[start], nodeBbox);
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

	if (k1 == INFINITY)
		throw std::runtime_error("MQBVH unable to handle geometry, too many primitives with the same centroid");

	// Create an intermediate node if the depth indicates to do so.
	// Register the split axis.
	if (depth % 2 == 0) {
		currentNode = CreateNode(parentIndex, childIndex, nodeBbox);
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
			// this object is on the left side
			leftChildBbox = Union(leftChildBbox, primsBboxes[primIndex]);
			leftChildCentroidsBbox = Union(leftChildCentroidsBbox, primsCentroids[primIndex]);
		} else {
			// Update the bounding boxes,
			// this object is on the right side.
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

int32_t  MQBVHAccel::CreateNode(int32_t parentIndex, int32_t childIndex,
	const BBox &nodeBbox) {
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

void MQBVHAccel::CreateLeaf(int32_t parentIndex, int32_t childIndex,
		u_int start, const BBox &nodeBbox) {
	// The leaf is directly encoded in the intermediate node.
	if (parentIndex < 0) {
		// The entire tree is a leaf
		nNodes = 1;
		parentIndex = 0;
	}

	QBVHNode &node = nodes[parentIndex];
	node.SetBBox(childIndex, nodeBbox);
	node.InitializeLeaf(childIndex, 1, start);
}

bool MQBVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
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

			const unsigned int leafIndex = QBVHNode::FirstQuadIndex(leafData);
			QBVHAccel *qbvh = leafs[leafIndex];

			if (leafsInvTransform[leafIndex]) {
				Ray r = (*leafsInvTransform[leafIndex])(*ray);
				RayHit rh;
				rh.SetMiss();
				if (qbvh->Intersect(&r, &rh)) {
					rayHit->t = rh.t;
					rayHit->b1 = rh.b1;
					rayHit->b2 = rh.b2;
					rayHit->index = rh.index + leafsOffset[leafIndex];

					ray->maxt = rh.t;
				}
			} else {
				RayHit rh;
				rh.SetMiss();
				if (qbvh->Intersect(ray, &rh)) {
					rayHit->t = rh.t;
					rayHit->b1 = rh.b1;
					rayHit->b2 = rh.b2;
					rayHit->index = rh.index + leafsOffset[leafIndex];
				}
			}
		}//end of the else
	}

	return !rayHit->Miss();
}
