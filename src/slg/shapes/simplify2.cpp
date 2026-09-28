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
#include <bit>
#include <algorithm>
#include <cstring> // for memset
#include <cmath> // for sqrt, round
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


// Rotation of the triangle corner indices: the indices that follow and
// precede a corner, i.e. (i + 1) % 3 and (i + 2) % 3 for i in [0, 3),
// without the modulo arithmetic. The reference and candidate vertex
// indices are always in [0, 3) by construction (the references are
// created with tvertex in [0, 3) and the candidates carry the same
// values), so the lookups stay in bounds.
constexpr u_int TRI_NEXT[3] = { 1, 2, 0 };
constexpr u_int TRI_PREV[3] = { 2, 0, 1 };

// Target number of regions of the simplify2 boundary deferral (see
// Simplify2::DeferBoundaryCandidates): enough regions to balance the
// closures across dozens of threads, at the cost of a thin deferred seam
constexpr size_t regionTarget = 64;
// Minimum number of kept candidates for the simplify2 boundary deferral:
// below it, the end game is faster without the deferral machinery (and
// behaves exactly like the serial algorithm)
constexpr size_t minKeptCandidates = 1024;

class Simplify2 {
public:
	Simplify2(const ExtTriangleMesh &srcMesh) {
		const auto vertCount = srcMesh.GetTotalVertexCount();
		const auto triCount = srcMesh.GetTotalTriangleCount();
		const VertexBuffer verts(srcMesh.GetVertices());
		const TriangleBuffer tris(srcMesh.GetTriangles());

		ResizeVertices(vertCount);
		for (size_t i = 0; i < vertCount; ++i)
			vertexP[i] = verts[i];

		if (srcMesh.HasNormals()) {
			const auto& norms = srcMesh.GetNormals();
			for (auto i = 0; i < vertCount; ++i)
				vertexNorm[i] = norms[i];

			hasNormals = true;
		} else
			hasNormals = false;

		if (srcMesh.HasUVs(0)) {
			const auto uvs = srcMesh.GetUVs(0);
			for (size_t i = 0; i < vertCount; ++i)
				vertexUV[i] = uvs[i];

			hasUVs = true;
		} else
			hasUVs = false;

		if (srcMesh.HasColors(0)) {
			const auto cols = srcMesh.GetColors(0);
			for (size_t i = 0; i < vertCount; ++i)
				vertexCol[i] = cols[i];

			hasColors = true;
		} else
			hasColors = false;

		if (srcMesh.HasAlphas(0)) {
			const auto alphas = srcMesh.GetAlphas(0);
			for (size_t i = 0; i < vertCount; ++i)
				vertexAlpha[i] = alphas[i];

			hasAlphas = true;
		} else
			hasAlphas = false;

		ResizeTriangles(triCount);
		for (size_t i = 0; i < triCount; ++i) {
			triangleV[3*i+0] = tris[i].v[0];
			triangleV[3*i+1] = tris[i].v[1];
			triangleV[3*i+2] = tris[i].v[2];
		}
	}

	~Simplify2() {
	}

	ExtTriangleMeshUPtr GetExtMesh() const {
		const size_t vertCount = GetVertexCount();
		const size_t triCount = GetTriangleCount();

		VertexBuffer newVertices(vertCount);
		for (size_t i = 0; i < vertCount; ++i)
			newVertices[i] = vertexP[i];

		NormalBuffer newNorms;
		if (hasNormals) {
			newNorms.Allocate(vertCount);
			for (auto i = 0; i < vertCount; ++i)
				newNorms[i] = vertexNorm[i];
		}

		ExtMeshProp<UV>::Layer newUVs = nullptr;
		if (hasUVs) {
			newUVs = std::make_shared<UV[]>(vertCount);
			for (size_t i = 0; i < vertCount; ++i)
				newUVs[i] = vertexUV[i];
		}

		ExtMeshProp<Spectrum>::Layer newCols = nullptr;
		if (hasColors) {
			newCols = std::make_shared<Spectrum[]>(vertCount);
			for (size_t i = 0; i < vertCount; ++i)
				newCols[i] = vertexCol[i];
		}

		ExtMeshProp<float>::Layer newAlphas = nullptr;
		if (hasAlphas) {
			newAlphas = std::make_shared<float[]>(vertCount);
			for (size_t i = 0; i < vertCount; ++i)
				newAlphas[i] = vertexAlpha[i];
		}

		TriangleBuffer newTris(triCount);
		for (size_t i = 0; i < triCount; ++i) {
			assert (triangleV[3*i+0] < vertCount);
			newTris[i].v[0] = triangleV[3*i+0];

			assert (triangleV[3*i+1] < vertCount);
			newTris[i].v[1] = triangleV[3*i+1];

			assert (triangleV[3*i+2] < vertCount);
			newTris[i].v[2] = triangleV[3*i+2];
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
		const float candidatePercent = 0.4f; // 40%

		// Init
		for (size_t i = 0; i < GetTriangleCount(); ++i)
			triangleDeleted[i] = false;

		// Init the screen projection cache (used by UpdateTriangleError when
		// edgeScreenSize > 0)
		vertexScreenX.assign(GetVertexCount(), 0.f);
		vertexScreenY.assign(GetVertexCount(), 0.f);
		vertexScreenValid.assign(GetVertexCount(), false);
		vertexScreenVisible.assign(GetVertexCount(), false);

		// Main iteration loop
		const size_t startTriangleCount = GetTriangleCount();
		deletedTriangles = 0;
		u_int totalDeletedTriangles = 0;
		for (size_t iteration = 0; iteration < 64; ++iteration) {
			if (startTriangleCount - totalDeletedTriangles <= targetTriangleCount)
				break;

			const double iterationStartTime = WallClockTime();

			// Precompute the screen space projections of all the vertices (when
			// enabled): the parallel phases below then never lazily write the
			// caches. Closures can share (read only) vertices, so the lazy
			// cache writes would otherwise race between closures on the shared
			// entries (the vertex projections are deterministic, but the writes
			// must not happen concurrently anyway). The vertices moved by the
			// collapses still have their cache invalidated and lazily
			// recomputed, but only within a single closure (their triangles
			// all belong to the collapsing closure).
			//
			// It runs before the mesh update because the parallel edge error
			// initialization of UpdateMesh reads the projections: nothing
			// moves the vertices in between, so the values are the same
			if (edgeScreenSize > 0.f) {
				tbb::parallel_for(size_t(0), GetVertexCount(), [this](size_t i) {
					float x, y;
					GetScreenPosition(i, &x, &y);
				});
			}

			double stepStartTime = WallClockTime();

			// Compact the deleted triangles (iteration > 0), rebuild the vertex
			// references and clear the dirty flags (quadrics, edge errors and
			// border flags are initialized once, at iteration 0)
			UpdateMesh(iteration);
			SDL_LOG("Simplify2: Mesh " << (iteration == 0 ? "initialized" : "updated") << " in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Build the edge candidate list and keep only the N% lowest error candidates
			stepStartTime = WallClockTime();
			// Lambda to compare the candidates by sort key: a single u64
			// comparison, no error array access in the comparator and no
			// tie-break branches (see CandidateKey)
			const auto keyCompare = [](const CandidateKey &a, const CandidateKey &b) {
				return a.key < b.key;
			};

			// An empty collapse context: the candidate building only reads
			// the global baseline references (no tail)
			CollapseContext candidateCtx;

			// Evaluate the candidates in parallel: the loop is read-only
			// (CalculateCollapseError and Flipped are const) and each triangle
			// writes only its own slot
			ScalableVector<u_int> candidateVertexIndex(GetTriangleCount(), NULL_INDEX);
			tbb::parallel_for(size_t(0), GetTriangleCount(),
					[this, &candidateCtx, &candidateVertexIndex](size_t i) {
				// Look for the (valid) triangle vertex with the minimum error
				u_int minErrorIndex = NULL_INDEX;
				float minError = std::numeric_limits<float>::infinity();
				for (size_t j = 0; j < 3; ++j) {
					const u_int i0 = triangleV[3*i+j];

					const u_int i1 = triangleV[3*i + TRI_NEXT[j]];

					// Border check
					if (preserveBorder) {
						if (vertexBorder[i0] && vertexBorder[i1])
							continue;
					} else {
						if (vertexBorder[i0] != vertexBorder[i1])
							continue;
					}

					// A corner that can not improve the current minimum
					// can never be recorded: this is the exact negation of
					// the original update test (and not >=, so that a NaN
					// error still never updates, exactly like before) and
					// the point reconstruction and screening below have
					// no side effect, so skipping them for a corner that
					// can not win changes nothing
					if (!(triangleErr[j][i] < minError))
						continue;

					// Reconstruct the collapse point from the choice
					// recorded with the error by the last error update:
					// the update evaluated the same error on the same
					// vertex state (nothing moves the vertices between
					// the error updates and the candidate evaluation),
					// so this is exactly the point the error evaluation
					// would compute again
					//
					// The border edges are a special case: with
					// preserveBorder, an edge with a single border
					// endpoint (the border check above has rejected the
					// border-border pairs) always collapses to the
					// border endpoint, whatever the quadric says. The
					// recorded choice is not used for them: at the
					// initialization pass the border flags are not
					// computed yet, so the recorded choices of the
					// border edges come from the general minimum error
					// path
					Point p;
					if (preserveBorder && (vertexBorder[i0] != vertexBorder[i1]))
						p = vertexBorder[i0] ? vertexP[i0] : vertexP[i1];
					else {
						const unsigned int choice = (triangleErrChoice[i] >> (2*j)) & 3;
						if (choice == 2)
							p = (vertexP[i0] + vertexP[i1]) / 2;
						else
							p = (choice == 1) ? vertexP[i1] : vertexP[i0];
					}

					// Don't remove if flipped
					if (Flipped(p, i0, i1, candidateCtx))
						continue;
					if (Flipped(p, i1, i0, candidateCtx))
						continue;

					minErrorIndex = j;
					minError = triangleErr[j][i];
				}

				if (minErrorIndex != NULL_INDEX)
					candidateVertexIndex[i] = minErrorIndex;
			});

			// Collect all valid candidates with their sort key
			ScalableVector<CandidateKey> candidateKeys;
			candidateKeys.reserve(GetTriangleCount());
			for (size_t i = 0; i < GetTriangleCount(); ++i) {
				const u_int tvertex = candidateVertexIndex[i];
				if (tvertex == NULL_INDEX)
					continue;

				// Order-preserving transformation of the collapse error
				// to an unsigned integer: the IEEE-754 bit pattern is
				// monotonic for the non-negative floats and reversed for
				// the negative ones, so the sign bit is set for the former
				// and the whole word is flipped for the latter
				const std::uint32_t errorBits = std::bit_cast<std::uint32_t>(triangleErr[tvertex][i]);
				const std::uint32_t errorKey = (errorBits & 0x80000000u) ?
					~errorBits : (errorBits | 0x80000000u);

				candidateKeys.push_back(CandidateKey{
					(static_cast<std::uint64_t>(errorKey) << 32) | static_cast<std::uint64_t>(i),
					SimplifyRef2{ u_int(i), tvertex } });
			}
			SDL_LOG("Simplify2: Found " << candidateKeys.size() << " edge candidates in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Keep only the N% lowest error candidates
			const size_t totalCandidateCount = candidateKeys.size();
			const u_int nPercentCount = std::max(1u, Floor2UInt(totalCandidateCount * candidatePercent));
			if (candidateKeys.size() > nPercentCount) {
				// Select the N% lowest error candidates: nth_element partitions
				// in average O(n) and only the kept prefix needs to be ordered
				// (instead of sorting all the candidates to throw most of them
				// away)
				std::nth_element(candidateKeys.begin(), candidateKeys.begin() + nPercentCount,
					candidateKeys.end(), keyCompare);
				candidateKeys.resize(nPercentCount);
			}

			// Sort the kept candidates by error (ascending)
			tbb::parallel_sort(candidateKeys.begin(), candidateKeys.end(), keyCompare);

			// Extract the sorted references for the downstream phases:
			// the keys are only needed by the sort
			ScalableVector<SimplifyRef2> allCandidates;
			allCandidates.reserve(candidateKeys.size());
			for (const CandidateKey &candidateKey : candidateKeys)
				allCandidates.push_back(candidateKey.ref);

			// Release the sort keys (several hundreds of MB in the first
			// iterations)
			ScalableVector<CandidateKey>().swap(candidateKeys);

			SDL_LOG("Simplify2: Kept the " << allCandidates.size() << " lowest error candidates ("
				<< (boost::format("%.1f") % (candidatePercent * 100.f)) << "% of " << totalCandidateCount << ")");

			// Defer the region boundary candidates: the closures are then
			// confined to the regions (and no longer giant), while the
			// deferred candidates just come back with the next iteration
			stepStartTime = WallClockTime();
			const size_t deferredCandidates = DeferBoundaryCandidates(allCandidates);
			if (deferredCandidates > 0)
				SDL_LOG("Simplify2: Deferred " << deferredCandidates << " region boundary candidates ("
					<< allCandidates.size() << " kept) in "
					<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Copy to candidateList in reverse order (worst first) to match original behavior
			candidateList = allCandidates;
			std::reverse(candidateList.begin(), candidateList.end());

			// Compute candidate closures for parallel processing
			stepStartTime = WallClockTime();
			ScalableVector<ScalableVector<u_int>> candidateClosures = ComputeCandidateClosures(allCandidates);
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
			// The deferred region boundary candidates come back with the
			// next iteration: only stop when nothing is left to do at all
			if (iterationDeletedTriangles == 0 && deferredCandidates == 0)
				break;
		}

		// Clean up mesh
		const double compactStartTime = WallClockTime();
		CompactMesh();
		SDL_LOG("Simplify2: Mesh compacted in "
			<< (boost::format("%.3f") % (WallClockTime() - compactStartTime)) << "secs");
	}

private:

	struct SimplifyRef2 {
		u_int tid, tvertex;
	};

	// Sort key of a SimplifyRef2 candidate: the 32-bit order-preserving
	// transformation of the collapse error in the high word and the
	// triangle index in the low word. Every triangle contributes at most
	// one candidate, so the triangle index is unique and the key is a
	// strict total order over distinct candidates: the comparison is a
	// single u64 test (no error array access in the comparator, no
	// tie-break branches) and the sorted sequence is independent of the
	// sort algorithm and of the thread scheduling
	struct CandidateKey {
		std::uint64_t key;
		SimplifyRef2 ref;
	};

	// Local working state for edge collapses.
	//
	// During the parallel processing of closures, each thread appends the new
	// references to its own tail (read through the global baseline, see
	// the segment loops) and counts its own deleted triangles, so that CollapseEdge
	// never mutates the shared reference list. The reference list is rebuilt
	// from scratch by UpdateMesh at each iteration, so the tails are simply
	// dropped at the end of the parallel processing (no merge needed).
	struct CollapseContext {
		// Cache aligned: the tail grows inside the parallel processing of
		// the closures (one context per thread)
		CacheAlignedVector<SimplifyRef2> refsTail;
		u_int deletedCount = 0;
	};

	// The triangle fields in homogeneous vectors (structure of arrays):
	// the passes use different fields (the closure passes only read the
	// vertex indices, the flag scans only read the flags, the sort
	// comparator only reads the errors), so each pass streams only what
	// it uses instead of the whole interleaved record
	// The triangle vertex indices in one flat array, three consecutive
	// entries per triangle (v(j, tid) = triangleV[3 * tid + j]): every
	// consumer reads two or three corners of a triangle, so the
	// interleaved chunk keeps them in the same cache line, and the flat
	// array keeps the data pointer in a register through the hot loops
	// (the previous array of three vectors reloaded the member pointer
	// and re-derived the byte offset for every reference)
	ScalableVector<u_int> triangleV;
	ScalableVector<Normal> triangleGeometryN;
	ScalableVector<float> triangleErr[3];
	// The collapse point choice of each edge error (0, 1 or 2: the two
	// endpoints or their midpoint), 2 bits per corner, packed in one byte
	// per triangle. The choice is recorded by UpdateTriangleError together
	// with the error, so that the candidate evaluation can reconstruct the
	// collapse point without re-evaluating the quadric error
	ScalableVector<unsigned char> triangleErrChoice;
	// The triangle flags as one byte per flag (and not std::vector<bool>):
	// the collapses of the parallel closure processing write the flags of
	// their (disjoint) triangles from multiple threads, and the packed
	// bits of a vector<bool> would share bytes between triangles: the
	// read-modify-write of the bit updates would race and lose updates.
	// One byte per flag keeps every write on its own address.
	ScalableVector<unsigned char> triangleDeleted;
	ScalableVector<unsigned char> triangleDirty;

	size_t GetTriangleCount() const { return triangleV.size() / 3; }

	void ResizeTriangles(const size_t count) {
		triangleV.resize(count * 3);
		for (size_t j = 0; j < 3; ++j)
			triangleErr[j].resize(count);
		triangleErrChoice.resize(count);
		triangleGeometryN.resize(count);
		triangleDeleted.resize(count);
		triangleDirty.resize(count);
	}

	// The vertex fields in homogeneous vectors (structure of arrays),
	// like the triangle fields: the hot passes read different fields
	// (Flipped and the error evaluation read the positions and the
	// quadrics, the reference walks read tstart/tcount, the candidate
	// evaluation reads the border flags), so each pass streams only
	// what it uses instead of the whole interleaved record
	ScalableVector<Point> vertexP;
	ScalableVector<Normal> vertexNorm;
	ScalableVector<UV> vertexUV;
	ScalableVector<Spectrum> vertexCol;
	ScalableVector<float> vertexAlpha;
	// One byte per flag (same rationale as the triangle flags)
	ScalableVector<unsigned char> vertexBorder;
	ScalableVector<u_int> vertexTstart;
	ScalableVector<u_int> vertexTcount;
	ScalableVector<SymetricMatrix2> vertexQ;

	size_t GetVertexCount() const { return vertexP.size(); }

	void ResizeVertices(const size_t count) {
		vertexP.resize(count);
		vertexNorm.resize(count);
		vertexUV.resize(count);
		vertexCol.resize(count);
		vertexAlpha.resize(count);
		vertexBorder.resize(count);
		vertexTstart.resize(count);
		vertexTcount.resize(count);
		vertexQ.resize(count);
	}
	// The vertex -> triangle references in homogeneous vectors (structure
	// of arrays): the closure CSR passes (the count, the fill and the
	// relation generator) and the border identification read only the
	// triangle indices, so they stream the dedicated u_int array instead
	// of fetching interleaved records to read half of them. The corner
	// index only holds values in [0, 3), so one byte is enough. Only
	// Flipped and UpdateTriangles need both fields (in their segment loops).
	ScalableVector<u_int> refTid;
	ScalableVector<unsigned char> refTvertex;

	CameraConstPtr camera;
	float edgeScreenSize;

	ScalableVector<SimplifyRef2> candidateList;

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
	ScalableVector<float> vertexScreenX;
	ScalableVector<float> vertexScreenY;
	ScalableVector<std::uint8_t> vertexScreenValid;    // the projection has been computed
	ScalableVector<std::uint8_t> vertexScreenVisible;  // and the vertex is visible

	bool CollapseEdge(const size_t trinagleIndex, const size_t startVertexIndex,
			CollapseContext &ctx, ScalableVector<bool> &deleted0, ScalableVector<bool> &deleted1) {
		if (triangleDeleted[trinagleIndex])
			return false;
		if (triangleDirty[trinagleIndex])
			return false;

		// The triangle corner vertex indices, used all along the function
		const u_int triVertices[3] = {
			triangleV[3*trinagleIndex+0],
			triangleV[3*trinagleIndex+1],
			triangleV[3*trinagleIndex+2]
		};

		const u_int i0 = triVertices[startVertexIndex];

		const u_int i1 = triVertices[TRI_NEXT[startVertexIndex]];

		// Border check
		if (vertexBorder[i0] != vertexBorder[i1])
			return false;

		// Compute vertex to collapse to
		Point p;
		CalculateCollapseError(i0, i1, &p);

		// true/false if the triangles referencing the vertex are deleted
		deleted0.resize(vertexTcount[i0]);
		deleted1.resize(vertexTcount[i1]);

		// Don't remove if flipped
		if (Flipped(p, i0, i1, ctx, deleted0))
			return false;
		if (Flipped(p, i1, i0, ctx, deleted1))
			return false;

		// Save original vertex information
		const Point triPoint0 = vertexP[triVertices[0]];
		const Point triPoint1 = vertexP[triVertices[1]];
		const Point triPoint2 = vertexP[triVertices[2]];

		const Normal triNorm0 = vertexNorm[triVertices[0]];
		const Normal triNorm1 = vertexNorm[triVertices[1]];
		const Normal triNorm2 = vertexNorm[triVertices[2]];

		const UV triUV0 = vertexUV[triVertices[0]];
		const UV triUV1 = vertexUV[triVertices[1]];
		const UV triUV2 = vertexUV[triVertices[2]];

		const Spectrum triCol0 = vertexCol[triVertices[0]];
		const Spectrum triCol1 = vertexCol[triVertices[1]];
		const Spectrum triCol2 = vertexCol[triVertices[2]];

		const float triAlpha0 = vertexAlpha[triVertices[0]];
		const float triAlpha1 = vertexAlpha[triVertices[1]];
		const float triAlpha2 = vertexAlpha[triVertices[2]];

		// Not flipped, so remove edge
		vertexP[i0] = p;
		// The vertex moved: invalidate its cached screen projection
		vertexScreenValid[i0] = false;
		vertexQ[i0] += vertexQ[i1];

		// Interpolate other vertex attributes
		float b1, b2;
		if (Triangle::GetBaryCoords(
				triPoint0,
				triPoint1,
				triPoint2,
				p, &b1, &b2)) {
			const float b0 = 1.f - b1 - b2;

			if (hasNormals)
				vertexNorm[i0] = Normalize(b0 * triNorm0 + b1 * triNorm1 + b2 * triNorm2);
			if (hasUVs)
				vertexUV[i0] = b0 * triUV0 + b1 * triUV1 + b2 * triUV2;
			if (hasColors)
				vertexCol[i0] = b0 * triCol0 + b1 * triCol1 + b2 * triCol2;
			if (hasAlphas)
				vertexAlpha[i0] = b0 * triAlpha0 + b1 * triAlpha1 + b2 * triAlpha2;
		} else {
			// Must be a malformed triangle
			if (hasNormals)
				vertexNorm[i0] = triNorm0;
			if (hasUVs)
				vertexUV[i0] = triUV0;
			if (hasColors)
				vertexCol[i0] = triCol0;
			if (hasAlphas)
				vertexAlpha[i0] = triAlpha0;
		}

		const size_t tstart = refTid.size() + ctx.refsTail.size();

		UpdateTriangles(i0, i0, deleted0, ctx);
		UpdateTriangles(i0, i1, deleted1, ctx);

		const size_t tcount = (refTid.size() + ctx.refsTail.size()) - tstart;

		// Append the new references to the local tail and repoint the vertex.
		// The tail is simply dropped at the end of the parallel processing: the
		// reference list is rebuilt from scratch by UpdateMesh at each
		// iteration, so nothing needs to be merged back.
		vertexTstart[i0] = u_int(tstart);
		vertexTcount[i0] = u_int(tcount);

		return true;
	}

	// Check if a triangle flips when this edge is removed
	// Check if a triangle flips when this edge is removed.
	//
	// The update of the per reference deleted flags is a compile time
	// decision: the candidate evaluation calls Flipped without flags
	// (tens of millions of times per iteration) and the runtime null
	// pointer tests disappear from the generated loop instead of being
	// re-tested for every reference.
	template<bool recordDeleted>
	bool FlippedImpl(const Point &p, const size_t i0, const size_t i1,
			const CollapseContext &ctx,
			ScalableVector<bool> *deleted) const {
		const u_int tstart0 = vertexTstart[i0];
		const u_int tcount0 = vertexTcount[i0];

		// Process one reference: returns true when the triangle flips.
		// (A lambda so that the two segment loops below share the source
		// and the compiler inlines it in both)
		auto processReference = [&](const size_t k, const SimplifyRef2 &ref) -> bool {
			const size_t tid = ref.tid;

			if (triangleDeleted[tid])
				return false;

			const u_int s = ref.tvertex;
			const u_int id1 = triangleV[3*tid + TRI_NEXT[s]];
			const u_int id2 = triangleV[3*tid + TRI_PREV[s]];

			// Delete ?
			if (id1 == i1 || id2 == i1) {
				if constexpr (recordDeleted)
					(*deleted)[k] = true;
				return false;
			}

			// Check if the triangle is too narrow. Same test as
			// AbsDot(Normalize(d1), Normalize(d2)) > .999f, rewritten with
			// squared quantities to avoid the normalizations (i.e. the
			// square roots): |d1.d2| / (|d1| |d2|) > .999f. By Lagrange's
			// identity, |d1|^2 |d2|^2 = (d1.d2)^2 + |cross|^2, so the
			// test is derived from d1DotD2^2 and the cross product length
			// (computed for the normal side test below) without the two
			// extra dot products:
			// (d1.d2)^2 (1 - c) > c |cross|^2 with c = .999f^2.
			// (The rounding differs from the two dot products form, so
			// borderline triangles can be judged differently)
			const Vector d1 = vertexP[id1] - p;
			const Vector d2 = vertexP[id2] - p;
			const float d1DotD2 = Dot(d1, d2);
			const Vector cross(Cross(d1, d2));
			const float crossSq = Dot(cross, cross);
			constexpr float narrowCosSq = .999f * .999f;
			if (d1DotD2 * d1DotD2 * (1.f - narrowCosSq) > narrowCosSq * crossSq)
				return true;

			// Check if the Normal is changing side. Same test as
			// Dot(Normalize(Cross(d1, d2)), t.geometryN) < .2f, rewritten
			// with squared quantities to avoid the square roots:
			// (cross . N) / |cross| < .2f. A zero cross product (degenerate
			// case) falls through like the NaN of the original test.
			const float crossDotN = Dot(Normal(cross), triangleGeometryN[tid]);
			if (crossSq > 0.f && (crossDotN <= 0.f ||
					crossDotN * crossDotN < .2f * .2f * crossSq))
				return true;

			if constexpr (recordDeleted)
				(*deleted)[k] = false;

			return false;
		};

		// The references of the vertex are contiguous: the ones in the
		// global baseline and the ones appended to the collapse tail form
		// two segments, so the base/tail test is hoisted out
		// of the loops instead of being re-evaluated (with the context
		// reload it forces) for every reference. The candidate
		// evaluation has an empty tail: its whole loop reads the baseline
		// with no test at all
		const size_t baseSize = refTid.size();
		const size_t baseRefCount = (tstart0 < baseSize) ?
				std::min<size_t>(tcount0, baseSize - tstart0) : 0;

		for (size_t k = 0; k < baseRefCount; ++k) {
			if (processReference(k,
					SimplifyRef2{ refTid[tstart0 + k], refTvertex[tstart0 + k] }))
				return true;
		}
		for (size_t k = baseRefCount; k < tcount0; ++k) {
			if (processReference(k, ctx.refsTail[tstart0 + k - baseSize]))
				return true;
		}

		return false;
	}

	// Without the per reference deleted flags (candidate evaluation)
	bool Flipped(const Point &p, const size_t i0, const size_t i1,
			const CollapseContext &ctx) const {
		return FlippedImpl<false>(p, i0, i1, ctx, nullptr);
	}

	// With the per reference deleted flags (edge collapse): a reference,
	// non null by construction
	bool Flipped(const Point &p, const size_t i0, const size_t i1,
			const CollapseContext &ctx, ScalableVector<bool> &deleted) const {
		return FlippedImpl<true>(p, i0, i1, ctx, &deleted);
	}

	// Update triangle connections and edge error after a edge is collapsed
	void UpdateTriangles(const size_t i0, const size_t vertexIndex,
			const ScalableVector<bool> &deleted, CollapseContext &ctx) {
		const u_int tstart = vertexTstart[vertexIndex];
		const u_int tcount = vertexTcount[vertexIndex];

		// Process one reference (a lambda so that the two segment loops
		// below share the source and the compiler inlines it in both)
		auto processReference = [&](const size_t k, const SimplifyRef2 &r) {
			const size_t tid = r.tid;

			if (triangleDeleted[tid])
				return;

			if (deleted[k]) {
				triangleDeleted[tid] = true;
				ctx.deletedCount++;
				return;
			}

			triangleV[3*tid + r.tvertex] = u_int(i0);
			triangleDirty[tid] = true;
			UpdateTriangleError(tid);

			ctx.refsTail.push_back(r);
		};

		// The references of the vertex are contiguous: the ones in the
		// global baseline and the ones appended to the collapse tail form
		// two segments, so the base/tail test is hoisted out of
		// the loops (like in FlippedImpl) instead of being re-evaluated
		// for every reference
		const size_t baseSize = refTid.size();
		const size_t baseRefCount = (tstart < baseSize) ?
				std::min<size_t>(tcount, baseSize - tstart) : 0;

		for (size_t k = 0; k < baseRefCount; ++k)
			processReference(k,
					SimplifyRef2{ refTid[tstart + k], refTvertex[tstart + k] });
		for (size_t k = baseRefCount; k < tcount; ++k)
			processReference(k, ctx.refsTail[tstart + k - baseSize]);
	}

	// Compact triangles, compute edge error and build reference list
	void UpdateMesh(const size_t iteration) {
		if (iteration > 0) {
			// Compact the triangle arrays: the fields are moved one by
			// one (the in place slots are skipped)
			size_t dst = 0;
			for (size_t i = 0; i < GetTriangleCount(); ++i) {
				if (triangleDeleted[i])
					continue;

				if (dst != i) {
					for (size_t j = 0; j < 3; ++j) {
						triangleV[3*dst+j] = triangleV[3*i+j];
						triangleErr[j][dst] = triangleErr[j][i];
					}
					triangleErrChoice[dst] = triangleErrChoice[i];
					triangleGeometryN[dst] = triangleGeometryN[i];
					triangleDirty[dst] = triangleDirty[i];
					triangleDeleted[dst] = false;
				}

				++dst;
			}

			ResizeTriangles(dst);
		}

		// Init Quadrics by Plane & Edge Errors
		//
		// Required at the beginning (iteration == 0)
		//
		// The quadric accumulation stays serial: the triangle sweep
		// scatters over the shared vertices (a parallel pass would race),
		// and the vertex-centric alternative recomputes the plane
		// quadrics in a different context, which rounds the geometry
		// normals differently (FMA contraction is context dependent)
		// and changes the simplification decisions. The edge error pass
		// is where the time goes: it is parallel (the writes are
		// disjoint and the screen projections are precomputed before
		// the mesh update, a lazily computed projection would be written
		// by several threads at once)
		if (iteration == 0) {
			for (size_t i = 0; i < GetVertexCount(); ++i)
				vertexQ[i] = SymetricMatrix2(0.0);

			for (size_t i = 0; i < GetTriangleCount(); ++i) {
				const u_int iv0 = triangleV[3*i+0];
				const u_int iv1 = triangleV[3*i+1];
				const u_int iv2 = triangleV[3*i+2];

				const Point &p0 = vertexP[iv0];

				const Normal geometryN(Normalize(Cross(vertexP[iv1] - p0, vertexP[iv2] - p0)));
				triangleGeometryN[i] = geometryN;

				// It doesn't matter what vertex I use here because the triangle
				// plane will pass for all 3
				const SymetricMatrix2 sm(geometryN.x, geometryN.y, geometryN.z,
						-Dot(Vector(geometryN), Vector(p0)));
				vertexQ[iv0] += sm;
				vertexQ[iv1] += sm;
				vertexQ[iv2] += sm;
			}

			tbb::parallel_for(size_t(0), GetTriangleCount(), [&](size_t i) {
				UpdateTriangleError(i);
			});
		}

		// Build the vertex -> triangles reference list (a CSR over the
		// vertices). The reference counts, the prefix offsets and the
		// fill cursors are accumulated in compact arrays (a few MB,
		// resident in the caches): the random increments of the count
		// and fill passes would otherwise touch the vertex records at
		// every step and stream the whole vertex array through the last
		// level cache. The tstart/tcount fields are written back in the
		// sequential prefix pass, which also covers the unused
		// vertices (tcount 0), so no separate initialization is needed.
		const size_t vertexCount = GetVertexCount();
		ScalableVector<u_int> vertexRefCounts(vertexCount, 0);
		for (size_t i = 0; i < GetTriangleCount(); ++i) {
			++vertexRefCounts[triangleV[3*i+0]];
			++vertexRefCounts[triangleV[3*i+1]];
			++vertexRefCounts[triangleV[3*i+2]];
		}

		// Prefix sum of the reference counts and write back of the
		// vertex fields
		//
		// The scan is parallel: the final pass of tbb::parallel_scan
		// carries the true prefix, so the written starts are exactly
		// the ones of the sequential scan. The count and fill passes
		// stay serial on the contrary: they are locality bound (the
		// mesh order makes the per vertex accesses cache friendly) and
		// the parallel alternatives cost more than they save (a
		// partition of the vertices would re-scan the whole triangle
		// array from every range, and a bucketed scatter would fault
		// in hundreds of MB of temporary buffers)
		ScalableVector<u_int> vertexRefStarts(vertexCount);
		{
			class VertexRefStartScan {
				Simplify2 &mesh;
				ScalableVector<u_int> &refStarts;
				const ScalableVector<u_int> &refCounts;
				size_t tstart;

			public:
				VertexRefStartScan(Simplify2 &p_mesh,
						ScalableVector<u_int> &p_refStarts,
						const ScalableVector<u_int> &p_refCounts)
					: mesh(p_mesh), refStarts(p_refStarts),
					  refCounts(p_refCounts), tstart(0) { }
				VertexRefStartScan(VertexRefStartScan &other, tbb::split)
					: mesh(other.mesh), refStarts(other.refStarts),
					  refCounts(other.refCounts), tstart(0) { }

				void operator()(const tbb::blocked_range<size_t> &range, tbb::pre_scan_tag) {
					for (size_t i = range.begin(); i < range.end(); ++i)
						tstart += refCounts[i];
				}

				void operator()(const tbb::blocked_range<size_t> &range, tbb::final_scan_tag) {
					for (size_t i = range.begin(); i < range.end(); ++i) {
						refStarts[i] = u_int(tstart);
						mesh.vertexTstart[i] = u_int(tstart);
						mesh.vertexTcount[i] = refCounts[i];

						tstart += refCounts[i];
					}
				}

				void reverse_join(VertexRefStartScan &rhs) {
					tstart += rhs.tstart;
				}

				void assign(VertexRefStartScan &rhs) {
					tstart = rhs.tstart;
				}
			};

			VertexRefStartScan startScan(*this, vertexRefStarts, vertexRefCounts);
			tbb::parallel_scan(tbb::blocked_range<size_t>(0, vertexCount, 16384), startScan);
		}

		// Write the references with a compact per vertex cursor
		const size_t refCount = GetTriangleCount() * 3;
		refTid.resize(refCount);
		refTvertex.resize(refCount);
		{
			ScalableVector<u_int> vertexRefCursors(vertexRefStarts);
			for (size_t i = 0; i < GetTriangleCount(); ++i) {
				for (size_t j = 0; j < 3; ++j) {
					const u_int slot = vertexRefCursors[triangleV[3*i+j]]++;

					refTid[slot] = u_int(i);
					refTvertex[slot] = u_int(j);
				}
			}
		}

		// Identify boundary : vertices[].border=0,1
		//
		// Required at the beginning (iteration == 0)
		//
		// Parallel per vertex: the identification reads only the star of
		// the vertex and stores true in the border flag of its link
		// vertices, a one byte idempotent write that loses no update
		// when several threads store it at once. The zeroing is a
		// separate pass: it must be complete before any flag is set
		if (iteration == 0) {
			tbb::parallel_for(size_t(0), GetVertexCount(), [this](size_t i) {
				vertexBorder[i] = false;
			});

			tbb::parallel_for(tbb::blocked_range<size_t>(0, GetVertexCount(), 16384),
				[&](const tbb::blocked_range<size_t> &r) {
					// Reused across the vertices of the range: the clear
					// is just a size reset, no reallocation
					ScalableVector<u_int> vcount, vids;
					for (size_t i = r.begin(); i < r.end(); ++i) {
						vcount.clear();
						vids.clear();

						for (size_t j = 0; j < vertexTcount[i]; ++j) {
							const size_t tid = refTid[vertexTstart[i] + j];

							for (size_t k = 0; k < 3; ++k) {
								size_t ofs = 0;
								u_int id = triangleV[3*tid+k];

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
								vertexBorder[vids[j]] = true;
						}
					}
				});
		}

		// Clear dirty flag
		tbb::parallel_for(size_t(0), GetTriangleCount(), [&](size_t i) {
			triangleDirty[i] = false;
		});
	}  // UpdateMesh

	// Finally compact mesh before exiting
	void CompactMesh() {
		size_t dst = 0;

		for (size_t i = 0; i < GetVertexCount(); ++i)
			vertexTcount[i] = 0;

		// Compact the triangle arrays: the fields are moved one by one
		// (the in place slots are skipped) and the used vertices are
		// marked
		for (size_t i = 0; i < GetTriangleCount(); ++i) {
			if (triangleDeleted[i]) continue;

			if (dst != i) {
				for (size_t j = 0; j < 3; ++j) {
					triangleV[3*dst+j] = triangleV[3*i+j];
					triangleErr[j][dst] = triangleErr[j][i];
				}
				triangleErrChoice[dst] = triangleErrChoice[i];
				triangleGeometryN[dst] = triangleGeometryN[i];
				triangleDirty[dst] = triangleDirty[i];
				triangleDeleted[dst] = false;
			}

			vertexTcount[triangleV[3*i+0]] = 1;
			vertexTcount[triangleV[3*i+1]] = 1;
			vertexTcount[triangleV[3*i+2]] = 1;

			++dst;
		}
		ResizeTriangles(dst);

		// Compact the vertex arrays: only the output fields are moved
		// (like the record compaction: the quadrics and the flags stay
		// behind, they are not used anymore) and the new index of each
		// survivor is kept in its own tstart slot for the remap
		dst = 0;
		for (size_t i = 0; i < GetVertexCount(); ++i) {
			if (!vertexTcount[i]) continue;

			vertexTstart[i] = u_int(dst);

			if (dst != i) {
				vertexP[dst] = vertexP[i];
				vertexNorm[dst] = vertexNorm[i];
				vertexUV[dst] = vertexUV[i];
				vertexCol[dst] = vertexCol[i];
				vertexAlpha[dst] = vertexAlpha[i];
			}

			dst++;
		}

		// Remap the triangle vertex indices to the compacted vertices
		for (size_t i = 0; i < GetTriangleCount(); ++i) {
			triangleV[3*i+0] = vertexTstart[triangleV[3*i+0]];
			triangleV[3*i+1] = vertexTstart[triangleV[3*i+1]];
			triangleV[3*i+2] = vertexTstart[triangleV[3*i+2]];
		}
		ResizeVertices(dst);
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

	// Error for one edge. The optional results are the collapse point
	// (one of the two endpoints or their midpoint) and the index of that
	// point (0, 1 or 2), so that the point can later be reconstructed
	// from the positions without re-evaluating the quadric error
	float CalculateCollapseError(const size_t v1Index, const size_t v2Index,
			Point *pResult = nullptr, unsigned char *choiceResult = nullptr) const {
		const SymetricMatrix2 q = vertexQ[v1Index] + vertexQ[v2Index];

		// Compute interpolated vertex
		const Point &p1 = vertexP[v1Index];
		const Point &p2 = vertexP[v2Index];
		const Point p3 = (p1 + p2) / 2;

		// Error can be negative, I add 1 to have screenErrorScale can than
		// work as expected
		const std::array<float, 3> errors = VertexError(q, p1, p2, p3);
		const float error1 = errors[0] + 1.f;
		const float error2 = errors[1] + 1.f;
		const float error3 = errors[2] + 1.f;

		float error;
		unsigned char choice;
		if (preserveBorder && vertexBorder[v1Index]) {
			error = error1;
			choice = 0;
			if (pResult)
				*pResult = p1;
		} else if (preserveBorder && vertexBorder[v2Index]) {
			error = error2;
			choice = 1;
			if (pResult)
				*pResult = p2;
		} else {
			error = std::min(error1, std::min(error2, error3));

			// The same choice as the point selection below (the last of
			// the minimal errors wins)
			choice = (error3 == error) ? 2 : ((error2 == error) ? 1 : 0);

			if (pResult) {
				if (error1 == error)
					*pResult = p1;
				if (error2 == error)
					*pResult = p2;
				if (error3 == error)
					*pResult = p3;
			}
		}

		if (choiceResult)
			*choiceResult = choice;

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
		const bool visible = camera->GetSamplePosition(vertexP[vertexIndex], &px, &py) &&
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
	void UpdateTriangleError(const size_t tid) {
		unsigned char choice[3];
		triangleErr[0][tid] = CalculateCollapseError(triangleV[3*tid+0], triangleV[3*tid+1], nullptr, &choice[0]);
		triangleErr[1][tid] = CalculateCollapseError(triangleV[3*tid+1], triangleV[3*tid+2], nullptr, &choice[1]);
		triangleErr[2][tid] = CalculateCollapseError(triangleV[3*tid+2], triangleV[3*tid+0], nullptr, &choice[2]);

		// The choice of each edge travels with the error: the candidate
		// evaluation reconstructs the collapse point from it
		triangleErrChoice[tid] = static_cast<unsigned char>(choice[0] | (choice[1] << 2) | (choice[2] << 4));

		if (edgeScreenSize > 0.f) {
			const float notVisibleScale = .5f;

			float sx[3], sy[3];
			bool visible[3];
			for (size_t j = 0; j < 3; ++j) {
				visible[j] = GetScreenPosition(triangleV[3*tid+j], &sx[j], &sy[j]);
			}

			for (size_t j = 0; j < 3; ++j) {
				const size_t j1 = TRI_NEXT[j];

				float scale;
				if (visible[j] && visible[j1]) {
					const float edge = sqrtf(Sqr(sx[j] - sx[j1]) + Sqr(sy[j] - sy[j1]));
					scale = (edge == 0.f) ? notVisibleScale :
							std::max(edge / edgeScreenSize, notVisibleScale);
				} else
					scale = notVisibleScale;

				triangleErr[j][tid] *= scale;
			}
		}
	}

	// Defers the region boundary candidates, to break the giant closures.
	//
	// The closures are the connected components of the conflict graph and,
	// as soon as the candidates cover the mesh, the components percolate: a
	// single giant closure holds nearly all the candidates (measured: up to
	// 95%) and its mandatory serial processing pins the whole parallel
	// phase. Dropping candidates by error does not break it: every triangle
	// bucket is a clique, so the conflict graph keeps alternate paths
	// through every error band. Only a geometric cut disconnects it.
	//
	// The vertices are therefore partitioned into a grid of regions (over
	// the two largest bounding box extents) and a candidate is deferred iff
	// a triangle of its endpoint stars spans several regions. All the star
	// triangles of a kept candidate are then single region (otherwise it
	// would be deferred) and they all contain the first endpoint (the
	// candidate triangle contains both), so they all lie in the region of
	// its two endpoints: two kept candidates of different regions can never
	// share a bucket triangle, the closures computed on the kept candidates
	// are confined to the regions, and the disjointness invariant of the
	// parallel processing is preserved. The closures can still share
	// vertices (read only), like before.
	//
	// The deferred candidates are simply regenerated by the next iteration
	// (the candidate list is rebuilt from scratch every iteration), so the
	// deferral needs no bookkeeping at all. The measured cost is 1-3% of
	// the candidates per iteration. The deferral is applied after the
	// candidate percentile cut, so the deferred candidates consume their
	// share of the percentile budget.
	//
	// Deterministic: the regions are a pure function of the vertex
	// positions (the bounding box reduction is schedule independent). The
	// decisions differ from the non deferred version, though (the deferred
	// collapses happen one iteration later).
	//
	// Returns the number of deferred candidates (0 also when the deferral
	// is skipped) and compacts the candidates in place, keeping the error
	// order.
	size_t DeferBoundaryCandidates(ScalableVector<SimplifyRef2>& candidates) {
		const size_t candidateCount = candidates.size();
		if (candidateCount < minKeptCandidates)
			return 0;

		// Bounding box of the vertices
		struct BBox {
			float lo[3], hi[3];
			BBox() {
				for (size_t a = 0; a < 3; ++a) {
					lo[a] = std::numeric_limits<float>::infinity();
					hi[a] = -std::numeric_limits<float>::infinity();
				}
			}
		};
		const BBox bbox = tbb::parallel_reduce(
			tbb::blocked_range<size_t>(0, GetVertexCount()),
			BBox(),
			[this](const tbb::blocked_range<size_t>& r, BBox init) {
				for (size_t i = r.begin(); i != r.end(); ++i) {
					const Point& p = vertexP[i];
					init.lo[0] = std::min(init.lo[0], p.x);
					init.hi[0] = std::max(init.hi[0], p.x);
					init.lo[1] = std::min(init.lo[1], p.y);
					init.hi[1] = std::max(init.hi[1], p.y);
					init.lo[2] = std::min(init.lo[2], p.z);
					init.hi[2] = std::max(init.hi[2], p.z);
				}
				return init;
			},
			[](BBox x, const BBox& y) {
				for (size_t a = 0; a < 3; ++a) {
					x.lo[a] = std::min(x.lo[a], y.lo[a]);
					x.hi[a] = std::max(x.hi[a], y.hi[a]);
				}
				return x;
			});

		// Partition along the two largest extents (e.g. the plane of a
		// terrain like mesh, never its displacement axis)
		const float ext[3] = { bbox.hi[0] - bbox.lo[0], bbox.hi[1] - bbox.lo[1],
			bbox.hi[2] - bbox.lo[2] };
		size_t axis1 = 0;
		for (size_t a = 1; a < 3; ++a)
			if (ext[a] > ext[axis1])
				axis1 = a;
		size_t axis2 = (axis1 + 1) % 3;
		if (ext[(axis1 + 2) % 3] > ext[axis2])
			axis2 = (axis1 + 2) % 3;

		size_t grid1 = static_cast<size_t>(std::round(std::sqrt(
			static_cast<double>(regionTarget) * ext[axis1] / std::max(ext[axis2], 1e-30f))));
		grid1 = std::max<size_t>(1, std::min(grid1, regionTarget));
		const size_t grid2 = std::max<size_t>(1, regionTarget / grid1);

		const auto axisCoord = [](const Point& p, size_t axis) {
			return axis == 0 ? p.x : (axis == 1 ? p.y : p.z);
		};
		const float lo1 = bbox.lo[axis1], hi1 = bbox.hi[axis1];
		const float lo2 = bbox.lo[axis2], hi2 = bbox.hi[axis2];
		const float inv1 = grid1 > 1 ? static_cast<float>(grid1) / std::max(hi1 - lo1, 1e-30f) : 0.f;
		const float inv2 = grid2 > 1 ? static_cast<float>(grid2) / std::max(hi2 - lo2, 1e-30f) : 0.f;

		ScalableVector<u_int> regionOfVertex(GetVertexCount());
		tbb::parallel_for(size_t(0), GetVertexCount(), [&](size_t i) {
			const Point& p = vertexP[i];
			size_t i1 = static_cast<size_t>(std::max(0.f, (axisCoord(p, axis1) - lo1) * inv1));
			size_t i2 = static_cast<size_t>(std::max(0.f, (axisCoord(p, axis2) - lo2) * inv2));
			i1 = std::min(i1, grid1 - 1);
			i2 = std::min(i2, grid2 - 1);
			regionOfVertex[i] = static_cast<u_int>(i1 * grid2 + i2);
		});

		// The triangles spanning several regions are the seams cutting the
		// conflict components apart
		ScalableVector<unsigned char> mixedTri(GetTriangleCount());
		tbb::parallel_for(size_t(0), GetTriangleCount(), [&](size_t t) {
			const u_int r0 = regionOfVertex[triangleV[3 * t + 0]];
			const u_int r1 = regionOfVertex[triangleV[3 * t + 1]];
			const u_int r2 = regionOfVertex[triangleV[3 * t + 2]];
			mixedTri[t] = (r0 != r1) || (r1 != r2);
		});

		// A candidate is deferred iff a triangle of its endpoint stars is a
		// seam triangle
		ScalableVector<unsigned char> deferred(candidateCount, 0);
		tbb::parallel_for(size_t(0), candidateCount, [&](size_t i) {
			const size_t tid = candidates[i].tid;
			const size_t tvertex = candidates[i].tvertex;
			for (const u_int v : { triangleV[3 * tid + tvertex],
					triangleV[3 * tid + TRI_NEXT[tvertex]] }) {
				const size_t tstart = vertexTstart[v];
				const size_t tcount = vertexTcount[v];
				for (size_t k = 0; k < tcount; ++k) {
					if (mixedTri[refTid[tstart + k]]) {
						deferred[i] = 1;
						return;
					}
				}
			}
		});

		// Skip the deferral when it would leave too few candidates
		size_t keptCount = 0;
		for (size_t i = 0; i < candidateCount; ++i)
			keptCount += 1u - deferred[i];
		if (keptCount < minKeptCandidates)
			return 0;

		// Compact the candidates in place, keeping the error order
		size_t keptIndex = 0;
		for (size_t i = 0; i < candidateCount; ++i) {
			if (!deferred[i])
				candidates[keptIndex++] = candidates[i];
		}
		candidates.resize(keptCount);

		return candidateCount - keptCount;
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
	// The closures are computed through an equivalent relation over the
	// vertices, instead of the conflict relation itself (whose graph
	// percolates: chaining its edges directly costs hundreds of millions
	// of relations):
	//
	//   Two vertices are linked iff a triangle references both and both
	//   carry at least one candidate. The closure of a candidate is the
	//   candidate union of the connected component holding its endpoints.
	//
	//   (=>) If candidates i and j conflict, a triangle references an
	//   endpoint x of i and an endpoint y of j; both vertices carry a
	//   candidate (i and j themselves), so the triangle links x and y
	//   and i, j land in the same component.
	//
	//   (<=) If an endpoint x of i and an endpoint y of j are connected
	//   through linked vertices x = v0, ..., vk = y, every vi carries a
	//   candidate: one candidate per intermediate vertex builds a
	//   conflict chain from i to j (the link triangle of each hop
	//   references an endpoint of the candidates at both hop vertices),
	//   so i and j are also in the same closure of the conflict relation.
	//
	// The two relations therefore partition the candidates identically.
	// Degenerate cases are included: the two endpoints of a candidate
	// are linked by its own triangle, candidates sharing a vertex are
	// linked by any triangle around the shared vertex, and a degenerate
	// triangle just links its (possibly repeated) candidate bearing
	// corners.
	//
	// The links are generated per triangle (at most two per triangle:
	// the first candidate bearing corner linked with the other bearing
	// ones), over the candidate bearing vertices compacted into a dense
	// index, by the parallel Union-Find of GroupByEquivalence (the
	// ranges iterate the triangles, the elements are the compacted
	// vertices). The closures are gathered from the candidate lists of
	// the member vertices, conflict free (the components are disjoint),
	// sorted by candidate index (the greedy processing order) and
	// deduplicated (a candidate is listed once per endpoint vertex,
	// both in the same component).
	//
	// Note: the resulting closures have disjoint triangle sets, so they can
	// be processed in parallel, but they can still share vertices (read
	// only). The screen space caches are precomputed for that reason: the
	// lazy cache writes would otherwise race on the shared vertices.
	ScalableVector<ScalableVector<u_int>> ComputeCandidateClosures(const ScalableVector<SimplifyRef2>& candidates) {
		const size_t candidateCount = candidates.size();
		if (candidateCount == 0) {
			return {};
		}

		// Build the vertex -> candidates index as a CSR structure (count,
		// prefix sum, fill on flat arrays): the candidates are scattered
		// over the vertex lists of their two endpoints (2 entries per
		// candidate), in mesh order (through a tid -> candidate index
		// map, so the scatter stays cache coherent: the error order of
		// the candidates would make every access a cold miss). The scatter
		// is serial but small and coherent; its parallel versions would
		// need either atomics or per thread histograms (gigabytes at this
		// mesh size). The vertex lists drive the relation generator (the
		// candidate bearing test) and the closure gathering.
		const size_t triangleCount = GetTriangleCount();
		const size_t vertexCount = GetVertexCount();

		// tid -> candidate index map (a triangle holds at most one
		// candidate): filled in parallel without conflicts, the candidate
		// triangles are unique
		ScalableVector<u_int> candidateOfTid(triangleCount, NULL_INDEX);
		tbb::parallel_for(size_t(0), candidateCount, [&](size_t i) {
			candidateOfTid[candidates[i].tid] = static_cast<u_int>(i);
		});

		// Vertex -> candidates CSR: count, prefix sum, fill (all in mesh
		// order)
		ScalableVector<u_int> vertexStart(vertexCount + 1, 0);
		for (size_t t = 0; t < triangleCount; ++t) {
			const u_int i = candidateOfTid[t];
			if (i == NULL_INDEX)
				continue;
			const auto tvertex = candidates[i].tvertex;
			++vertexStart[triangleV[3*t + tvertex] + 1];
			++vertexStart[triangleV[3*t + TRI_NEXT[tvertex]] + 1];
		}
		for (size_t v = 0; v < vertexCount; ++v) {
			vertexStart[v + 1] += vertexStart[v];
		}

		ScalableVector<u_int> vertexEntries(vertexStart[vertexCount]);
		{
			ScalableVector<u_int> vertexCursor(vertexStart.begin(), vertexStart.end() - 1);
			for (size_t t = 0; t < triangleCount; ++t) {
				const u_int i = candidateOfTid[t];
				if (i == NULL_INDEX)
					continue;
				const auto tvertex = candidates[i].tvertex;
				vertexEntries[vertexCursor[triangleV[3*t + tvertex]]++] = i;
				vertexEntries[vertexCursor[triangleV[3*t + TRI_NEXT[tvertex]]]++] = i;
			}
		}

		// Candidate bearing vertices (a vertex carries a candidate iff its
		// list is not empty), compacted into a dense index: the elements of
		// the vertex relation above
		//
		// The compaction is the flag + prefix + scatter pattern, without
		// any atomic: the dense index of a flagged vertex is the count of
		// the flagged vertices before it, computed by tbb::parallel_scan
		// (its final pass carries the true prefix, so its writes
		// reproduce the serial running count and the dense indices are
		// assigned in ascending vertex order), and the inverse array is
		// filled by a disjoint scatter (a flagged vertex writes only its
		// own dense slot)
		ScalableVector<u_int> candVertexOfVertex(vertexCount, NULL_INDEX);

		// Vertex flags (one byte per vertex), computed in parallel
		ScalableVector<unsigned char> vertexHasCandidate(vertexCount);
		tbb::parallel_for(tbb::blocked_range<size_t>(0, vertexCount, 16384),
			[&](const tbb::blocked_range<size_t> &r) {
				for (size_t v = r.begin(); v < r.end(); ++v)
					vertexHasCandidate[v] = vertexStart[v + 1] > vertexStart[v];
			});

		// Dense index scan: the final pass writes the dense index of each
		// flagged vertex, the pre pass only accumulates the counts
		// (without any write)
		class VertexDenseIndexScan {
			ScalableVector<u_int> &denseIndexOfVertex;
			const ScalableVector<unsigned char> &hasCandidate;
			size_t count;

		public:
			VertexDenseIndexScan(ScalableVector<u_int> &p_denseIndexOfVertex,
					const ScalableVector<unsigned char> &p_hasCandidate)
				: denseIndexOfVertex(p_denseIndexOfVertex),
				  hasCandidate(p_hasCandidate),
				  count(0) { }
			VertexDenseIndexScan(VertexDenseIndexScan &other, tbb::split)
				: denseIndexOfVertex(other.denseIndexOfVertex),
				  hasCandidate(other.hasCandidate),
				  count(0) { }

			void operator()(const tbb::blocked_range<size_t> &range, tbb::pre_scan_tag) {
				for (size_t v = range.begin(); v < range.end(); ++v)
					count += hasCandidate[v];
			}

			void operator()(const tbb::blocked_range<size_t> &range, tbb::final_scan_tag) {
				for (size_t v = range.begin(); v < range.end(); ++v) {
					if (hasCandidate[v])
						denseIndexOfVertex[v] = static_cast<u_int>(count++);
				}
			}

			void reverse_join(VertexDenseIndexScan &rhs) {
				count += rhs.count;
			}

			void assign(VertexDenseIndexScan &rhs) {
				count = rhs.count;
			}

			size_t getCount() const {
				return count;
			}
		};

		VertexDenseIndexScan denseIndexScan(candVertexOfVertex, vertexHasCandidate);
		tbb::parallel_scan(tbb::blocked_range<size_t>(0, vertexCount, 16384), denseIndexScan);

		// Inverse mapping: disjoint scatter
		ScalableVector<u_int> vertexOfCandVertex(denseIndexScan.getCount());
		tbb::parallel_for(size_t(0), vertexCount, [&](size_t v) {
			const u_int denseIndexOfVertex = candVertexOfVertex[v];
			if (denseIndexOfVertex != NULL_INDEX)
				vertexOfCandVertex[denseIndexOfVertex] = static_cast<u_int>(v);
		});
		const size_t candVertexCount = vertexOfCandVertex.size();

		// Relation generator over the triangles [r1, r2): link the first
		// candidate bearing corner of each triangle with the other bearing
		// ones (at most two links; a repeated corner carries the same
		// compacted vertex and would link with itself, a no op skipped)
		auto relationGenerator =
			[this, &candVertexOfVertex]
				(size_t r1, size_t r2) -> ScalableVector<Relation> {
			// Scalable allocator: allocated per chunk, inside the parallel
			// evaluation of the generator, consumed once by the Union-Find
			// (no caching expected)
			ScalableVector<Relation> relations;
			for (size_t t = r1; t < r2; ++t) {
				u_int link = NULL_INDEX;
				for (size_t j = 0; j < 3; ++j) {
					const u_int cv = candVertexOfVertex[triangleV[3*t + j]];
					if (cv == NULL_INDEX)
						continue;
					if (link == NULL_INDEX)
						link = cv;
					else if (cv != link)
						relations.emplace_back(link, cv);
				}
			}
			return relations;
		};

		// Group the linked vertices with the parallel Union-Find. The
		// generator is evaluated in parallel by GroupByEquivalence:
		// relations are generated and united in the same parallel_reduce
		// pass, without materializing a full relations vector. The ranges
		// iterate the triangles, the elements are the candidate bearing
		// vertices.
		const Classes classes = GroupByEquivalence(candVertexCount, GetTriangleCount(),
				RelationFunction(relationGenerator));

		// Gather the closures from the components: the candidate lists of
		// the member vertices, sorted by candidate index (the greedy
		// processing order) and deduplicated (a candidate is listed once
		// per endpoint vertex, both in the same component). Conflict free
		// per class: the components are disjoint.
		ScalableVector<ScalableVector<u_int>> closures(classes.size());
		tbb::parallel_for(size_t(0), classes.size(), [&](size_t c) {
			const auto& component = classes[c];
			size_t closureSize = 0;
			for (const size_t cv : component) {
				const u_int v = vertexOfCandVertex[cv];
				closureSize += vertexStart[v + 1] - vertexStart[v];
			}
			auto& closure = closures[c];
			closure.reserve(closureSize);
			for (const size_t cv : component) {
				const u_int v = vertexOfCandVertex[cv];
				for (u_int p = vertexStart[v], end = vertexStart[v + 1]; p < end; ++p)
					closure.push_back(vertexEntries[p]);
			}
			std::sort(closure.begin(), closure.end());
			closure.erase(std::unique(closure.begin(), closure.end()), closure.end());
		});

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
		const ScalableVector<ScalableVector<u_int>>& closures;
		const ScalableVector<SimplifyRef2>& allCandidates;

		// Local state: appended refs tail and deleted triangles counter
		CollapseContext ctx;

		// Candidate triangles deleted by this body (for the disjointness check).
		// Cache aligned: one per thread, appended in the parallel processing
		CacheAlignedVector<u_int> deletedCandidates;

	public:
		// Constructor for the master thread
		ParallelClosureProcessor(Simplify2& s,
				const ScalableVector<ScalableVector<u_int>>& c,
				const ScalableVector<SimplifyRef2>& a)
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
			// different closure processes. Debug only: the check sorts
			// every deleted triangle, which would dominate the Release
			// processing phase (and the disjointness is guaranteed by the
			// closure computation, now also region confined)
#ifndef NDEBUG
			std::sort(deletedCandidates.begin(), deletedCandidates.end());
			for (size_t i = 1; i < deletedCandidates.size(); ++i) {
				if (deletedCandidates[i] == deletedCandidates[i - 1]) {
					SDL_LOG("ERROR: Triangle " << deletedCandidates[i] << " was deleted in multiple closures!");
					SDL_LOG("  This indicates a bug in closure computation - triangle sets overlap.");
					assert(false && "Triangle deleted in multiple closures - triangle sets overlap!");
				}
			}
#endif

			simplify.deletedTriangles = ctx.deletedCount;
		}

	private:
		// Process a single closure
		void ProcessClosure(const ScalableVector<u_int>& closureIndices) {
			// Scalable allocator: recreated for every closure (no caching
			// expected), resized for every candidate
			ScalableVector<bool> deleted0, deleted1;

			// Process each candidate in the closure in order
			for (size_t idx : closureIndices) {
				const SimplifyRef2& candidate = allCandidates[idx];
				auto tid = candidate.tid;

				// Skip if the triangle was already deleted (e.g. by an
				// earlier collapse in this closure)
				if (simplify.triangleDeleted[tid]) {
					continue;
				}

				// Try to collapse this edge
				const bool success = simplify.CollapseEdge(tid, candidate.tvertex, ctx, deleted0, deleted1);

				if (success) {
					// Record the collapsed candidate for the disjointness check
					deletedCandidates.push_back(tid);
				}
			}
		}
	};

	// Process all closures using TBB parallel_reduce
	// The closures are sorted in place, by ascending size: the reduce
	// descends the left spine of the range itself, so the small closures
	// must sit there (the big ones land in the stolen right halves, where
	// the thief threads split them among each other)
	void ProcessClosuresParallel(ScalableVector<ScalableVector<u_int>>& closures,
			const ScalableVector<SimplifyRef2>& allCandidates) {
		if (closures.empty()) {
			return;
		}

		tbb::parallel_sort(closures.begin(), closures.end(),
				[](const ScalableVector<u_int>& a, const ScalableVector<u_int>& b) {
					return a.size() < b.size();
				});

		// Use parallel_reduce to process closures in parallel.
		//
		// The closures have disjoint triangle sets so the updates of triangles
		// performed by CollapseEdge (including the global deleted/dirty flags)
		// are race-free and need no merge. The closures can still share
		// vertices, but only read only (and the screen caches are
		// precomputed, see the vertexScreen* comment).
		ParallelClosureProcessor processor(*this, closures, allCandidates);

		// Grain size 1: the closure sizes span several orders of magnitude, so
		// count based grains would pin the biggest closures to one thread
		const size_t grain_size = 1;

		SDL_LOG("Simplify2: Processing " << closures.size() << " closures on "
			<< tbb::this_task_arena::max_concurrency() << " threads (grain size " << grain_size << ")");

		tbb::parallel_reduce(
			tbb::blocked_range<size_t>(0, closures.size(), grain_size),
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
