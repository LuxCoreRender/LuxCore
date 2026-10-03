/***************************************************************************
 * Copyright 1998-2025 by authors (see AUTHORS.txt)                        *
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

// This shape modifier will merge vertices whose interdistance is lower than
// a given threshold

#include <cassert>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <tuple>
#include <format>
#include <execution>
#include <span>

#include "luxrays/core/trianglemesh.h"
#include "oneapi/tbb.h"
#include "oneapi/tbb/scalable_allocator.h"
#include "oneapi/tbb/cache_aligned_allocator.h"

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/merge_on_distance.h"
#include "slg/utils/group_by_equivalence.h"
#include "slg/scene/scene.h"
#include "luxrays/utils/utils.h"

using luxrays::Point;
using luxrays::WallClockTime;
using namespace oneapi::tbb;


namespace {



class NearlyEqualComparator {
public:

	explicit NearlyEqualComparator(const unsigned int tolerance) :
		boundmin(minus_epsilon * float(tolerance)),
		boundmax(plus_epsilon * float(tolerance))
	{}
	inline bool compare(const float a, const float b) const {
		const float delta1 = b - a;
		const float delta2 = a - b;
		const bool res1 = (boundmin <= delta1 and delta1 <= boundmax);
		const bool res2 = (boundmin <= delta2 and delta2 <= boundmax);
		return res1 or res2;
	}

private:
	inline static const float minus_epsilon =
		std::nextafter(0.f, std::numeric_limits<float>::lowest());

	inline static const float plus_epsilon =
		std::nextafter(0.f, std::numeric_limits<float>::max());

	const float boundmin;
	const float boundmax;
};


inline bool compare_points(
	const luxrays::Point& p1,
	const luxrays::Point& p2,
	const NearlyEqualComparator& comparator
) {
	const bool m0 = comparator.compare(p1[0], p2[0]);
	const bool m1 = comparator.compare(p1[1], p2[1]);
	const bool m2 = comparator.compare(p1[2], p2[2]);
	return m0 && m1 && m2;
}


// Cell Id, for partition indexing
// This id can be handled in two ways:
// - 3-uple of signed int 16
// - an unsigned int 64 (with first 16 bits set to zero)
union CellId {
	CellId(const int16_t x, const int16_t y, const int16_t z) {
		i16[0] = 0;
		i16[1] = x;
		i16[2] = y;
		i16[3] = z;
	}
	CellId(const uint64_t id) : u64(id) {
		if (i16[0]) {
			throw std::runtime_error("Invalid CellId");
		}
	}
	CellId(const CellId& other) : u64(other.u64) {};

	CellId(const CellId& other, int16_t dx, int16_t dy, int16_t dz) :
		u64(other.u64) {
		i16[1] += dx;
		i16[2] += dy;
		i16[3] += dz;
	};

	inline int16_t x() const { return i16[1]; }
	inline int16_t y() const { return i16[2]; }
	inline int16_t z() const { return i16[3]; }
	inline uint64_t id() const { return u64; }

	inline bool operator==(const CellId& other) const {
		return u64 == other.u64;
	}
	inline bool out_of_bound(int16_t dx, int16_t dy, int16_t dz) const {
		bool res =
			   (x() == INT16_MIN && dx == -1)
			or (x() == INT16_MAX && dx == +1)
			or (y() == INT16_MIN && dy == -1)
			or (y() == INT16_MAX && dy == +1)
			or (z() == INT16_MIN && dz == -1)
			or (z() == INT16_MAX && dz == +1);
		return res;
	}
private:
	uint64_t u64;
	int16_t i16[4];
};

}

// Introduce hash into std namespace
namespace std {
template <>
struct hash<CellId>
{
	std::size_t operator()(const CellId& x) const noexcept
	{
		std::size_t h = std::hash<uint64_t>{}(x.id());
		return h;
    }
};
}  // namespace std

namespace {

// Grid for spatial partitioning
class Grid {
public:
	Grid(
		const Point& p_origin,
		float p_cellSizeX,
		float p_cellSizeY,
		float p_cellSizeZ
	) :
		m_origin(p_origin),
		m_cellSizeX(p_cellSizeX),
		m_cellSizeY(p_cellSizeY),
		m_cellSizeZ(p_cellSizeZ)
	{};

	const Point& origin() const { return m_origin; }
	const float cellSizeX() const { return m_cellSizeX; }
	const float cellSizeY() const { return m_cellSizeY; }
	const float cellSizeZ() const { return m_cellSizeZ; }

private:
	const Point m_origin;
	const float m_cellSizeX, m_cellSizeY, m_cellSizeZ;

};



// Create a grid, ie give an origin point (or midPoint) and cell sizes on X, Y,
// Z
Grid ComputeGrid(const luxrays::Points points) {

	auto numPoints = points.size();

	constexpr float minlimit = std::numeric_limits<float>::min();
	constexpr float maxlimit = std::numeric_limits<float>::max();

	using sixfloats = std::tuple<float, float, float, float, float, float>;

	constexpr size_t grain = 1024;

	// Compute bounding box
	auto boundingbox = parallel_reduce(
		// Range
		blocked_range<const Point*>(points.data(), points.data() + numPoints, grain),

		// Init
		std::make_tuple(maxlimit, maxlimit, maxlimit, minlimit, minlimit, minlimit),

		// Body
		[](const blocked_range<const Point*>& r, sixfloats init) -> sixfloats {
			auto [minX, minY, minZ, maxX, maxY, maxZ] = init;
			for(auto p=r.begin(); p!=r.end(); ++p) {
				minX = std::min(minX, p->x);
				minY = std::min(minY, p->y);
				minZ = std::min(minZ, p->z);
				maxX = std::max(maxX, p->x);
				maxY = std::max(maxY, p->y);
				maxZ = std::max(maxZ, p->z);
			}
			return std::make_tuple(minX, minY, minZ, maxX, maxY, maxZ);
		},

		// Reduce
		[](const sixfloats& a, const sixfloats& b) {
			const auto [minXa, minYa, minZa, maxXa, maxYa, maxZa] = a;
			const auto [minXb, minYb, minZb, maxXb, maxYb, maxZb] = b;
			return std::make_tuple(
				std::min(minXa, minXb),
				std::min(minYa, minYb),
				std::min(minZa, minZb),
				std::max(maxXa, maxXb),
				std::max(maxYa, maxYb),
				std::max(maxZa, maxZb)
			);
		}
	);

	const auto [minX, minY, minZ, maxX, maxY, maxZ] = boundingbox;
	Point midPoint((minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2);

	// Compute cell sizes
	constexpr float numCells = static_cast<float>(1 << 16);
	float cellSizeX = (maxX - minX) / numCells;
	float cellSizeY = (maxY - minY) / numCells;
	float cellSizeZ = (maxZ - minZ) / numCells;

	// Make the cells slightly larger than the bounding box
	cellSizeX = std::nextafterf(cellSizeX, INFINITY);
	cellSizeY = std::nextafterf(cellSizeY, INFINITY);
	cellSizeZ = std::nextafterf(cellSizeZ, INFINITY);

	SDL_LOG("Merge On Distance - Grid dimensions: Origin = "
		<< midPoint << " Cell size = ("
		<< cellSizeX << ", " << cellSizeY << ", " << cellSizeZ << ")"
	)

	return Grid(midPoint, cellSizeX, cellSizeY, cellSizeZ);

}

// The partition object (and subobjects)

using PartitionPoint = std::pair<size_t, luxrays::Point>;

// Partition element: a point number and its coordinates, padded and aligned
struct alignas(std::hardware_destructive_interference_size)
PartitionElem : public PartitionPoint {
	PartitionElem(size_t id, const luxrays::Point point)
		: PartitionPoint(id, point) {}
};

using PartitionBucket = std::vector<
	PartitionElem,
	tbb::cache_aligned_allocator<PartitionElem>
>;
using PartitionAllocator = tbb::scalable_allocator<
	std::pair<const CellId, PartitionBucket>
>;
using PartitionHash = std::hash<CellId>;
using PartitionEqual = std::equal_to<CellId>;

using Partition = std::unordered_map<
	CellId,
	PartitionBucket,
	PartitionHash,
	PartitionEqual,
	PartitionAllocator
>;

// Assign points to grid (do the partitioning)
Partition AssignPointsToGrid(
	const Grid& grid, const luxrays::Points points
) {
	auto numPoints = points.size();

	// Avoid tiny sets of data for body
	constexpr size_t grain = 1024;

	auto partition = tbb::parallel_reduce(
		// Range
		blocked_range<size_t>(0, numPoints, grain),

		// Init
		Partition(numPoints),

		// Body
		[&](const blocked_range<size_t>& r, Partition&& partition) -> Partition {
			for (size_t i = r.begin(); i != r.end(); ++i) {
				const auto point = points[i];

				const auto p = point - grid.origin();
				const auto cellX = static_cast<int16_t>(p.x / grid.cellSizeX());
				const auto cellY = static_cast<int16_t>(p.y / grid.cellSizeY());
				const auto cellZ = static_cast<int16_t>(p.z / grid.cellSizeZ());
				const CellId cellId(cellX, cellY, cellZ);

				partition[cellId].emplace_back(i, point);
			}
			return partition;
		},

		// Reduce
		[](Partition&& partition1, Partition&& partition2) -> Partition {
			partition1.merge(partition2);
			for (const auto& [cellId, partitionBucket2] : partition2) {
				auto& partitionBucket1 = partition1[cellId];
				partitionBucket1.reserve(partitionBucket1.size() + partitionBucket2.size());
				partitionBucket1.insert(
					partitionBucket1.end(), partitionBucket2.begin(), partitionBucket2.end()
				);
			}
			return partition1;
		}
	);

	// Debug
#if 0
	size_t sup = 0;
	size_t count = 0;
	for (auto const& [key, value] : partition) {
		sup = std::max(value.size(), sup);
		count += value.size();
	}
	SDL_LOG("Grid sup/total: " << sup << " " << count);
#endif

	return partition;
}

constexpr std::array<std::array<int, 3>, 27> adjacency() {
	std::array<std::array<int, 3>, 27> res;
	size_t i = 0;
	for (auto dx : {-1, 0, 1}) {
		for (auto dy : {-1, 0, 1}) {
			for (auto dz : {-1, 0, 1}) {
				res[i++] = {dx, dy, dz};
			}
		}
	}
	return res;
}



// Functor to generate pairs for PointGrouping
// Takes a range [r1, r2) and returns pairs of equivalent point indices
class PointPairGenerator {
	const Partition& partition;
	const NearlyEqualComparator& comparator;
	static constexpr auto ADJACENCY = adjacency();

public:
	PointPairGenerator(const Partition& p_partition,
		const NearlyEqualComparator& p_comparator)
		: partition(p_partition),
		  comparator(p_comparator) {}

	// Returns a vector of pairs for the given range
	std::vector<std::pair<size_t, size_t>> operator()(
		size_t r1, size_t r2) const {
		std::vector<std::pair<size_t, size_t>> pairs;
		auto it = partition.begin();
		std::advance(it, r1);
		for (auto i = r1; i < r2; ++i, ++it) {
			const auto& [cellId, cellPoints] = *it;

			for (const auto& [idx, curPoint] : cellPoints) {
				// Check adjacent cells
				for (const auto [dx, dy, dz] : ADJACENCY) {
					// Skip out-of-bound cells
					if (cellId.out_of_bound(dx, dy, dz)) continue;
					const CellId adjCellId(cellId, dx, dy, dz);

					const auto adjIt = partition.find(adjCellId);
					if (adjIt == partition.end()) continue;
					const auto& adjPoints = adjIt->second;

					// For each point in current cell and for each point
					// in adjacent cell, compute distance
					for (const auto& [adjIdx, adjPoint] : adjPoints) {
						if (idx >= adjIdx) continue;
						if (compare_points(curPoint, adjPoint, comparator)) {
							pairs.emplace_back(idx, adjIdx);
						}
					}
				}  // for dx, dy, dz
			}
		}
		return pairs;
	}
};


using Cluster = std::vector<size_t, tbb::scalable_allocator<size_t>>;

using ClusterMap = std::unordered_map<
	size_t,
	Cluster,
	std::hash<size_t>,
	std::equal_to<size_t>,
	tbb::scalable_allocator<std::pair<const size_t, Cluster>>
>;

ClusterMap GroupPoints(const Partition& partition, size_t numPoints, u_int tolerance) {
	const NearlyEqualComparator comparator(tolerance);

	// Generate all pairs using PointPairGenerator
	std::vector<std::pair<size_t, size_t>> allPairs;
	const PointPairGenerator pairGenerator(partition, comparator);
	const size_t partition_size = partition.size();
	constexpr size_t grain = 1024;

	static tbb::affinity_partitioner tbb_partitioner;
	allPairs.reserve(numPoints * 10); // Reserve approximate space

	// Use parallel_reduce to collect all pairs
	allPairs = tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, partition_size, grain),
		std::vector<std::pair<size_t, size_t>>(),
		[pairGenerator](const tbb::blocked_range<size_t>& r, std::vector<std::pair<size_t, size_t>> localPairs) -> std::vector<std::pair<size_t, size_t>> {
			auto pairs = pairGenerator(r.begin(), r.end());
			localPairs.insert(localPairs.end(), pairs.begin(), pairs.end());
			return localPairs;
		},
		[](std::vector<std::pair<size_t, size_t>> a, const std::vector<std::pair<size_t, size_t>>& b) -> std::vector<std::pair<size_t, size_t>> {
			a.insert(a.end(), b.begin(), b.end());
			return a;
		},
		tbb_partitioner
	);

	// Now use GroupByEquivalence with all pairs
	const auto classes = slg::GroupByEquivalence(numPoints, std::span(allPairs));

	// Convert Classes to ClusterMap
	ClusterMap clusterMap;
	for (const auto& cluster : classes) {
		if (!cluster.empty()) {
			const size_t root = cluster[0];
			clusterMap[root] = Cluster(cluster.begin(), cluster.end());
		}
	}
	return clusterMap;
}

// Merge nearby points, with spatial partioning acceleration
//
// This is the entry point of the merging algorithm
//
// Spatial partioning means that the points are first partitioned into a grid,
// and the distance computations are made only between points within the same
// grid cell and adjacent cells. This saves a lot of computations in case of
// large collection of points
//
// Returns:
// - The merged points, in the form of clusters (map of vectors)
//
ClusterMap mergePoints(
	const luxrays::Points points, size_t numPoints, u_int tolerance, bool importNormals
) {

	// Compute grid for spatial partitioning
	const Grid grid{ComputeGrid(points)};

	// Assign points to grids cells (in other words: partition)
	const Partition partition{
		AssignPointsToGrid(grid, points)
	};

	// For each cell, compare points within the cell and adjacent cells
	// and gather points at (nearly) zero distance from each others.
	// Gathering is made via GroupByEquivalence
	const ClusterMap clusters{GroupPoints(partition, numPoints, tolerance)};

	return clusters;

}


// Recreate a mesh, based on a source mesh and a clusterisation
// Replace each variable with interpolated value
luxrays::ExtTriangleMeshUPtr RecreateMesh(
	const luxrays::ExtTriangleMesh& srcMesh,
	const ClusterMap& clustermap,
	bool importNormals
) {
	const auto numPoints = srcMesh.GetTotalVertexCount();
	const auto srcPoints = srcMesh.GetVertices();
	const auto numNewPoints = clustermap.size();

	// Nota: we use smart pointers until the creation of the ExtTriangleMesh,
	// so that the memory is automatically released if something goes wrong in
	// the process. Please keep it so.

	std::vector<Cluster> clusters;
	clusters.reserve(numNewPoints);
	std::transform(
		std::make_move_iterator(clustermap.begin()),
		std::make_move_iterator(clustermap.end()),
		std::back_inserter(clusters),
		[](auto &kv){ return kv.second;}
	);

	std::vector<size_t> pointMap(numPoints);

	// Allocate mesh data structures
	// Points
	luxrays::VertexBuffer newPoints(numNewPoints);
	//TODO
	//std::unique_ptr<Point> newPoints{
		//luxrays::ExtTriangleMesh::AllocVerticesBuffer(numNewPoints)
	//};
	//auto newPointsPtr = newPoints.get();

	// Normals
	luxrays::NormalBuffer newNormals;
	const auto& srcNormals = srcMesh.GetNormals();
	if (importNormals && srcMesh.HasNormals()) {
		newNormals.Allocate(numNewPoints);
	}

	// UV
	luxrays::ExtMeshProp<luxrays::UV> newUVs;
	luxrays::ExtMeshProp<luxrays::UV> srcUVs;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (srcMesh.HasUVs(i)) {
			srcUVs.SetLayer(i, srcMesh.GetUVs(i), numPoints);
			newUVs.AllocateLayer(i, numNewPoints);
		}
	}

	// Colors
	luxrays::ExtMeshProp<luxrays::Spectrum> newColors, srcColors;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (srcMesh.HasColors(i)) {
			srcColors[i] = srcMesh.GetColors(i);
			newColors.AllocateLayer(i, numNewPoints);
		}
	}

	// Alphas
	luxrays::ExtMeshProp<float> newAlphas, srcAlphas;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (srcMesh.HasAlphas(i)) {
			srcAlphas[i] = srcMesh.GetAlphas(i);
			newAlphas.AllocateLayer(i, numNewPoints);
		}
	}

	// VertexAOVs
	luxrays::ExtMeshProp<float>  newVertexAOVs, srcVertexAOVs;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (srcMesh.HasVertexAOV(i)) {
			srcVertexAOVs[i] = srcMesh.GetVertexAOVs(i);
			newVertexAOVs.AllocateLayer(i, numNewPoints);
		}
	}

	// Avoid tiny sets of data for tbb body
	constexpr size_t grain = 1024;

	// Compute merged values of points, normals, uv etc.
	tbb::parallel_for(
        tbb::blocked_range<size_t>(0, numNewPoints, grain),

		[&](tbb::blocked_range<size_t>& r) {

			for (auto newIdx = r.begin(); newIdx != r.end(); ++newIdx) {
				const auto& cluster = clusters[newIdx];
				auto cluster_size = cluster.size();
				if (!cluster_size) continue;

				// Compute point map (mapping between old and new points)
				for (auto oldIdx: cluster) {
					pointMap[oldIdx] = newIdx;
				}

				// Compute merged points (centroids)
				Point newPoint = std::transform_reduce(
					cluster.cbegin(),
					cluster.cend(),
					Point(0, 0, 0),
					std::plus{},
					[&srcPoints](auto idx) -> Point { return srcPoints[idx]; }
				) / cluster_size;
				newPoints[newIdx] = newPoint;

				// Compute merged normals
				if (importNormals && srcMesh.HasNormals()) {
					luxrays::Normal newNormal = std::transform_reduce(
						cluster.cbegin(),
						cluster.cend(),
						luxrays::Normal(0, 0, 0),
						std::plus{},
						[&srcNormals](auto idx) -> luxrays::Normal {
							return srcNormals[idx];
						}
					) / cluster_size;
					const float newNormalLength = newNormal.Length();
					if (newNormalLength) {
						newNormal /= newNormalLength;
					}
					newNormals[newIdx] = newNormal;
				}

				// Compute merged uv
				for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
					if (srcMesh.HasUVs(i)) {
						auto newUVPtr = newUVs[i].get();
						luxrays::UV newUV = std::transform_reduce(
							cluster.cbegin(),
							cluster.cend(),
							luxrays::UV(0, 0),
							std::plus{},
							[&srcUVs, &i](auto idx) -> luxrays::UV {
								return srcUVs[i][idx];
							}
						) / cluster_size;
						newUVs[i].get()[newIdx] = newUV;
					}
				}

				// Compute merged colors
				for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
					if (srcMesh.HasColors(i)) {
						auto newColorsPtr = newColors[i].get();
						luxrays::Spectrum newColor = std::transform_reduce(
							cluster.cbegin(),
							cluster.cend(),
							luxrays::Spectrum(0, 0, 0),
							std::plus{},
							[&srcColors, &i](auto idx) -> luxrays::Spectrum {
								return srcColors[i][idx];
							}
						) / cluster_size;
						newColorsPtr[newIdx] = newColor;
					}
				}

				// Compute merged alphas
				for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
					if (srcMesh.HasAlphas(i)) {
						auto newAlphasPtr = newAlphas[i].get();
						float newAlpha = std::transform_reduce(
							cluster.cbegin(),
							cluster.cend(),
							0.f,
							std::plus{},
							[&srcAlphas, &i](auto idx) -> float {
								return srcAlphas[i][idx];
							}
						) / cluster_size;
						newAlphasPtr[newIdx] = newAlpha;
					}
				}

				// Compute merged vertex AOVs
				for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
					if (srcMesh.HasVertexAOV(i)) {
						auto newVertexAOVsPtr = newVertexAOVs[i].get();
						float newVertexAOV = std::transform_reduce(
							cluster.cbegin(),
							cluster.cend(),
							0.f,
							std::plus{},
							[&srcVertexAOVs, &i](auto idx) -> float {
								return srcVertexAOVs[i][idx];
							}
						) / cluster_size;
						newVertexAOVsPtr[newIdx] = newVertexAOV;
					}
				}
			}  // lambda main for loop
		}  // Body lambda
	);  // tbb::parallel_for

	// Recompute triangles
	const size_t numTriangles = srcMesh.GetTotalTriangleCount();
	SDL_LOG("Number of triangles " << numTriangles);
	auto oldTriangles = srcMesh.GetTriangles();
	luxrays::TriangleBuffer newTriangles(numTriangles);
	//auto newTrianglesPtr = newTriangles.get();
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, numTriangles),
		[&](const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i != r.end(); ++i) {
				const auto& oldTriangle = oldTriangles[i];
				auto newTriangle = luxrays::Triangle(
					pointMap[oldTriangle.v[0]],
					pointMap[oldTriangle.v[1]],
					pointMap[oldTriangle.v[2]]
				);
				newTriangles[i] = newTriangle;
			}
		}
	);

	///TODO
	//// Create layer arrays (release smart pointers...)
	//ExtMeshProp<UV> meshUVs;
	//ExtMeshProp<Spectrum> meshCols;
	//ExtMeshProp<float> meshAlphas;

	//for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		//if (srcMesh.HasUVs(i)) {
			//(*meshUVs)[i] = newUVs[i].release();
		//} else {
			//(*meshUVs)[i] = nullptr;
		//}

		//if (srcMesh.HasColors(i)) {
			//(*meshCols)[i] = newColors[i].release();
		//} else {
			//(*meshCols)[i] = nullptr;
		//}

		//if (srcMesh.HasAlphas(i)) {
			//(*meshAlphas)[i] = newAlphas[i].release();
		//} else {
			//(*meshAlphas)[i] = nullptr;
		//}
	//}

	// Create new mesh
	auto newMesh = std::make_unique<luxrays::ExtTriangleMesh>(
		std::move(newPoints),
		std::move(newTriangles),
		std::move(newNormals),
		newUVs,
		newColors,
		newAlphas,
		srcMesh.GetBevelRadius()
	);

	// Copy AOV to new mesh
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (srcMesh.HasVertexAOV(i)) {
			newMesh->SetVertexAOV(i, newVertexAOVs.GetLayer(i), newVertexAOVs.GetLayerSize());
		}
	}

	return newMesh;
}



} // namespace


namespace slg {

MergeOnDistanceShape::MergeOnDistanceShape(
	luxrays::ExtTriangleMeshRef  srcMesh,
	u_int tolerance,
	bool importNormals
) {

	SDL_LOG("Merge On Distance - Applying to " << srcMesh.GetName());

	const double startTime = WallClockTime();

	mesh = std::move(ApplyMergeOnDistance(srcMesh, tolerance, importNormals));

	const double endTime = WallClockTime();
	SDL_LOG(
		"Merge On Distance - Merging time: " << endTime - startTime << " seconds"
	);
}

MergeOnDistanceShape::~MergeOnDistanceShape() {
}

luxrays::ExtTriangleMeshUPtr
MergeOnDistanceShape::ApplyMergeOnDistance(
	luxrays::ExtTriangleMeshRef srcMesh,
	u_int tolerance,
	bool importNormals
) {


	// Get merged points
	auto clusters = mergePoints(
		srcMesh.GetVertices(),
		srcMesh.GetTotalVertexCount(),
		tolerance,
		importNormals
	);

	auto dstMesh = RecreateMesh(srcMesh, clusters, importNormals);


	SDL_LOG(
		"Merge On Distance - Reduced from "
		<< srcMesh.GetTotalVertexCount()
		<< " to "
		<< dstMesh->GetTotalVertexCount()
		<< " vertices"
	);

	return dstMesh;
}

luxrays::ExtTriangleMeshUPtr
MergeOnDistanceShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}

}  // namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
