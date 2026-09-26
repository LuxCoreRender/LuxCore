/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

// Only compile this file if the feature is enabled

#include <map>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cstring> // for memset
#include <functional>

#include <oneapi/tbb.h>

#include <boost/format.hpp>

#include "luxrays/core/trianglemesh.h"
#include "luxrays/usings.h"
#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/simplify2.h"
#include "luxrays/utils/buffer.h"
#include "slg/scene/scene.h"
#include "slg/utils/harlequincolors.h"
#include "slg/utils/group_by_equivalence.h"
#include "slg/cameras/camera.h"

using namespace luxrays;

namespace slg {
namespace simplify2 {

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
//
// The following code is based on Sven Forstmann's quadric mesh simplification
// code (https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification)
// and heavily modified for LuxCoreRender
//
// Papers at https://mgarland.org/research/quadrics.html

/////////////////////////////////////////////
//
// Mesh Simplification Tutorial
//
// (C) by Sven Forstmann in 2014
//
// License : MIT
// http://opensource.org/licenses/MIT
//
// https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
//
// 5/2016: Chris Rorden created minimal version for OSX/Linux/Windows compile

// SymetricMatrix for quadric error metrics
// The 4x4 symmetric matrix has 10 unique elements:
// [0] = m11, [1] = m12, [2] = m13, [3] = m14,
// [4] = m22, [5] = m23, [6] = m24,
// [7] = m33, [8] = m34,
// [9] = m44

class SymetricMatrix2 {
public:
	// Storage: 10 unique elements of symmetric 4x4 matrix
	float m[10];

	// Default constructor - initialize to zero
	SymetricMatrix2() {
		for (size_t i = 0; i < 10; ++i) {
			m[i] = 0.0f;
		}
	}

	// Constructor with scalar value
	explicit SymetricMatrix2(const float c) {
		for (size_t i = 0; i < 10; ++i) {
			m[i] = c;
		}
	}

	// Constructor with all 10 elements
	SymetricMatrix2(
			const float m11, const float m12, const float m13, const float m14,
			const float m22, const float m23, const float m24,
			const float m33, const float m34,
			const float m44) {
		m[0] = m11;
		m[1] = m12;
		m[2] = m13;
		m[3] = m14;
		m[4] = m22;
		m[5] = m23;
		m[6] = m24;
		m[7] = m33;
		m[8] = m34;
		m[9] = m44;
	}

	// Make plane from normal (a,b,c) and distance d: ax+by+cz+d=0
	// This creates the outer product matrix: [a;b;c;d] * [a b c d]
	// For a symmetric matrix, we only store the upper triangular part
	SymetricMatrix2(const float a, const float b, const float c, const float d) {
		// For the plane constructor, SIMD doesn't provide much benefit
		// due to the scattered access pattern. Use scalar operations.
		// This is typically called once per triangle during initialization,
		// not in the hot path.
		m[0] = a * a;
		m[1] = a * b;
		m[2] = a * c;
		m[3] = a * d;
		m[4] = b * b;
		m[5] = b * c;
		m[6] = b * d;
		m[7] = c * c;
		m[8] = c * d;
		m[9] = d * d;
	}

	// Accessor for element
	float operator[](int c) const {
		return m[c];
	}

	// Element accessor for non-const
	float& operator[](int c) {
		return m[c];
	}

	// Determinant of 3x3 submatrix
	// Note: This is for the full 4x4 matrix, but we use specific indices
	float det(
			const size_t a11, const size_t a12, const size_t a13,
			const size_t a21, const size_t a22, const size_t a23,
			const size_t a31, const size_t a32, const size_t a33) const {
		// For a 3x3 submatrix of the 4x4 matrix
		// Using scalar operations as SIMD doesn't help much here
		const float det = m[a11] * m[a22] * m[a33] + m[a13] * m[a21] * m[a32] + m[a12] * m[a23] * m[a31]
				- m[a13] * m[a22] * m[a31] - m[a11] * m[a23] * m[a32] - m[a12] * m[a21] * m[a33];
		return det;
	}

	// Addition (loop form so the 10 float additions are SIMD vectorized
	// by the compiler: 2x 128-bit + 1x 64-bit packed adds)
	const SymetricMatrix2 operator+(const SymetricMatrix2 &n) const {
		SymetricMatrix2 r;
		for (size_t i = 0; i < 10; ++i)
			r.m[i] = m[i] + n.m[i];
		return r;
	}

	// In-place addition (loop form for the same SIMD vectorization)
	SymetricMatrix2& operator+=(const SymetricMatrix2& n) {
		for (size_t i = 0; i < 10; ++i)
			m[i] += n.m[i];

		return *this;
	}
};


// Cache-aligned vector for the buffers of the multithreaded parts that
// persist through a whole phase and are accessed repeatedly: the storage
// starts on a cache line boundary, so the buffers of different threads
// never share a cache line.
template<typename T>
using CacheAlignedVector = std::vector<T, tbb::cache_aligned_allocator<T>>;

// Vector with the TBB scalable allocator (tbbmalloc), for the short-lived
// buffers of the multithreaded parts that are allocated and freed at a
// high rate with no caching expected (per closure, per chunk): they are
// served from the per-thread caches of tbbmalloc instead of the shared
// heap.
template<typename T>
using ScalableVector = std::vector<T, tbb::scalable_allocator<T>>;


class Simplify2 {
public:
	Simplify2(const ExtTriangleMesh &srcMesh) {
		const auto vertCount = srcMesh.GetTotalVertexCount();
		const auto triCount = srcMesh.GetTotalTriangleCount();
		const VertexBuffer verts(srcMesh.GetVertices());
		const TriangleBuffer tris(srcMesh.GetTriangles());

		vertices.resize(vertCount);
		for (size_t i = 0; i < vertCount; ++i)
			vertices[i].p = verts[i];

		if (srcMesh.HasNormals()) {
			const auto& norms = srcMesh.GetNormals();
			for (auto i = 0; i < vertCount; ++i)
				vertices[i].norm = norms[i];

			hasNormals = true;
		} else
			hasNormals = false;

		if (srcMesh.HasUVs(0)) {
			const auto uvs = srcMesh.GetUVs(0);
			for (size_t i = 0; i < vertCount; ++i)
				vertices[i].uv = uvs[i];

			hasUVs = true;
		} else
			hasUVs = false;

		if (srcMesh.HasColors(0)) {
			const auto cols = srcMesh.GetColors(0);
			for (size_t i = 0; i < vertCount; ++i)
				vertices[i].col = cols[i];

			hasColors = true;
		} else
			hasColors = false;

		if (srcMesh.HasAlphas(0)) {
			const auto alphas = srcMesh.GetAlphas(0);
			for (size_t i = 0; i < vertCount; ++i)
				vertices[i].alpha = alphas[i];

			hasAlphas = true;
		} else
			hasAlphas = false;

		triangles.resize(triCount);
		for (size_t i = 0; i < triCount; ++i) {
			triangles[i].v[0] = tris[i].v[0];
			triangles[i].v[1] = tris[i].v[1];
			triangles[i].v[2] = tris[i].v[2];
		}
	}

	~Simplify2() {
	}

	ExtTriangleMeshUPtr GetExtMesh() const {
		const size_t vertCount = vertices.size();
		const size_t triCount = triangles.size();

		VertexBuffer newVertices(vertCount);
		for (size_t i = 0; i < vertCount; ++i)
			newVertices[i] = vertices[i].p;

		NormalBuffer newNorms;
		if (hasNormals) {
			newNorms.Allocate(vertCount);
			for (auto i = 0; i < vertCount; ++i)
				newNorms[i] = vertices[i].norm;
		}

		ExtMeshProp<UV>::Layer newUVs = nullptr;
		if (hasUVs) {
			newUVs = std::make_shared<UV[]>(vertCount);
			for (size_t i = 0; i < vertCount; ++i)
				newUVs[i] = vertices[i].uv;
		}

		ExtMeshProp<Spectrum>::Layer newCols = nullptr;
		if (hasColors) {
			newCols = std::make_shared<Spectrum[]>(vertCount);
			for (size_t i = 0; i < vertCount; ++i)
				newCols[i] = vertices[i].col;
		}

		ExtMeshProp<float>::Layer newAlphas = nullptr;
		if (hasAlphas) {
			newAlphas = std::make_shared<float[]>(vertCount);
			for (size_t i = 0; i < vertCount; ++i)
				newAlphas[i] = vertices[i].alpha;
		}

		TriangleBuffer newTris(triCount);
		for (size_t i = 0; i < triCount; ++i) {
			assert (triangles[i].v[0] < vertCount);
			newTris[i].v[0] = triangles[i].v[0];

			assert (triangles[i].v[1] < vertCount);
			newTris[i].v[1] = triangles[i].v[1];

			assert (triangles[i].v[2] < vertCount);
			newTris[i].v[2] = triangles[i].v[2];
		}

		return std::make_unique<ExtTriangleMesh>(
			std::move(newVertices),
			std::move(newTris),
			std::move(newNorms),
			newUVs,
			newCols,
			newAlphas
		);
	}

	void Decimate(const float targetTriangleCount, CameraConstRef scnCamera,
			const float screenSize, const bool border) {
		// TODO: Implement new simplification algorithm here
		// This is where you would put your new implementation

		// For now, just log that we're using the new version
		SDL_LOG("SimplifyShape2: Using experimental simplification algorithm");

		// Call the original algorithm as fallback
		preserveBorder = border;
		camera = &scnCamera;
		edgeScreenSize = screenSize;

		// Work on N% of all triangles for each iteration (keep only N% lowest error candidates)
		// TODO: this should be a parameter (like target), tunable per shape
		const float candidatePercent = 0.3f; // 30%

		// Init
		for (size_t i = 0; i < triangles.size(); ++i)
			triangles[i].deleted = false;

		// Init the screen projection cache (used by UpdateTriangleError when
		// edgeScreenSize > 0)
		vertexScreenX.assign(vertices.size(), 0.f);
		vertexScreenY.assign(vertices.size(), 0.f);
		vertexScreenValid.assign(vertices.size(), false);
		vertexScreenVisible.assign(vertices.size(), false);

		// Main iteration loop
		const size_t startTriangleCount = triangles.size();
		deletedTriangles = 0;
		u_int totalDeletedTriangles = 0;
		for (size_t iteration = 0; iteration < 64; ++iteration) {
			if (startTriangleCount - totalDeletedTriangles <= targetTriangleCount)
				break;

			const double iterationStartTime = WallClockTime();
			double stepStartTime = iterationStartTime;

			// Compact the deleted triangles (iteration > 0), rebuild the vertex
			// references and clear the dirty flags (quadrics, edge errors and
			// border flags are initialized once, at iteration 0)
			UpdateMesh(iteration);
			SDL_LOG("Simplify2: Mesh " << (iteration == 0 ? "initialized" : "updated") << " in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Precompute the screen space projections of all the vertices (when
			// enabled): the parallel phases below then never lazily write the
			// caches. Closures can share (read only) vertices, so the lazy
			// cache writes would otherwise race between closures on the shared
			// entries (the vertex projections are deterministic, but the writes
			// must not happen concurrently anyway). The vertices moved by the
			// collapses still have their cache invalidated and lazily
			// recomputed, but only within a single closure (their triangles
			// all belong to the collapsing closure).
			if (edgeScreenSize > 0.f) {
				tbb::parallel_for(size_t(0), vertices.size(), [this](size_t i) {
					float x, y;
					GetScreenPosition(i, &x, &y);
				});
			}

			// Build the edge candidate list and keep only the N% lowest error candidates
			stepStartTime = WallClockTime();
			std::vector<SimplifyRef2> allCandidates;
			allCandidates.reserve(triangles.size());

			// Lambda to compare SimplifyRef2 by error
			auto refCompare = [this](const SimplifyRef2 &a, const SimplifyRef2 &b) {
				return triangles[a.tid].err[a.tvertex] < triangles[b.tid].err[b.tvertex];
			};

			// An empty collapse context: the candidate building only reads
			// the global baseline references (no tail)
			CollapseContext candidateCtx;

			// Evaluate the candidates in parallel: the loop is read-only
			// (CalculateCollapseError and Flipped are const) and each triangle
			// writes only its own slot
			std::vector<u_int> candidateVertexIndex(triangles.size(), NULL_INDEX);
			tbb::parallel_for(size_t(0), triangles.size(),
					[this, &candidateCtx, &candidateVertexIndex](size_t i) {
				const SimplifyTriangle2 &t = triangles[i];

				// Look for the (valid) triangle vertex with the minimum error
				u_int minErrorIndex = NULL_INDEX;
				float minError = std::numeric_limits<float>::infinity();
				for (size_t j = 0; j < 3; ++j) {
					const u_int i0 = t.v[j];
					const SimplifyVertex2 &v0 = vertices[i0];

					const u_int i1 = t.v[(j + 1) % 3];
					const SimplifyVertex2 &v1 = vertices[i1];

					// Border check
					if (preserveBorder) {
						if (v0.border && v1.border)
							continue;
					} else {
						if (v0.border != v1.border)
							continue;
					}

					// Compute vertex to collapse to
					Point p;
					CalculateCollapseError(i0, i1, &p);

					// Don't remove if flipped
					if (Flipped(p, i0, i1, candidateCtx))
						continue;
					if (Flipped(p, i1, i0, candidateCtx))
						continue;

					if (t.err[j] < minError) {
						minErrorIndex = j;
						minError = t.err[j];
					}
				}

				if (minErrorIndex != NULL_INDEX)
					candidateVertexIndex[i] = minErrorIndex;
			});

			// Collect all valid candidates
			for (size_t i = 0; i < triangles.size(); ++i) {
				if (candidateVertexIndex[i] != NULL_INDEX)
					allCandidates.push_back(SimplifyRef2{ u_int(i), candidateVertexIndex[i] });
			}
			SDL_LOG("Simplify2: Found " << allCandidates.size() << " edge candidates in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Keep only the N% lowest error candidates
			const size_t totalCandidateCount = allCandidates.size();
			const u_int nPercentCount = std::max(1u, Floor2UInt(totalCandidateCount * candidatePercent));
			if (allCandidates.size() > nPercentCount) {
				// Select the N% lowest error candidates: nth_element partitions
				// in average O(n) and only the kept prefix needs to be ordered
				// (instead of sorting all the candidates to throw most of them
				// away)
				std::nth_element(allCandidates.begin(), allCandidates.begin() + nPercentCount,
					allCandidates.end(), refCompare);
				allCandidates.resize(nPercentCount);
			}

			// Sort the kept candidates by error (ascending)
			std::sort(allCandidates.begin(), allCandidates.end(), refCompare);
			SDL_LOG("Simplify2: Kept the " << allCandidates.size() << " lowest error candidates ("
				<< (boost::format("%.1f") % (candidatePercent * 100.f)) << "% of " << totalCandidateCount << ")");

			// Copy to candidateList in reverse order (worst first) to match original behavior
			candidateList = allCandidates;
			std::reverse(candidateList.begin(), candidateList.end());

			// Compute candidate closures for parallel processing
			stepStartTime = WallClockTime();
			std::vector<std::vector<u_int>> candidateClosures = ComputeCandidateClosures(allCandidates);
			size_t maxClosureSize = 0;
			for (const auto& closure : candidateClosures)
				maxClosureSize = std::max(maxClosureSize, closure.size());
			SDL_LOG("Simplify2: Computed " << candidateClosures.size() << " closures (max size "
				<< maxClosureSize << ") in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Process closures in parallel using TBB parallel_reduce
			deletedTriangles = 0;
			stepStartTime = WallClockTime();
			ProcessClosuresParallel(candidateClosures, allCandidates);
			SDL_LOG("Simplify2: Processed " << candidateClosures.size() << " closures in parallel in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Note: the closures have disjoint triangle sets, so the global
			// triangle flags written by the collapses are race-free and need no
			// merge; only the deleted triangles counter is merged (and the
			// closure disjointness asserted) by applyResult.

			const u_int iterationDeletedTriangles = deletedTriangles;
			totalDeletedTriangles += iterationDeletedTriangles;
			SDL_LOG("Simplify2 iteration " << iteration << " (" << allCandidates.size() << " edge candidates, deleted "
				<< iterationDeletedTriangles << "/" << totalDeletedTriangles << " of " << startTriangleCount
				<< " triangles) in " << (boost::format("%.3f") % (WallClockTime() - iterationStartTime)) << "secs");
			if (iterationDeletedTriangles == 0)
				break;
		}

		// Clean up mesh
		const double compactStartTime = WallClockTime();
		CompactMesh();
		SDL_LOG("Simplify2: Mesh compacted in "
			<< (boost::format("%.3f") % (WallClockTime() - compactStartTime)) << "secs");
	}

private:
	struct SimplifyTriangle2 {
		u_int v[3];
		Normal geometryN;
		float err[3];
		bool deleted, dirty;
	};

	struct SimplifyVertex2 {
		Point p;
		Normal norm;
		UV uv;
		Spectrum col;
		float alpha;

		u_int tstart, tcount;
		SymetricMatrix2 q;

		bool border;
	};

	struct SimplifyRef2 {
		u_int tid, tvertex;
	};

	// Local working state for edge collapses.
	//
	// During the parallel processing of closures, each thread appends the new
	// references to its own tail (read through the global baseline, see
	// GetRef) and counts its own deleted triangles, so that CollapseEdge
	// never mutates the shared reference list. The reference list is rebuilt
	// from scratch by UpdateMesh at each iteration, so the tails are simply
	// dropped at the end of the parallel processing (no merge needed).
	struct CollapseContext {
		// Cache aligned: the tail grows inside the parallel processing of
		// the closures (one context per thread)
		CacheAlignedVector<SimplifyRef2> refsTail;
		u_int deletedCount = 0;
	};

	// Read a reference by logical index: the global baseline plus the tail
	// appended by the collapse context
	const SimplifyRef2 &GetRef(const CollapseContext &ctx, const size_t index) const {
		const size_t baseSize = refs.size();
		return (index < baseSize) ? refs[index] : ctx.refsTail[index - baseSize];
	}

	std::vector<SimplifyTriangle2> triangles;
	std::vector<SimplifyVertex2> vertices;
	std::vector<SimplifyRef2> refs;

	CameraConstPtr camera;
	float edgeScreenSize;

	std::vector<SimplifyRef2> candidateList;

	u_int deletedTriangles;
	bool hasNormals, hasUVs, hasColors, hasAlphas, preserveBorder;

	// Cached screen space projections of the vertices (normalized
	// coordinates), lazily computed and invalidated when a vertex moves.
	// Only used when edgeScreenSize > 0.
	//
	// The caches are precomputed at the beginning of each iteration, so the
	// parallel phases never lazily write the entries: the closures can share
	// (read only) vertices, and the lazy writes would otherwise race between
	// closures on the shared entries. The vertices moved by the collapses
	// still have their cache invalidated and lazily recomputed, but only
	// within a single closure (their triangles all belong to the collapsing
	// closure). The validity/visibility flags are one byte per vertex (not
	// bit packed): the bit read-modify-write of e.g. std::vector<bool>
	// would race between closures.
	std::vector<float> vertexScreenX;
	std::vector<float> vertexScreenY;
	std::vector<std::uint8_t> vertexScreenValid;    // the projection has been computed
	std::vector<std::uint8_t> vertexScreenVisible;  // and the vertex is visible

	bool CollapseEdge(const size_t trinagleIndex, const size_t startVertexIndex,
			CollapseContext &ctx, ScalableVector<bool> &deleted0, ScalableVector<bool> &deleted1) {
		SimplifyTriangle2 &t = triangles[trinagleIndex];

		if (t.deleted)
			return false;
		if (t.dirty)
			return false;

		const u_int i0 = t.v[startVertexIndex];
		SimplifyVertex2 &v0 = vertices[i0];

		const u_int i1 = t.v[(startVertexIndex + 1) % 3];
		SimplifyVertex2 &v1 = vertices[i1];

		// Border check
		if (v0.border != v1.border)
			return false;

		// Compute vertex to collapse to
		Point p;
		CalculateCollapseError(i0, i1, &p);

		// true/false if the triangles referencing the vertex are deleted
		deleted0.resize(v0.tcount);
		deleted1.resize(v1.tcount);

		// Don't remove if flipped
		if (Flipped(p, i0, i1, ctx, &deleted0))
			return false;
		if (Flipped(p, i1, i0, ctx, &deleted1))
			return false;

		// Save original vertex information
		const Point triPoint0 = vertices[t.v[0]].p;
		const Point triPoint1 = vertices[t.v[1]].p;
		const Point triPoint2 = vertices[t.v[2]].p;

		const Normal triNorm0 = vertices[t.v[0]].norm;
		const Normal triNorm1 = vertices[t.v[1]].norm;
		const Normal triNorm2 = vertices[t.v[2]].norm;

		const UV triUV0 = vertices[t.v[0]].uv;
		const UV triUV1 = vertices[t.v[1]].uv;
		const UV triUV2 = vertices[t.v[2]].uv;

		const Spectrum triCol0 = vertices[t.v[0]].col;
		const Spectrum triCol1 = vertices[t.v[1]].col;
		const Spectrum triCol2 = vertices[t.v[2]].col;

		const float triAlpha0 = vertices[t.v[0]].alpha;
		const float triAlpha1 = vertices[t.v[1]].alpha;
		const float triAlpha2 = vertices[t.v[2]].alpha;

		// Not flipped, so remove edge
		v0.p = p;
		// The vertex moved: invalidate its cached screen projection
		vertexScreenValid[i0] = false;
		v0.q = v1.q + v0.q;

		// Interpolate other vertex attributes
		float b1, b2;
		if (Triangle::GetBaryCoords(
				triPoint0,
				triPoint1,
				triPoint2,
				p, &b1, &b2)) {
			const float b0 = 1.f - b1 - b2;

			if (hasNormals)
				v0.norm = Normalize(b0 * triNorm0 + b1 * triNorm1 + b2 * triNorm2);
			if (hasUVs)
				v0.uv = b0 * triUV0 + b1 * triUV1 + b2 * triUV2;
			if (hasColors)
				v0.col = b0 * triCol0 + b1 * triCol1 + b2 * triCol2;
			if (hasAlphas)
				v0.alpha = b0 * triAlpha0 + b1 * triAlpha1 + b2 * triAlpha2;
		} else {
			// Must be a malformed triangle
			if (hasNormals)
				v0.norm = triNorm0;
			if (hasUVs)
				v0.uv = triUV0;
			if (hasColors)
				v0.col = triCol0;
			if (hasAlphas)
				v0.alpha = triAlpha0;
		}

		const size_t tstart = refs.size() + ctx.refsTail.size();

		UpdateTriangles(i0, v0, deleted0, ctx);
		UpdateTriangles(i0, v1, deleted1, ctx);

		const size_t tcount = (refs.size() + ctx.refsTail.size()) - tstart;

		// Append the new references to the local tail and repoint the vertex.
		// The tail is simply dropped at the end of the parallel processing: the
		// reference list is rebuilt from scratch by UpdateMesh at each
		// iteration, so nothing needs to be merged back.
		v0.tstart = tstart;
		v0.tcount = tcount;

		return true;
	}

	// Check if a triangle flips when this edge is removed
	bool Flipped(const Point &p, const size_t i0, const size_t i1,
			const CollapseContext &ctx,
			ScalableVector<bool> *deleted = nullptr) const {
		const SimplifyVertex2 &v0 = vertices[i0];

		for (size_t k = 0; k < v0.tcount; ++k) {
			const SimplifyRef2 &ref = GetRef(ctx, v0.tstart + k);
			const SimplifyTriangle2 &t = triangles[ref.tid];

			if (t.deleted)
				continue;

			const u_int s = ref.tvertex;
			const u_int id1 = t.v[(s + 1) % 3];
			const u_int id2 = t.v[(s + 2) % 3];

			// Delete ?
			if (id1 == i1 || id2 == i1) {
				if (deleted)
					(*deleted)[k] = true;
				continue;
			}

			// Check if the triangle is too narrow. Same test as
			// AbsDot(Normalize(d1), Normalize(d2)) > .999f, rewritten with
			// squared quantities to avoid the normalizations (i.e. the
			// square roots): |d1.d2| / (|d1| |d2|) > .999f
			const Vector d1 = vertices[id1].p - p;
			const Vector d2 = vertices[id2].p - p;
			const float d1DotD2 = Dot(d1, d2);
			if (d1DotD2 * d1DotD2 > .999f * .999f * Dot(d1, d1) * Dot(d2, d2))
				return true;

			// Check if the Normal is changing side. Same test as
			// Dot(Normalize(Cross(d1, d2)), t.geometryN) < .2f, rewritten
			// with squared quantities to avoid the square roots:
			// (cross . N) / |cross| < .2f. A zero cross product (degenerate
			// case) falls through like the NaN of the original test.
			const Vector cross(Cross(d1, d2));
			const float crossSq = Dot(cross, cross);
			const float crossDotN = Dot(Normal(cross), t.geometryN);
			if (crossSq > 0.f && (crossDotN <= 0.f ||
					crossDotN * crossDotN < .2f * .2f * crossSq))
				return true;

			if (deleted)
				(*deleted)[k] = false;
		}

		return false;
	}

	// Update triangle connections and edge error after a edge is collapsed
	void UpdateTriangles(const size_t i0, const SimplifyVertex2 &v,
			const ScalableVector<bool> &deleted, CollapseContext &ctx) {
		for (size_t k = 0; k < v.tcount; ++k) {
			const SimplifyRef2 &r = GetRef(ctx, v.tstart + k);
			SimplifyTriangle2 &t = triangles[r.tid];

			if (t.deleted)
				continue;

			if (deleted[k]) {
				t.deleted = true;
				ctx.deletedCount++;
				continue;
			}

			t.v[r.tvertex] = i0;
			t.dirty = true;
			UpdateTriangleError(t);

			ctx.refsTail.push_back(r);
		}
	}

	// Compact triangles, compute edge error and build reference list
	void UpdateMesh(const size_t iteration) {
		if (iteration > 0) {
			// Compact triangles
			int dst = 0;
			for (size_t i = 0; i < triangles.size(); ++i)
				if (!triangles[i].deleted)
					triangles[dst++] = triangles[i];

			triangles.resize(dst);
		}

		// Init Quadrics by Plane & Edge Errors
		//
		// Required at the beginning (iteration == 0)
		//
		if (iteration == 0) {
			for (size_t i = 0; i < vertices.size(); ++i)
				vertices[i].q = SymetricMatrix2(0.0);

			for (size_t i = 0; i < triangles.size(); ++i) {
				SimplifyTriangle2 &t = triangles[i];

				SimplifyVertex2 &v0 = vertices[t.v[0]];
				SimplifyVertex2 &v1 = vertices[t.v[1]];
				SimplifyVertex2 &v2 = vertices[t.v[2]];

				const Normal geometryN(Normalize(Cross(v1.p - v0.p, v2.p - v0.p)));
				t.geometryN = geometryN;

				// It doesn't matter what vertex I use here because the triangle
				// plane will pass for all 3
				const SymetricMatrix2 sm(geometryN.x, geometryN.y, geometryN.z,
						-Dot(Vector(geometryN), Vector(v0.p)));
				v0.q += sm;
				v1.q += sm;
				v2.q += sm;
			}

			for (size_t i = 0; i < triangles.size(); ++i) {
				// Calc Edge Error
				SimplifyTriangle2 &t = triangles[i];

				UpdateTriangleError(t);
			}
		}

		// Init Reference ID list
		for (size_t i = 0; i < vertices.size(); ++i) {
			vertices[i].tstart = 0;
			vertices[i].tcount = 0;
		}

		for (size_t i = 0; i < triangles.size(); ++i) {
			SimplifyTriangle2 &t = triangles[i];

			vertices[t.v[0]].tcount++;
			vertices[t.v[1]].tcount++;
			vertices[t.v[2]].tcount++;
		}

		size_t tstart = 0;
		for (size_t i = 0; i < vertices.size(); ++i) {
			SimplifyVertex2 &v = vertices[i];

			v.tstart = tstart;
			tstart += v.tcount;
			v.tcount = 0;
		}

		// Write References
		refs.resize(triangles.size() * 3);
		for (size_t i = 0; i < triangles.size(); ++i) {
			SimplifyTriangle2 &t = triangles[i];

			for (size_t j = 0; j < 3; ++j) {
				SimplifyVertex2 &v = vertices[t.v[j]];

				refs[v.tstart + v.tcount].tid = i;
				refs[v.tstart + v.tcount].tvertex = j;

				v.tcount++;
			}
		}

		// Identify boundary : vertices[].border=0,1
		//
		// Required at the beginning (iteration == 0)
		if (iteration == 0) {
			for (size_t i = 0; i < vertices.size(); ++i)
				vertices[i].border = false;

			std::vector<u_int> vcount, vids;
			for (size_t i = 0; i < vertices.size(); ++i) {
				SimplifyVertex2 &v = vertices[i];
				vcount.clear();
				vids.clear();

				for (size_t j = 0; j < v.tcount; ++j) {
					int k = refs[v.tstart + j].tid;
					SimplifyTriangle2 &t = triangles[k];

					for (size_t k = 0; k < 3; ++k) {
						size_t ofs = 0;
						u_int id = t.v[k];

						while (ofs < vcount.size()) {
							if (vids[ofs] == id)
								break;

							ofs++;
						}

						if (ofs == vcount.size()) {
							vcount.push_back(1);
							vids.push_back(id);
						} else
							vcount[ofs]++;
					}
				}

				for (size_t j = 0; j < vcount.size(); ++j) {
					if (vcount[j] == 1)
						vertices[vids[j]].border = true;
				}
			}
		}

		// Clear dirty flag
		for (size_t i = 0; i < triangles.size(); ++i)
			triangles[i].dirty = false;
	}  // UpdateMesh

	// Finally compact mesh before exiting
	void CompactMesh() {
		size_t dst = 0;

		for (size_t i = 0; i < vertices.size(); ++i)
			vertices[i].tcount = 0;

		for (size_t i = 0; i < triangles.size(); ++i) {
			if (!triangles[i].deleted) {
				const SimplifyTriangle2 &t = triangles[i];
				triangles[dst++] = t;

				vertices[t.v[0]].tcount = 1;
				vertices[t.v[1]].tcount = 1;
				vertices[t.v[2]].tcount = 1;
			}
		}
		triangles.resize(dst);

		dst = 0;
		for (size_t i = 0; i < vertices.size(); ++i) {
			if (vertices[i].tcount) {
				vertices[i].tstart = dst;
				vertices[dst].p = vertices[i].p;

				vertices[dst].norm = vertices[i].norm;
				vertices[dst].uv = vertices[i].uv;
				vertices[dst].col = vertices[i].col;
				vertices[dst].alpha = vertices[i].alpha;

				dst++;
			}
		}

		for (size_t i = 0; i < triangles.size(); ++i) {
			SimplifyTriangle2 &t = triangles[i];

			t.v[0] = vertices[t.v[0]].tstart;
			t.v[1] = vertices[t.v[1]].tstart;
			t.v[2] = vertices[t.v[2]].tstart;
		}
		vertices.resize(dst);
	}

	// Error between vertex and Quadric, evaluated for the 3 points at
	// once: the evaluations are independent and share the same quadric
	// coefficients, so they are processed in SIMD lanes (one point per
	// lane, the 4th lane is padding). Each lane evaluates the same
	// expression as the original scalar version.
	std::array<float, 3> VertexError(const SymetricMatrix2 &q,
			const Point &p1, const Point &p2, const Point &p3) const {
		// Pack the point coordinates by component, padded to the SIMD width
		alignas(16) float x[4] = { p1.x, p2.x, p3.x, 0.f };
		alignas(16) float y[4] = { p1.y, p2.y, p3.y, 0.f };
		alignas(16) float z[4] = { p1.z, p2.z, p3.z, 0.f };
		alignas(16) float e[4] = { 0.f, 0.f, 0.f, 0.f };

		for (size_t k = 0; k < 4; ++k) {
			const float xv = x[k], yv = y[k], zv = z[k];
			constexpr std::array<float, 10> constfactors {
				1.f, 2.f, 2.f, 2.f, 1.f, 2.f, 2.f, 1.f, 2.f, 1.f
			};
			const std::array<float, 10> coefs1 {
				xv, xv, xv, xv, yv, yv, yv, zv, zv, 1.0f
			};
			const std::array<float, 10> coefs2 {
				xv, yv, zv, 1.f, yv, zv, 1.f, zv, 1.f, 1.f
			};

			//e[k] =
						//q[0] * xv * xv
				//+ 2.f * q[1] * xv * yv
				//+ 2.f * q[2] * xv * zv
				//+ 2.f * q[3] * xv
				//+		q[4] * yv * yv
				//+ 2.f * q[5] * yv * zv
				//+ 2.f * q[6] * yv
				//+       q[7] * zv * zv
				//+ 2.f * q[8] * zv
				//+		q[9];
			for (size_t i = 0; i != 10; ++i) e[k] += constfactors[i] * q[i] * coefs1[i] * coefs2[i];
		}

		return { e[0], e[1], e[2] };
	}

	// Error for one edge
	float CalculateCollapseError(const size_t v1Index, const size_t v2Index,
			Point *pResult = nullptr) const {
		const SimplifyVertex2 &v1 = vertices[v1Index];
		const SimplifyVertex2 &v2 = vertices[v2Index];

		const SymetricMatrix2 q = v1.q + v2.q;

		// Compute interpolated vertex
		const Point &p1 = v1.p;
		const Point &p2 = v2.p;
		const Point p3 = (p1 + p2) / 2;

		// Error can be negative, I add 1 to have screenErrorScale can than
		// work as expected
		const std::array<float, 3> errors = VertexError(q, p1, p2, p3);
		const float error1 = errors[0] + 1.f;
		const float error2 = errors[1] + 1.f;
		const float error3 = errors[2] + 1.f;

		float error;
		if (preserveBorder && v1.border) {
			error = error1;
			if (pResult)
				*pResult = p1;
		} else if (preserveBorder && v2.border) {
			error = error2;
			if (pResult)
				*pResult = p2;
		} else {
			error = std::min(error1, std::min(error2, error3));

			if (pResult) {
				if (error1 == error)
					*pResult = p1;
				if (error2 == error)
					*pResult = p2;
				if (error3 == error)
					*pResult = p3;
			}
		}

		// Adding 1.0 because error have negative values
		return std::max(error + 1.f, 0.f);
	}

	// Get the screen space projection (normalized coordinates) of a vertex,
	// with lazy caching. Returns false if the vertex is not visible (the
	// result is cached anyway: the camera projection is expensive).
	bool GetScreenPosition(const size_t vertexIndex, float * const x, float * const y) {
		if (vertexScreenValid[vertexIndex]) {
			*x = vertexScreenX[vertexIndex];
			*y = vertexScreenY[vertexIndex];
			return vertexScreenVisible[vertexIndex];
		}

		float px, py;
		const bool visible = camera->GetSamplePosition(vertices[vertexIndex].p, &px, &py) &&
				IsValid(px) && IsValid(py);

		if (visible) {
			// Normalize
			px /= camera->filmWidth;
			py /= camera->filmHeight;
		}

		vertexScreenX[vertexIndex] = px;
		vertexScreenY[vertexIndex] = py;
		vertexScreenValid[vertexIndex] = true;
		vertexScreenVisible[vertexIndex] = visible;

		*x = px;
		*y = py;
		return visible;
	}

	// Update the collapse errors of a triangle: quadric error scaled by the
	// screen error scale (one cached camera projection per vertex instead
	// of two per edge)
	void UpdateTriangleError(SimplifyTriangle2 &t) {
		t.err[0] = CalculateCollapseError(t.v[0], t.v[1]);
		t.err[1] = CalculateCollapseError(t.v[1], t.v[2]);
		t.err[2] = CalculateCollapseError(t.v[2], t.v[0]);

		if (edgeScreenSize > 0.f) {
			const float notVisibleScale = .5f;

			float sx[3], sy[3];
			bool visible[3];
			for (size_t j = 0; j < 3; ++j) {
				visible[j] = GetScreenPosition(t.v[j], &sx[j], &sy[j]);
			}

			for (size_t j = 0; j < 3; ++j) {
				const size_t j1 = (j + 1) % 3;

				float scale;
				if (visible[j] && visible[j1]) {
					const float edge = sqrtf(Sqr(sx[j] - sx[j1]) + Sqr(sy[j] - sy[j1]));
					scale = (edge == 0.f) ? notVisibleScale :
							std::max(edge / edgeScreenSize, notVisibleScale);
				} else
					scale = notVisibleScale;

				t.err[j] *= scale;
			}
		}
	}

	// Computes candidate closures (connected components in the conflict graph)
	//
	// Two candidates conflict (i.e. their collapses are not independent) iff
	// a triangle references an endpoint of both edges: everything a collapse
	// touches (CollapseEdge) is the vertex record of its first endpoint and
	// the records of the triangles referencing either endpoint, so two
	// collapses with no such common triangle read and write disjoint data and
	// commute (either order gives the same result). This includes the shared
	// endpoint case (the triangles around the shared vertex are common).
	//
	// The conflict relation is expressed as a triangle -> candidates reverse
	// index (a CSR over the triangles, filled from the candidate endpoints
	// through the vertex references): all the candidates in the same bucket
	// pairwise conflict, so the (chained) buckets define the equivalence
	// relation given to GroupByEquivalence (which relies on a parallel
	// Union-Find).
	//
	// Note: the resulting closures have disjoint triangle sets, so they can
	// be processed in parallel, but they can still share vertices (read
	// only). The screen space caches are precomputed for that reason: the
	// lazy cache writes would otherwise race on the shared vertices.
	std::vector<std::vector<u_int>> ComputeCandidateClosures(const std::vector<SimplifyRef2>& candidates) {
		const size_t candidateCount = candidates.size();
		if (candidateCount == 0) {
			return {};
		}

		// Build the triangle -> candidates reverse index as a CSR structure
		// (bucket count, prefix sum, bucket fill on flat arrays): no hashing
		// and no per bucket allocation.
		// (Kept serial: the count/fill updates of a triangle would be shared
		// by all the candidates reading it, so the parallel version would
		// need contended atomics.)
		const size_t triangleCount = triangles.size();
		std::vector<u_int> bucketStart(triangleCount + 1, 0);
		for (size_t i = 0; i < candidateCount; ++i) {
			const SimplifyTriangle2& t = triangles[candidates[i].tid];
			for (const size_t v : { t.v[candidates[i].tvertex],
					t.v[(candidates[i].tvertex + 1) % 3] }) {
				for (size_t k = 0; k < vertices[v].tcount; ++k)
					++bucketStart[refs[vertices[v].tstart + k].tid + 1];
			}
		}
		for (size_t tid = 0; tid < triangleCount; ++tid) {
			bucketStart[tid + 1] += bucketStart[tid];
		}

		std::vector<u_int> bucketEntries(bucketStart[triangleCount]);
		{
			std::vector<u_int> bucketCursor(bucketStart.begin(), bucketStart.end() - 1);
			for (size_t i = 0; i < candidateCount; ++i) {
				const SimplifyTriangle2& t = triangles[candidates[i].tid];
				for (const size_t v : { t.v[candidates[i].tvertex],
						t.v[(candidates[i].tvertex + 1) % 3] }) {
					for (size_t k = 0; k < vertices[v].tcount; ++k)
						bucketEntries[bucketCursor[refs[vertices[v].tstart + k].tid]++] = i;
				}
			}
		}

		// Lazy relation generator: for candidates [r1, r2), look up each
		// triangle touching their edge in the CSR and chain i with the next
		// candidate in the bucket. Bucket entries are in ascending candidate
		// index order (filled by iterating candidates 0..N), so
		// std::lower_bound finds i's position. Every candidate emits its own
		// forward link, so the full chain (c0,c1), (c1,c2), ... is
		// reconstructed in parallel across threads.
		auto relationGenerator =
				[this, &candidates, &bucketStart, &bucketEntries]
				(size_t r1, size_t r2) -> ScalableVector<Relation> {
			// Scalable allocator: allocated per chunk, inside the parallel
			// evaluation of the generator, consumed once by the Union-Find
			// (no caching expected)
			ScalableVector<Relation> relations;
			for (size_t i = r1; i < r2; ++i) {
				const SimplifyTriangle2& t = triangles[candidates[i].tid];
				for (const size_t v : { t.v[candidates[i].tvertex],
						t.v[(candidates[i].tvertex + 1) % 3] }) {
					for (size_t k = 0; k < vertices[v].tcount; ++k) {
						const u_int tid = refs[vertices[v].tstart + k].tid;
						const u_int start = bucketStart[tid];
						const u_int end = bucketStart[tid + 1];
						if (end - start <= 1)
							continue;
						// Find i in the sorted bucket. The same candidate can
						// appear multiple times in a bucket (a triangle
						// referencing both endpoints of the edge, or a
						// degenerate triangle referencing an endpoint twice):
						// skip the duplicates of i when chaining.
						const auto it = std::lower_bound(
								bucketEntries.begin() + start,
								bucketEntries.begin() + end,
								static_cast<u_int>(i));
						// If i is found and not the last in the bucket, chain
						// with the next candidate
						if (it != bucketEntries.begin() + end && *it == i) {
							auto next = std::next(it);
							while (next != bucketEntries.begin() + end && *next == i)
								++next;
							if (next != bucketEntries.begin() + end)
								relations.emplace_back(*it, *next);
						}
					}
				}
			}
			return relations;
		};

		// Group the connected candidates with the parallel Union-Find.
		// The generator is evaluated in parallel by GroupByEquivalence:
		// relations are generated and united in the same parallel_reduce
		// pass, without materializing a relations vector.
		const Classes classes = GroupByEquivalence(candidateCount,
				RelationFunction(relationGenerator));

		// Convert the classes to closures (the GroupByEquivalence classes come
		// with their members in ascending order, i.e. ascending error: the
		// greedy processing order of the collapses)
		std::vector<std::vector<u_int>> closures;
		closures.reserve(classes.size());
		for (const auto& indices : classes)
			closures.push_back(std::vector<u_int>(indices.begin(), indices.end()));

		return closures;
	}

	// Class for processing closures in parallel using TBB parallel_reduce.
	//
	// The closures have disjoint triangle sets, so the triangle updates
	// performed by CollapseEdge (including the global deleted/dirty flags)
	// are race-free and need no merge. The closures can still share vertices,
	// but only read only (a collapse writes only the vertex record of its
	// first endpoint, whose triangles all belong to the collapsing closure;
	// the screen space caches are precomputed, so the lazy cache writes never
	// happen across closures). Each body only appends thread-local references
	// (CollapseContext, dropped at the end: the reference list is rebuilt at
	// each iteration) and counts its deleted triangles.
	class ParallelClosureProcessor {
		Simplify2& simplify;
		const std::vector<std::vector<u_int>>& closures;
		const std::vector<SimplifyRef2>& allCandidates;

		// Local state: appended refs tail and deleted triangles counter
		CollapseContext ctx;

		// Candidate triangles deleted by this body (for the disjointness check).
		// Cache aligned: one per thread, appended in the parallel processing
		CacheAlignedVector<u_int> deletedCandidates;

	public:
		// Constructor for the master thread
		ParallelClosureProcessor(Simplify2& s,
				const std::vector<std::vector<u_int>>& c,
				const std::vector<SimplifyRef2>& a)
			: simplify(s), closures(c), allCandidates(a) {
			ctx.deletedCount = s.deletedTriangles;
		}

		// Split constructor for TBB
		ParallelClosureProcessor(ParallelClosureProcessor& other, tbb::split)
			: simplify(other.simplify), closures(other.closures), allCandidates(other.allCandidates) {}

		// Process a range of closures
		void operator()(const tbb::blocked_range<size_t>& r) {
			for (size_t i = r.begin(); i < r.end(); ++i) {
				ProcessClosure(closures[i]);
			}
		}

		// Join (reduce step): merge the sibling's counter and disjointness data
		void join(ParallelClosureProcessor& other) {
			ctx.deletedCount += other.ctx.deletedCount;
			deletedCandidates.insert(deletedCandidates.end(),
					other.deletedCandidates.begin(), other.deletedCandidates.end());
		}

		// Check the closure disjointness and merge the deleted triangles
		// counter. The global triangle flags are already up to date: they are
		// written by the collapses themselves.
		void applyResult() {
			// Assertion: a candidate triangle cannot be deleted in two
			// different closure processes
			std::sort(deletedCandidates.begin(), deletedCandidates.end());
			for (size_t i = 1; i < deletedCandidates.size(); ++i) {
				if (deletedCandidates[i] == deletedCandidates[i - 1]) {
					SDL_LOG("ERROR: Triangle " << deletedCandidates[i] << " was deleted in multiple closures!");
					SDL_LOG("  This indicates a bug in closure computation - triangle sets overlap.");
					assert(false && "Triangle deleted in multiple closures - triangle sets overlap!");
				}
			}

			simplify.deletedTriangles = ctx.deletedCount;
		}

	private:
		// Process a single closure
		void ProcessClosure(const std::vector<u_int>& closureIndices) {
			// Scalable allocator: recreated for every closure (no caching
			// expected), resized for every candidate
			ScalableVector<bool> deleted0, deleted1;

			// Process each candidate in the closure in order
			for (size_t idx : closureIndices) {
				const SimplifyRef2& candidate = allCandidates[idx];

				// Skip if the triangle was already deleted (e.g. by an
				// earlier collapse in this closure)
				if (simplify.triangles[candidate.tid].deleted) {
					continue;
				}

				// Try to collapse this edge
				const bool success = simplify.CollapseEdge(candidate.tid, candidate.tvertex, ctx, deleted0, deleted1);

				if (success) {
					// Record the collapsed candidate for the disjointness check
					deletedCandidates.push_back(candidate.tid);
				}
			}
		}
	};

	// Process all closures using TBB parallel_reduce
	void ProcessClosuresParallel(const std::vector<std::vector<u_int>>& closures,
			const std::vector<SimplifyRef2>& allCandidates) {
		if (closures.empty()) {
			return;
		}

		// Process the largest closures first to limit the load imbalance: the
		// candidates of a closure interact, so a closure is processed serially
		// and the biggest ones must start as early as possible
		std::vector<std::vector<u_int>> sortedClosures(closures.begin(), closures.end());
		std::sort(sortedClosures.begin(), sortedClosures.end(),
				[](const std::vector<u_int>& a, const std::vector<u_int>& b) {
					return a.size() > b.size();
				});

		// Use parallel_reduce to process closures in parallel.
		//
		// The closures have disjoint triangle sets so the updates of triangles
		// performed by CollapseEdge (including the global deleted/dirty flags)
		// are race-free and need no merge. The closures can still share
		// vertices, but only read only (and the screen caches are
		// precomputed, see the vertexScreen* comment).
		ParallelClosureProcessor processor(*this, sortedClosures, allCandidates);

		// Grain size: process at least 1 closure per thread
		const size_t grain_size = std::max<size_t>(1, sortedClosures.size() / tbb::this_task_arena::max_concurrency());

		SDL_LOG("Simplify2: Processing " << sortedClosures.size() << " closures on "
			<< tbb::this_task_arena::max_concurrency() << " threads (grain size " << grain_size << ")");

		tbb::parallel_reduce(
			tbb::blocked_range<size_t>(0, sortedClosures.size(), grain_size),
			processor
		);

		// Check the closure disjointness and merge the deleted triangles counter
		processor.applyResult();
	}

};

} // namespace simplify2

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

} // namespace slg

namespace slg {

using namespace luxrays;
using namespace slg;

SimplifyShape2::SimplifyShape2(CameraConstPtr camera, ExtTriangleMeshRef srcMesh,
		const float target, const float edgeScreenSize, const bool preserveBorder) {
	SDL_LOG("SimplifyShape2: Creating simplified shape " << srcMesh.GetName() << " with target " << target);

	if ((edgeScreenSize > 0.f) && !camera)
		throw std::runtime_error("The scene.GetCamera() must be defined in order to enable simplify edgescreensize option");

	const double startTime = WallClockTime();

	const u_int targetCount = std::max(1u, Floor2UInt(srcMesh.GetTotalTriangleCount() * target));

	simplify2::Simplify2 simplify(srcMesh);
	simplify.Decimate(targetCount, *camera, edgeScreenSize, preserveBorder);
	mesh = simplify.GetExtMesh();

	SDL_LOG("SimplifyShape2: Simplified shape from " << srcMesh.GetTotalTriangleCount() << " to " << mesh->GetTotalTriangleCount() << " faces");

	const double endTime = WallClockTime();
	SDL_LOG("SimplifyShape2 time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

SimplifyShape2::~SimplifyShape2() {
}

ExtTriangleMeshUPtr SimplifyShape2::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}

// Feature flag function
bool IsSimplify2Enabled() {
	return true;
}

} // namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
