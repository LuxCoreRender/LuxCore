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

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>

#include <algorithm>
#include <boost/format.hpp>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <utility>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/pointiness.h"
#include "luxrays/utils/buffer.h"
#include "slg/scene/scene.h"

using namespace luxrays;
using namespace slg;

// Anonymous namespace: this file-local helper must have internal linkage.
// A global-scope class Edge also exists in edgedetectoraov.cpp and
// exttrianglemeshbevel.cpp with a different layout; as a global class the
// inline members and std::vector<Edge> instantiations have identical mangled
// names across TUs (ODR violation), and the linker can pick another TU's
// vector<Edge> code (different sizeof(Edge)) -> heap corruption.
namespace {

class Edge {
public:
	Edge() : v0(0), v1(0) { }
	// Normalize to (lo, hi) so an undirected edge has a single representation
	Edge(const u_int va, const u_int vb) : v0(std::min(va, vb)), v1(std::max(va, vb)) { }
	~Edge() { }

	// Lexicographic order: deterministic iteration regardless of insertion
	// order or allocation addresses (the old pointer-based fallback made the
	// FP accumulation loops below sum in a run-dependent order)
	bool operator<(const Edge &e) const {
		return v0 < e.v0 || (v0 == e.v0 && v1 < e.v1);
	}
	bool operator==(const Edge &e) const {
		return v0 == e.v0 && v1 == e.v1;
	}

	u_int v0, v1;
};

// Serializes the SDL_LOG emission of pointiness shapes running on TBB
// worker threads (the debug handler itself is not synchronized)
std::mutex &LogMutex() {
	static std::mutex m;
	return m;
}

}  // namespace

// Compare vertices by sum of their coordinates.
class VertexAverageComparator {
public:
	VertexAverageComparator(const Point *verts) : verts_(verts)
	{
	}

	bool operator()(const int &vert_idx_a, const int &vert_idx_b)
	{
		const Point &vert_a = verts_[vert_idx_a];
		const Point &vert_b = verts_[vert_idx_b];
		if (vert_a == vert_b) {
			// Special case for doubles, so we ensure ordering.
			return vert_idx_a > vert_idx_b;
		}
		const float x1 = vert_a.x + vert_a.y + vert_a.z;
		const float x2 = vert_b.x + vert_b.y + vert_b.z;
		return x1 < x2;
	}

protected:
	const Point *verts_;
};

PointinessShape::PointinessShape(ExtTriangleMeshRef srcMesh, const u_int destAOVIndex) {
	// The source mesh is expected to be already welded (BlendLuxCore welds
	// coincident vertices at export); use a mergeondistance shape first for
	// meshes with split vertices (e.g. raw PLY files)
	// Gather this shape's log lines and emit them with a single SDL_LOG
	// call at the end: the ctor runs on TBB worker threads (parallel
	// ParseShapes) and the debug handler is not synchronized, so per-line
	// calls from several shapes would interleave
	std::ostringstream log;
	log << "Pointiness shape " << srcMesh.GetName() << "\n";

	const double startTime = WallClockTime();

	const u_int originalVertCount = srcMesh.GetTotalVertexCount();
	const u_int triCount = srcMesh.GetTotalTriangleCount();

	const Points originalVertices = srcMesh.GetVertices();

	// Find duplicate vertices
	auto compareVerts = [](const TriangleMesh &mesh, const u_int vertIndex1, const u_int vertIndex2) {
		auto& triMesh = dynamic_cast<const ExtTriangleMesh&>(mesh);

		return (DistanceSquared(
					triMesh.GetVertex(Transform::TRANS_IDENTITY, vertIndex1),
					triMesh.GetVertex(Transform::TRANS_IDENTITY, vertIndex2)) < DEFAULT_EPSILON_STATIC) &&
				(triMesh.HasNormals() && Dot(
					triMesh.GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex1),
					triMesh.GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex2)));
	};
	std::vector<u_int> uniqueVertices;
	const u_int uniqueVertCount = srcMesh.GetUniqueVerticesMapping(uniqueVertices, compareVerts);
	log << "Pointiness shape has " << uniqueVertCount << " unique vertices over " << originalVertCount << "\n";

	const TriangleBuffer tris(srcMesh.GetTriangles());

	// Build the edge information: flat vector + parallel sort + unique.
	// Edges are normalized (lo, hi) and the (v0, v1) pairs are unique, so
	// both sorts below are total orders -> deterministic output regardless
	// of thread count.
	std::vector<Edge> edges((size_t)triCount * 3);
	tbb::parallel_for(tbb::blocked_range<u_int>(0, triCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i) {
				edges[i * 3]     = Edge(uniqueVertices[tris[i].v[0]], uniqueVertices[tris[i].v[1]]);
				edges[i * 3 + 1] = Edge(uniqueVertices[tris[i].v[1]], uniqueVertices[tris[i].v[2]]);
				edges[i * 3 + 2] = Edge(uniqueVertices[tris[i].v[2]], uniqueVertices[tris[i].v[0]]);
			}
		});
	tbb::parallel_sort(edges.begin(), edges.end());
	edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	const size_t edgeCount = edges.size();

	// Permutation of the edge indices sorted by (v1, v0, index): a total
	// order, so the v1-side accumulation below is deterministic too
	std::vector<u_int> byV1(edgeCount);
	std::iota(byV1.begin(), byV1.end(), 0u);
	tbb::parallel_sort(byV1.begin(), byV1.end(),
		[&](const u_int a, const u_int b) {
			const Edge &ea = edges[a], &eb = edges[b];
			if (ea.v1 != eb.v1)
				return ea.v1 < eb.v1;
			if (ea.v0 != eb.v0)
				return ea.v0 < eb.v0;
			return a < b;
		});

	// Per-edge normalized direction, computed once (pure function of the edge)
	std::vector<Vector> edgeDirs(edgeCount);
	tbb::parallel_for(tbb::blocked_range<size_t>(0, edgeCount),
		[&](const tbb::blocked_range<size_t> &r) {
			for (size_t i = r.begin(); i < r.end(); ++i)
				edgeDirs[i] = Normalize(originalVertices[edges[i].v0] - originalVertices[edges[i].v1]);
		});

	// Build the vertex information. In the v0-sorted edge array every vertex
	// owns a contiguous run, so each vertex can be accumulated independently
	// (race-free) and in a fixed order (deterministic): first the v0 side,
	// then the v1 side via the byV1 permutation.
	std::vector<Vector> vertexEdgeVecs(originalVertCount, Vector());
	std::vector<u_int> vertexCounters(originalVertCount, 0);
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int v = r.begin(); v < r.end(); ++v) {
				const auto lo = std::lower_bound(edges.begin(), edges.end(), v,
					[](const Edge &e, const u_int x) { return e.v0 < x; });
				const auto hi = std::upper_bound(edges.begin(), edges.end(), v,
					[](const u_int x, const Edge &e) { return x < e.v0; });
				Vector acc;
				u_int cnt = 0;
				for (auto it = lo; it != hi; ++it) {
					acc += edgeDirs[it - edges.begin()];
					++cnt;
				}
				vertexEdgeVecs[v] = acc;
				vertexCounters[v] = cnt;
			}
		});
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int v = r.begin(); v < r.end(); ++v) {
				const auto lo = std::lower_bound(byV1.begin(), byV1.end(), v,
					[&](const u_int k, const u_int x) { return edges[k].v1 < x; });
				const auto hi = std::upper_bound(byV1.begin(), byV1.end(), v,
					[&](const u_int x, const u_int k) { return x < edges[k].v1; });
				for (auto it = lo; it != hi; ++it) {
					vertexEdgeVecs[v] -= edgeDirs[*it];
					vertexCounters[v]++;
				}
			}
		});

	// Build the normal information
	std::vector<Normal> vertexNormal(originalVertCount);
	if (srcMesh.HasNormals()) {
		tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
			[&](const tbb::blocked_range<u_int> &r) {
				for (u_int i = r.begin(); i < r.end(); ++i)
					vertexNormal[i] = srcMesh.GetShadeNormal(Transform::TRANS_IDENTITY, i);
			});
	} else {
		// Accumulate the geometry normal of each triangle into its unique
		// vertices: sort (unique vertex, triangle index) pairs -- a total
		// order -- then accumulate per vertex over its contiguous run
		std::vector<std::pair<u_int, u_int>> triKeys((size_t)triCount * 3);
		tbb::parallel_for(tbb::blocked_range<u_int>(0, triCount),
			[&](const tbb::blocked_range<u_int> &r) {
				for (u_int i = r.begin(); i < r.end(); ++i) {
					triKeys[i * 3]     = {uniqueVertices[tris[i].v[0]], i};
					triKeys[i * 3 + 1] = {uniqueVertices[tris[i].v[1]], i};
					triKeys[i * 3 + 2] = {uniqueVertices[tris[i].v[2]], i};
				}
			});
		tbb::parallel_sort(triKeys.begin(), triKeys.end());
		tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
			[&](const tbb::blocked_range<u_int> &r) {
				for (u_int v = r.begin(); v < r.end(); ++v) {
					const auto lo = std::lower_bound(triKeys.begin(), triKeys.end(), v,
						[](const std::pair<u_int, u_int> &p, const u_int x) { return p.first < x; });
					const auto hi = std::upper_bound(triKeys.begin(), triKeys.end(), v,
						[](const u_int x, const std::pair<u_int, u_int> &p) { return x < p.first; });
					Normal acc;
					for (auto it = lo; it != hi; ++it)
						acc += srcMesh.GetGeometryNormal(Transform::TRANS_IDENTITY, it->second);
					// the serial loop only accumulates into representative
					// vertices; duplicates keep the zero-initialized normal
					vertexNormal[v] = (uniqueVertices[v] == v) ? acc : Normal();
				}
			});
		tbb::parallel_for(tbb::blocked_range<u_int>(0, uniqueVertCount),
			[&](const tbb::blocked_range<u_int> &r) {
				for (u_int i = r.begin(); i < r.end(); ++i) {
					// Ignore not unique vertices
					if (uniqueVertices[i] == i)
						vertexNormal[i] = Normalize(vertexNormal[i]);
				}
			});
	}

	// Build the curvature information
	std::vector<float> rawCurvature(originalVertCount);
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i) {
				if (vertexCounters[i] > 0)
					rawCurvature[i] = -Dot(vertexNormal[i], vertexEdgeVecs[i] / vertexCounters[i]);
				else
					rawCurvature[i] = 0.f;
			}
		});

	// Blur the curvature information (same two-pass, per-vertex scheme as
	// the vertex information above)
	auto curvature = std::make_unique<float[]>(originalVertCount);
	std::span<float> curvatureSpan(curvature.get(), originalVertCount);
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i)
				curvatureSpan[i] = 0.f;
		});
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i)
				vertexCounters[i] = 1;
		});
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int v = r.begin(); v < r.end(); ++v) {
				const auto lo = std::lower_bound(edges.begin(), edges.end(), v,
					[](const Edge &e, const u_int x) { return e.v0 < x; });
				const auto hi = std::upper_bound(edges.begin(), edges.end(), v,
					[](const u_int x, const Edge &e) { return x < e.v0; });
				for (auto it = lo; it != hi; ++it) {
					curvatureSpan[v] += rawCurvature[it->v1];
					vertexCounters[v]++;
				}
			}
		});
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int v = r.begin(); v < r.end(); ++v) {
				const auto lo = std::lower_bound(byV1.begin(), byV1.end(), v,
					[&](const u_int k, const u_int x) { return edges[k].v1 < x; });
				const auto hi = std::upper_bound(byV1.begin(), byV1.end(), v,
					[&](const u_int x, const u_int k) { return x < edges[k].v1; });
				for (auto it = lo; it != hi; ++it) {
					curvatureSpan[v] += rawCurvature[edges[*it].v0];
					vertexCounters[v]++;
				}
			}
		});

	// Normalize and factor also old vertex alphas. Serial order matters here:
	// a duplicate reads its representative's curvature scaled if the
	// representative has a lower index, unscaled if higher. Three
	// race-free passes reproduce that exactly.
	const bool hasAlphas = srcMesh.HasAlphas(0);
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i)
				if (uniqueVertices[i] != i && uniqueVertices[i] > i)
					curvatureSpan[i] = curvatureSpan[uniqueVertices[i]];
		});
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i)
				if (uniqueVertices[i] == i)
					curvatureSpan[i] *= (hasAlphas ? srcMesh.GetAlpha(i, 0) : 1.f) / vertexCounters[i];
		});
	tbb::parallel_for(tbb::blocked_range<u_int>(0, originalVertCount),
		[&](const tbb::blocked_range<u_int> &r) {
			for (u_int i = r.begin(); i < r.end(); ++i)
				if (uniqueVertices[i] != i && uniqueVertices[i] < i)
					curvatureSpan[i] = curvatureSpan[uniqueVertices[i]];
		});

	if (destAOVIndex == NULL_INDEX) {
		// Make a copy of the original mesh and overwrite vertex color information
		mesh = srcMesh.Copy(
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			curvatureSpan
		);
	} else {
		mesh = srcMesh.Copy();

		assert (destAOVIndex < EXTMESH_MAX_DATA_COUNT);
		mesh->SetVertexAOV(destAOVIndex, curvatureSpan);
	}

	const double endTime = WallClockTime();
	log << "Pointiness time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs";
	{
		std::lock_guard<std::mutex> lock(LogMutex());
		SDL_LOG(log.str());
	}
}

PointinessShape::~PointinessShape() {
}

ExtTriangleMeshUPtr PointinessShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
