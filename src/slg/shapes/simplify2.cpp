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

// NOTICE - the general principles of this implementation
//
// This is the multithreaded successor of the single threaded
// implementation (simplify.cpp), restructured around one hard
// constraint: it must take exactly the same decisions, with no lock
// and no atomic in the algorithm itself (the internals of TBB
// excepted), reproducible run after run.
//
// Each iteration evaluates the collapse error of the corners of every
// triangle, keeps the N% lowest error candidates, and collapses what
// remains; the iterations repeat until the target triangle count is
// reached. The principles every step follows:
//
// 1. Determinism comes from the structure, never from
// synchronization. The parallel phases write disjoint targets (every
// triangle fills only its own slot, every scatter bucket only its own
// range), the prefixes come from the final pass of
// tbb::parallel_scan (it carries the exact running value a serial
// scan would produce), and the candidate keys are unique (the error
// bits and the triangle index packed in one word), so the selected
// set is a well defined prefix whatever the sort algorithm or the
// thread scheduling is. A step that can not be structured this way
// stays serial.
//
// 2. One record, one writer. The flags are one byte per record and
// never bit packed (the read-modify-write of a shared byte would
// race between threads), the per thread scratch (histograms,
// reference tails) is written by its thread only and combined
// serially, and the few shared writes are idempotent (storing the
// same byte twice loses nothing). The per thread buffers are cache
// aligned (no false sharing), the short lived multithreaded buffers
// come from the TBB scalable allocator (no heap contention).
//
// 3. The closure is the unit of parallel work. The candidates whose
// collapses conflict form the connected components of a conflict
// graph, built by a lock free Union-Find over the relations
// (GroupByEquivalence). One closure is processed serially by one
// thread - nothing else touches its triangles, so no lock is needed
// - and the closures are processed in parallel. The giant closures
// are broken geometrically: the candidates whose star spans several
// regions are deferred, which disconnects the graph at the region
// seams (dropping by error can not break it, every error band keeps
// alternate paths through the triangle cliques).
//
// 4. The data follows the passes. The mesh fields live in
// homogeneous arrays (structure of arrays), one array per field, so
// every pass streams only what it uses; the fields always consumed
// together (the corner indices, the corner errors) are interleaved
// so they travel in one cache line and move in one block. Every
// index (the references, the dense ids) is built with the same CSR
// pattern: count, prefix, disjoint scatter.
//
// 5. Serial is a measured decision, not a default. The passes that
// stay serial (the in place compaction, the CSR count and fill, the
// candidate collect) are the ones where the parallel variants
// measured slower: they are locality bound, cluster friendly, and
// the alternatives pay more in redundant scans, scratch
// initialization or straggling than they save.
//
// 6. The float expressions are frozen in place. FMA contraction is
// context dependent: the same expression compiled in another context
// rounds differently, and one different rounding changes the
// decisions. The quadric accumulation stays serial for this reason,
// and every refactoring near the float paths is integer only or
// re-verified against the reference run.
//
// 7. Every change is verified against the reference run: the per
// iteration kept, deferred and closure counts and the final face
// count must match bit for bit, over several runs, before the change
// is kept.
//
// The main steps of the algorithm:
//
// 0. Initialization (once): the source mesh is copied into the
//    structure of arrays, the quadric of every vertex is accumulated
//    from the plane quadrics of its triangles (serial, principle 6),
//    the collapse errors of all triangle corners are evaluated
//    (parallel) and the border vertices are identified (parallel).
//
// 1. Mesh update (every iteration): the triangles deleted by the
//    previous iteration are compacted away (in place, strictly left
//    to right) and the vertex -> triangle reference list is rebuilt
//    with the CSR pattern (count, prefix, fill).
//
// 2. Candidate evaluation (parallel, one slot per triangle): every
//    triangle records the corner with the lowest collapse error,
//    screened by the border rules and the two flip tests. The error
//    and the collapse point are the ones the last error update
//    recorded on the same vertex state: the evaluation only reads.
//
// 3. Selection: every candidate is keyed by its error (an order
//    preserving transformation of the float bits) and its triangle
//    index, the N% lowest keys are selected with an MSB first radix
//    descent (the exact prefix std::nth_element would partition) and
//    sorted by ascending error.
//
// 4. Deferral: the candidates whose endpoint star spans several
//    regions of a grid over the bounding box are put aside for a
//    second wave of the same iteration. The cut is geometric
//    (principle 3): it disconnects the conflict graph at the region
//    seams, so the closures of the main wave stay small.
//
// 5. Closure computation: the conflict graph of the kept candidates
//    is partitioned into connected components by a lock free
//    Union-Find (GroupByEquivalence).
//
// 6. Collapse (parallel, one closure per task): a closure walks its
//    candidates in the sorted order; each collapse deletes the
//    triangles around its edge, moves the surviving vertices to the
//    recorded collapse point, updates the errors of the neighboring
//    triangles and appends their references to the per thread tail.
//    The closures own disjoint triangle sets: nothing is shared, no
//    lock is needed.
//
// 7. End game: the second wave processes the deferred seam candidates
//    right after the region confined closures of every iteration, so
//    nothing eligible is ever stranded (a re-deferred strip would
//    freeze at its initial density and stay visible along the region
//    grid). The loop stops when the target triangle count is reached,
//    when an iteration deletes nothing, or when the kept batch has
//    fallen below one ten thousandth of the first batch (the one whose
//    error rank defined the threshold E): the drain would then chase
//    an insignificant share of the mesh at the full cost of an
//    iteration. The halt is mesh dependent and can not fire at all
//    when the first batch is small (one ten thousandth of it is below
//    one candidate), so it only engages where the tail is expensive -
//    the large meshes. The mesh is then compacted one last time
//    (triangles, vertices and corner remap) and written back.

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
		const auto& srcVerts = verts.GetObjects();
		std::copy(srcVerts.begin(), srcVerts.end(), vertexP.begin());

		if (srcMesh.HasNormals()) {
			const auto& srcNorms = srcMesh.GetNormals().GetObjects();
			std::copy(srcNorms.begin(), srcNorms.end(), vertexNorm.begin());

			hasNormals = true;
		} else
			hasNormals = false;

		if (srcMesh.HasUVs(0)) {
			const auto uvs = srcMesh.GetUVs(0);
			std::copy_n(uvs.get(), vertCount, vertexUV.begin());

			hasUVs = true;
		} else
			hasUVs = false;

		if (srcMesh.HasColors(0)) {
			const auto cols = srcMesh.GetColors(0);
			std::copy_n(cols.get(), vertCount, vertexCol.begin());

			hasColors = true;
		} else
			hasColors = false;

		if (srcMesh.HasAlphas(0)) {
			const auto alphas = srcMesh.GetAlphas(0);
			std::copy_n(alphas.get(), vertCount, vertexAlpha.begin());

			hasAlphas = true;
		} else
			hasAlphas = false;

		ResizeTriangles(triCount);
		// The triangle corners are copied through the sub object view of
		// the buffer: a Triangle is its three corner indices, so the
		// flat unsigned int view is already the interleaved layout of
		// triangleV
		const auto& srcTris = tris.GetSubObjects();
		std::copy(srcTris.begin(), srcTris.end(), triangleV.begin());
	}

	~Simplify2() {
	}

	ExtTriangleMeshUPtr GetExtMesh() const {
		const size_t vertCount = GetVertexCount();
		const size_t triCount = GetTriangleCount();

		VertexBuffer newVertices(vertCount);
		std::copy(vertexP.begin(), vertexP.end(),
				newVertices.GetObjects().begin());

		NormalBuffer newNorms;
		if (hasNormals) {
			newNorms.Allocate(vertCount);
			std::copy(vertexNorm.begin(), vertexNorm.end(),
					newNorms.GetObjects().begin());
		}

		ExtMeshProp<UV>::Layer newUVs = nullptr;
		if (hasUVs) {
			newUVs = std::make_shared<UV[]>(vertCount);
			std::copy(vertexUV.begin(), vertexUV.end(), newUVs.get());
		}

		ExtMeshProp<Spectrum>::Layer newCols = nullptr;
		if (hasColors) {
			newCols = std::make_shared<Spectrum[]>(vertCount);
			std::copy(vertexCol.begin(), vertexCol.end(), newCols.get());
		}

		ExtMeshProp<float>::Layer newAlphas = nullptr;
		if (hasAlphas) {
			newAlphas = std::make_shared<float[]>(vertCount);
			std::copy(vertexAlpha.begin(), vertexAlpha.end(), newAlphas.get());
		}

		TriangleBuffer newTris(triCount);
		// The triangle corners are copied through the sub object view of
		// the buffer: a Triangle is its three corner indices, so the
		// flat unsigned int view is already the interleaved layout of
		// triangleV. The index check of the debug build covers all the
		// corners at once
		assert(std::all_of(triangleV.begin(), triangleV.end(),
				[&](const u_int v) { return v < vertCount; }));
		std::copy(triangleV.begin(), triangleV.end(),
				newTris.GetSubObjects().begin());

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

		// The selection is error driven: the first iteration keeps the
		// N% lowest error candidates and the error at that rank (E)
		// becomes the fixed threshold of the following ones - every
		// candidate below E is collapsed until none is left (or until
		// the target triangle count is reached, whichever comes
		// first). The flat regions cascade (a collapse makes its
		// neighbors cheaper), the detailed ones keep their triangles,
		// and the run ends with the homogeneous error property: no
		// collapse cheaper than E remains anywhere in the mesh
		const float initialCandidatePercent = 0.4f;
		float errorThreshold = 0.f;
		// The kept count of the first iteration: the reference of the
		// mesh dependent drain halt below
		size_t initialKeptCandidateCount = 0;

		// Init
		for (size_t i = 0; i < GetTriangleCount(); ++i)
			triangleDeleted[i] = false;

		// Init the screen projection cache (used by UpdateTriangleError when
		// edgeScreenSize > 0)
		vertexScreenX.assign(GetVertexCount(), 0.f);
		vertexScreenY.assign(GetVertexCount(), 0.f);
		vertexScreenValid.assign(GetVertexCount(), false);
		vertexScreenVisible.assign(GetVertexCount(), false);

		// The live triangle count of the run: the deferred compactions
		// leave the deleted triangles in the arrays (skipped by the
		// passes through their flags), so the array count still
		// carries them and the live estimate subtracts them
		const size_t startTriangleCount = GetTriangleCount() - uncompactedDeletions;
		deletedTriangles = 0;
		u_int totalDeletedTriangles = 0;
		// Main iteration loop. There is no iteration limit: the loop
		// ends when the target triangle count is reached, or when an
		// iteration deletes nothing (the fixed error threshold makes
		// the selection repeat itself from there), which includes the
		// complete drain with its homogeneous error certificate
		for (size_t iteration = 0;; ++iteration) {
			if (startTriangleCount - totalDeletedTriangles <= targetTriangleCount)
				break;

			const double iterationStartTime = WallClockTime();

			// A new generation for the star invalidation dedup: the
			// per vertex counters of the previous iterations are all
			// lower
			++invalidationGen;

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
			const bool deferredUpdate = UpdateMesh(iteration);
			SDL_LOG("Simplify2: Mesh " << (iteration == 0 ? "initialized" : "updated") << " in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs"
				<< (deferredUpdate ? " (deferred)" : ""));

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
			// writes only its own slot. The candidate and its validity are
			// the member records: they survive the iteration, cleared by the
			// collapses on every changed input (see UpdateTriangles) and
			// moved by the compaction with the triangle
			tbb::parallel_for(size_t(0), GetTriangleCount(),
					[this, &candidateCtx](size_t i) {
				// The recorded candidate is current: the last
				// evaluation of this triangle ran on the same inputs
				// (nothing touched the triangle or the stars its flip
				// tests read since), so it would select the same corner
				// again. Only reachable when the cache is active
				if (evalCacheActive && candidateValid[i])
					return;

				// A triangle deleted by a deferred compaction is not
				// evaluated: its record stays stale, the candidate
				// collection skips it through the same flag
				if (triangleDeleted[i])
					return;

				// Look for the (valid) triangle vertex with the minimum error
				u_int minErrorIndex = NULL_INDEX;
				float minError = std::numeric_limits<float>::infinity();
				const size_t triOffset = 3*i;
				for (size_t j = 0; j < 3; ++j) {
					const u_int i0 = triangleV[triOffset+j];

					const u_int i1 = triangleV[triOffset + TRI_NEXT[j]];

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
					if (!(triangleErr[triOffset + j] < minError))
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
					minError = triangleErr[triOffset + j];
				}

				// Record the outcome: the candidate of every
				// evaluation (the NULL_INDEX of a triangle without a
				// passing corner is an outcome like any other), and
				// the validity only when the cache runs - the records
				// of an inactive cache stay invalid, so the first
				// active evaluation recomputes the whole mesh once
				candidateVertexIndex[i] = minErrorIndex;
				if (evalCacheActive)
					candidateValid[i] = 1;
			});

			// Collect all valid candidates with their sort key
			ScalableVector<CandidateKey> candidateKeys;
			candidateKeys.reserve(GetTriangleCount());
			for (size_t i = 0; i < GetTriangleCount(); ++i) {
				// A triangle deleted by a deferred compaction carries
				// no candidate: its stale record is skipped
				if (triangleDeleted[i])
					continue;

				const u_int tvertex = candidateVertexIndex[i];
				if (tvertex == NULL_INDEX)
					continue;

				// Order-preserving transformation of the collapse error
				// to an unsigned integer: the IEEE-754 bit pattern is
				// monotonic for the non-negative floats and reversed for
				// the negative ones, so the sign bit is set for the former
				// and the whole word is flipped for the latter
				const std::uint32_t errorBits = std::bit_cast<std::uint32_t>(triangleErr[3*i + tvertex]);
				const std::uint32_t errorKey = (errorBits & 0x80000000u) ?
					~errorBits : (errorBits | 0x80000000u);

				// The tie break of the equal errors: the triangle index
				// scrambled by an odd multiplier (a bijection of
				// [0, 2^32), the keys stay unique). The scramble spreads
				// the candidates with exactly equal errors (the flat
				// regions) uniformly over the mesh: with the raw index
				// they were selected in storage order, and the selection
				// boundary left storage aligned bands in the mesh
				const std::uint32_t tieKey = u_int(i) * 0x9E3779B1u;

				candidateKeys.push_back(CandidateKey{
					(static_cast<std::uint64_t>(errorKey) << 32) | tieKey,
					SimplifyRef2{ u_int(i), tvertex } });
			}
			SDL_LOG("Simplify2: Found " << candidateKeys.size() << " edge candidates in "
				<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");

			// Keep the candidates below the error threshold: the first
			// iteration takes the N% lowest ones (the rank cut that
			// defines E), the following ones take everything below E
			// (the inclusive test: an error equal to the threshold is
			// kept)
			const size_t totalCandidateCount = candidateKeys.size();
			if (iteration == 0) {
				const u_int nPercentCount = std::max(1u,
						Floor2UInt(totalCandidateCount * initialCandidatePercent));
				if (candidateKeys.size() > nPercentCount)
					SelectLowestKeys(candidateKeys, nPercentCount);
			} else {
				// The threshold in the packed key domain: the error
				// word of the keys is the order preserving
				// transformation of the float error bits (see the key
				// building above), so the selection stays a single u64
				// comparison per candidate
				const std::uint32_t errorBits = std::bit_cast<std::uint32_t>(errorThreshold);
				const std::uint32_t thresholdErrorKey = (errorBits & 0x80000000u) ?
						~errorBits : (errorBits | 0x80000000u);
				const std::uint64_t thresholdKey =
						(std::uint64_t(thresholdErrorKey) << 32) | 0xffffffffu;

				SelectKeysBelowThreshold(candidateKeys, thresholdKey);
			}
			// The kept count (the keys are released below, after the
			// extraction of the references)
			const size_t keptCandidateCount = candidateKeys.size();

			// The production phase is over when the selected batch is
			// a negligible share of the live mesh: the touched set is
			// then small against the rescreening cost and the
			// evaluation cache pays (below one percent on the stress
			// scenes: the plane never crosses it, its drain reaches
			// the target with the batch still at six percent, Lucy
			// crosses it at iteration 16 and keeps it for the whole
			// tail)
			if (!evalCacheActive &&
					keptCandidateCount * 100 < startTriangleCount - totalDeletedTriangles) {
				evalCacheActive = true;
				SDL_LOG("Simplify2: Evaluation cache active (the kept batch fell below one percent"
						" of the live triangles)");
			}
			// The reference of the mesh dependent drain halt: the
			// batch of the iteration that defined the error threshold
			if (iteration == 0)
				initialKeptCandidateCount = keptCandidateCount;

			// Sort the kept candidates by error (ascending)
			tbb::parallel_sort(candidateKeys.begin(), candidateKeys.end(), keyCompare);

			// E, the error at the rank cut of the first iteration: the
			// keys are sorted, so the last kept one carries it. The
			// inverse of the order preserving transformation of the
			// key building
			if (iteration == 0 && !candidateKeys.empty()) {
				const std::uint32_t thresholdErrorKey =
						std::uint32_t(candidateKeys.back().key >> 32);
				const std::uint32_t errorBits = (thresholdErrorKey & 0x80000000u) ?
						(thresholdErrorKey & 0x7fffffffu) : ~thresholdErrorKey;
				errorThreshold = std::bit_cast<float>(errorBits);
			}

			// Extract the sorted references for the downstream phases:
			// the keys are only needed by the sort
			ScalableVector<SimplifyRef2> allCandidates;
			allCandidates.reserve(candidateKeys.size());
			for (const CandidateKey &candidateKey : candidateKeys)
				allCandidates.push_back(candidateKey.ref);

			// Release the sort keys (several hundreds of MB in the first
			// iterations)
			ScalableVector<CandidateKey>().swap(candidateKeys);

			SDL_LOG("Simplify2: Kept the " << allCandidates.size() << " lowest error candidates (error < "
				<< (boost::format("%.3g") % errorThreshold) << ")");

			// The mesh dependent halt of the drain: the kept batch has
			// fallen below a negligible remnant of the one that
			// defined the error threshold (one ten thousandth of it) -
			// the remaining iterations would delete an insignificant
			// share of the mesh at the full cost of one. The halt can
			// not fire on a productive drain (the batch stays within
			// orders of magnitude of the reference until the natural
			// end) and can not fire at all when the first batch is
			// small (one ten thousandth of it is below one candidate),
			// so it only engages where the tail is expensive: the
			// large meshes
			if (keptCandidateCount > 0 &&
					keptCandidateCount < initialKeptCandidateCount / 10000) {
				SDL_LOG("Simplify2: The kept batch (" << keptCandidateCount
					<< ") has fallen below one ten thousandth of the initial one ("
					<< initialKeptCandidateCount << ") - stopping the drain");
				break;
			}

			// Defer the region boundary candidates: the closures of
			// the main batch are then confined to the regions (and no
			// longer giant). The deferred strip is processed as a
			// second wave of the same iteration, so no candidate is
			// ever stranded (the strip used to come back with the next
			// iteration, but a strip whose neighborhood keeps region
			// spanning triangles is re-deferred forever: it freezes at
			// its initial density and stays visible along the region
			// grid)
			stepStartTime = WallClockTime();
			ScalableVector<SimplifyRef2> stripCandidates;
			const size_t deferredCandidates = DeferBoundaryCandidates(allCandidates, stripCandidates);
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

			// Second wave: the deferred seam candidates of the
			// iteration, processed alone after the region confined
			// closures. Their conflict graph reconnects through the
			// seams (typically one big closure), so the wave is mostly
			// serial - but the strip is thin and the closures of the
			// main wave are complete, so nothing is shared with them.
			// The strip coarsens with the mesh instead of freezing at
			// its initial density
			if (!stripCandidates.empty()) {
				stepStartTime = WallClockTime();
				ScalableVector<ScalableVector<u_int>> stripClosures =
					ComputeCandidateClosures(stripCandidates);
				ProcessClosuresParallel(stripClosures, stripCandidates);
				SDL_LOG("Simplify2: Processed the " << stripCandidates.size()
					<< " deferred region boundary candidates in "
					<< (boost::format("%.3f") % (WallClockTime() - stepStartTime)) << "secs");
			}

			const u_int iterationDeletedTriangles = deletedTriangles;
			totalDeletedTriangles += iterationDeletedTriangles;

			// Merge the appended star segments of the collapse phase
			// into the reference base: the walks of the next iteration
			// read the merged layout with the same two segment
			// arithmetic (the base including the appends, plus the
			// fresh per body tails of that iteration). The blocks of
			// the split bodies land in the reduce join order: the
			// merged layout varies with the scheduling, but the walks
			// are order independent (an OR over the star), so the
			// decisions are identical run after run
			{
				size_t appendTotal = 0;
				for (const auto& block : iterationRefAppends)
					appendTotal += block.first.size();

				const size_t baseSize = refTid.size();
				refTid.resize(baseSize + appendTotal);
				refTvertex.resize(baseSize + appendTotal);

				size_t offset = 0;
				for (auto& block : iterationRefAppends) {
					for (size_t k = 0; k < block.first.size(); ++k) {
						refTid[baseSize + offset + k] = block.first[k].tid;
						refTvertex[baseSize + offset + k] = block.first[k].tvertex;
					}
					// The repointed starts of the welded vertices were
					// relative to their own body tail: rewrite them
					// into the merged layout
					for (const auto& append : block.second)
						vertexTstart[append.vertex] =
								u_int(baseSize + offset + append.tailStart);

					offset += block.first.size();
				}
				iterationRefAppends.clear();

				// The garbage accounting: the appended segments
				// supersede roughly one old entry each and every
				// deleted triangle orphans its three base entries
				staleRefCount += appendTotal + 3 * size_t(iterationDeletedTriangles);
				uncompactedDeletions += iterationDeletedTriangles;
			}
			SDL_LOG("Simplify2 iteration " << iteration << " (" << allCandidates.size() << " edge candidates, deleted "
				<< iterationDeletedTriangles << "/" << totalDeletedTriangles << " of " << startTriangleCount
				<< " triangles) in " << (boost::format("%.3f") % (WallClockTime() - iterationStartTime)) << "secs");
			// An iteration that deletes nothing is terminal: the
			// threshold is fixed and the deferred candidates are
			// processed in the same iteration, so the errors can only
			// change with the collapses - the next iterations would
			// repeat the same selection
			if (iterationDeletedTriangles == 0) {
				// The homogeneous error certificate: no collapse
				// cheaper than the threshold remains anywhere in the
				// mesh
				if (keptCandidateCount == 0)
					SDL_LOG("Simplify2: No collapse below the error threshold "
						<< (boost::format("%.3g") % errorThreshold) << " remains (homogeneous error reached)");
				break;
			}
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
	// scrambled triangle index in the low word (an odd multiplier
	// bijection of [0, 2^32)). Every triangle contributes at most one
	// candidate, so the key is a strict total order over distinct
	// candidates: the comparison is a single u64 test (no error array
	// access in the comparator, no tie-break branches) and the sorted
	// sequence is independent of the sort algorithm and of the thread
	// scheduling. The scramble keeps the keys unique but orders the
	// candidates with exactly equal errors (the flat regions)
	// uniformly over the mesh instead of in storage order: the
	// selection boundary used to leave storage aligned bands
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
	// A star repoint record of the collapse phase: the welded vertex
	// and the start of its new reference segment inside the appending
	// body tail (relative to the base size)
	struct RefAppend {
		u_int vertex;
		u_int tailStart;
	};

	struct CollapseContext {
		// Cache aligned: the tail grows inside the parallel processing of
		// the closures (one context per thread)
		CacheAlignedVector<SimplifyRef2> refsTail;
		CacheAlignedVector<RefAppend> refAppends;
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
	// The three collapse errors of a triangle, interleaved (err(j, tid) =
	// triangleErr[3*tid + j]): the compactions move them in one
	// contiguous 12 byte block like the vertex indices, and the passes
	// that read the three errors of a triangle read one cache line
	// instead of three streams
	ScalableVector<float> triangleErr;
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
	// The recorded candidate of every triangle (the corner its last
	// evaluation selected, or NULL_INDEX when no corner passed) and
	// its validity flag. The evaluation of a triangle reproduces its
	// recorded outcome exactly while its inputs are unchanged, so the
	// flag turns the evaluation from a full rescreening of the mesh
	// into a recompute of the triangles touched by the collapses. The
	// collapses clear the flag on every triangle whose inputs changed:
	// the triangles of the collapsed edge stars (rewired, or with a
	// moved vertex) and, through the star walks, the triangles whose
	// flip tests read them (the tests walk the stars of the endpoints
	// and read the slots and the positions of their triangles). The
	// compaction moves the record and the flag with the triangle
	ScalableVector<u_int> candidateVertexIndex;
	ScalableVector<unsigned char> candidateValid;
	// The per vertex generation of the star invalidation: the current
	// iteration generation is compared against the vertex counter, so
	// a vertex star is walked at most once per iteration however many
	// collapses touch it. The concurrent plain stores of the same
	// value are the principle 2 idempotent shared writes
	ScalableVector<u_int> vertexInvalidatedGen;
	u_int invalidationGen = 1;
	// The deferred reference rebuild accounting: the star segments
	// appended since the last rebuild (merged into the reference base
	// at the end of every collapse phase; their superseded entries and
	// the deleted triangles' entries stay behind as garbage until the
	// rebuild), the triangles deleted since the last rebuild and the
	// live triangle count right after the last rebuild. The mesh
	// update rebuilds the reference list only when the accumulated
	// garbage exceeds one eighth of the live references: below that
	// the walks carry a negligible overhead and the O(live mesh)
	// count/prefix/fill passes cost more than they save
	size_t staleRefCount = 0;
	size_t uncompactedDeletions = 0;
	size_t rebuildLiveTriangleCount = 0;
	// The star segments appended by the collapse phase of the current
	// iteration, with the repoint records of their welded vertices:
	// handed over by the closure processors, consumed and cleared by
	// the merge at the end of the collapse phase
	std::vector<std::pair<CacheAlignedVector<SimplifyRef2>, CacheAlignedVector<RefAppend>>> iterationRefAppends;
	// The evaluation cache activates only once the run is out of the
	// production phase: while the selected batch is a large share of
	// the mesh, the collapses rewrite nearly everything, the records
	// would be invalidated as fast as they are written and the star
	// walks would cost more than the skipped rescreening saves. The
	// switch is a latch (the drain never returns to the production
	// ratios) and is written once per iteration, between the selection
	// and the parallel phases, so the plain read needs no
	// synchronization
	bool evalCacheActive = false;

	size_t GetTriangleCount() const { return triangleV.size() / 3; }

	void ResizeTriangles(const size_t count) {
		triangleV.resize(count * 3);
		triangleErr.resize(count * 3);
		triangleErrChoice.resize(count);
		triangleGeometryN.resize(count);
		triangleDeleted.resize(count);
		triangleDirty.resize(count);
		candidateVertexIndex.resize(count);
		candidateValid.resize(count);
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
		vertexInvalidatedGen.resize(count);
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

	// Select the n lowest keys of the array: a MSB first radix descent
	// on the 16 bit digits of the keys finds the threshold of the n
	// lowest ones, then they are compacted out of place.
	//
	// The keys are unique (the triangle index is in the low bits), so
	// the selected set is exactly the prefix std::nth_element would
	// have partitioned with, and the sort that follows produces the
	// same sequence whatever the order of the selected entries: the
	// compaction does not need to preserve it.
	//
	// The descent histograms one digit per level into one histogram per
	// thread (tbb::combinable, without any atomic) and combines them
	// serially. Only the keys sharing the digits fixed so far are
	// counted (the prefix test of the histogram passes: no compaction
	// happens during the search). The descent always ends on an exact
	// bin boundary - the last level at the latest, where a bin holds at
	// most one of the unique keys - so the selected set is exactly the
	// keys with key >> thresholdShift <= threshold, a single comparison
	// the compaction can test.
	//
	// The compaction is out of place in fixed size chunks: the chunks
	// count their selected keys, the offsets are the prefix of the
	// counts and each chunk copies into its own slot range (disjoint,
	// without any atomic). The swap releases the original array (several
	// hundreds of MB in the first iterations) before the sort runs on
	// the selected one.
	void SelectLowestKeys(ScalableVector<CandidateKey> &keys, const size_t n) {
		const size_t keyCount = keys.size();
		if (keyCount <= n)
			return;

		// The digit of the level and the prefix fixed so far
		unsigned thresholdShift;
		std::uint64_t threshold;
		{
			std::uint64_t prefix = 0;
			unsigned fixedBits = 0;
			size_t remaining = n;
			bool found = false;

			for (unsigned level = 0; level < 4 && !found; ++level) {
				const unsigned digitShift = 48 - 16 * level;

				// Histogram of the digit of the survivors, one per
				// thread
				tbb::combinable<ScalableVector<u_int>> localHistograms;
				tbb::parallel_for(tbb::blocked_range<size_t>(0, keyCount, 262144),
					[&](const tbb::blocked_range<size_t> &r) {
						ScalableVector<u_int> &localHistogram = localHistograms.local();
						if (localHistogram.empty())
							localHistogram.resize(65536, 0);

						for (size_t i = r.begin(); i < r.end(); ++i) {
							const std::uint64_t key = keys[i].key;

							// Only the keys sharing the digits fixed so
							// far (all of them at the first level)
							if (fixedBits && (key >> (64 - fixedBits)) != prefix)
								continue;

							++localHistogram[(key >> digitShift) & 0xffffu];
						}
					});

				// Combine the per thread histograms and scan for the
				// bin holding the rank
				ScalableVector<u_int> histogram(65536, 0);
				localHistograms.combine_each([&](const ScalableVector<u_int> &localHistogram) {
					for (size_t b = 0; b < 65536; ++b)
						histogram[b] += localHistogram[b];
				});

				size_t cumulative = 0;
				for (u_int digit = 0; digit < 65536; ++digit) {
					const size_t count = histogram[digit];
					// The prefix value of the digits fixed so far plus
					// this one (the value of the top fixedBits + 16 bits
					// of the key, the space of the threshold comparison
					// below)
					const std::uint64_t extendedPrefix = (prefix << 16) | digit;

					// The rank lands on the start of the bin: only the
					// keys below it are selected
					if (cumulative == remaining) {
						threshold = extendedPrefix - 1;
						thresholdShift = 64 - fixedBits - 16;
						found = true;
						break;
					}

					// The rank lands inside or at the end of the bin
					if (cumulative + count >= remaining) {
						if (cumulative + count == remaining) {
							// At the end: the whole bin is selected
							threshold = extendedPrefix;
							thresholdShift = 64 - fixedBits - 16;
							found = true;
							break;
						}

						// Inside: fix the digit and descend to the next
						// level with the reduced rank
						prefix = extendedPrefix;
						fixedBits += 16;
						remaining -= cumulative;
						break;
					}

					cumulative += count;
				}
			}
		}

		// Compact the selected keys out of place in fixed size chunks
		const size_t chunkSize = 262144;
		const size_t chunkCount = (keyCount + chunkSize - 1) / chunkSize;
		ScalableVector<size_t> chunkSelectedCounts(chunkCount, 0);
		tbb::parallel_for(size_t(0), chunkCount, [&](size_t c) {
			const size_t iBegin = c * chunkSize;
			const size_t iEnd = std::min(iBegin + chunkSize, keyCount);

			size_t selectedCount = 0;
			for (size_t i = iBegin; i < iEnd; ++i) {
				if ((keys[i].key >> thresholdShift) <= threshold)
					++selectedCount;
			}
			chunkSelectedCounts[c] = selectedCount;
		});

		// The chunk offsets (the prefix of the chunk counts)
		ScalableVector<size_t> chunkOffsets(chunkCount);
		size_t selectedTotal = 0;
		for (size_t c = 0; c < chunkCount; ++c) {
			chunkOffsets[c] = selectedTotal;
			selectedTotal += chunkSelectedCounts[c];
		}

		// The chunk slot ranges are disjoint
		ScalableVector<CandidateKey> selectedKeys(selectedTotal);
		tbb::parallel_for(size_t(0), chunkCount, [&](size_t c) {
			const size_t iBegin = c * chunkSize;
			const size_t iEnd = std::min(iBegin + chunkSize, keyCount);

			size_t k = chunkOffsets[c];
			for (size_t i = iBegin; i < iEnd; ++i) {
				if ((keys[i].key >> thresholdShift) <= threshold)
					selectedKeys[k++] = keys[i];
			}
		});

		// Release the original array and take the selected one
		keys.swap(selectedKeys);
	}
	// Select the keys below a threshold: the threshold comes from the
	// error schedule of the iteration (a value in the packed key
	// domain, see the selection in Decimate), not from a rank, so
	// there is no search. The keys are compacted out of place in fixed
	// size chunks: the chunks count their selected keys, the offsets
	// are the prefix of the counts and each chunk copies into its own
	// slot range (disjoint, without any atomic)
	void SelectKeysBelowThreshold(ScalableVector<CandidateKey> &keys,
			const std::uint64_t threshold) {
		const size_t keyCount = keys.size();

		// Compact the selected keys out of place in fixed size chunks
		const size_t chunkSize = 262144;
		const size_t chunkCount = (keyCount + chunkSize - 1) / chunkSize;
		ScalableVector<size_t> chunkSelectedCounts(chunkCount, 0);
		tbb::parallel_for(size_t(0), chunkCount, [&](size_t c) {
			const size_t iBegin = c * chunkSize;
			const size_t iEnd = std::min(iBegin + chunkSize, keyCount);

			size_t selectedCount = 0;
			for (size_t i = iBegin; i < iEnd; ++i) {
				if (keys[i].key <= threshold)
					++selectedCount;
			}
			chunkSelectedCounts[c] = selectedCount;
		});

		// The chunk offsets (the prefix of the chunk counts)
		ScalableVector<size_t> chunkOffsets(chunkCount);
		size_t selectedTotal = 0;
		for (size_t c = 0; c < chunkCount; ++c) {
			chunkOffsets[c] = selectedTotal;
			selectedTotal += chunkSelectedCounts[c];
		}

		// The threshold keeps every key: nothing to compact
		if (selectedTotal == keyCount)
			return;

		// The chunk slot ranges are disjoint
		ScalableVector<CandidateKey> selectedKeys(selectedTotal);
		tbb::parallel_for(size_t(0), chunkCount, [&](size_t c) {
			const size_t iBegin = c * chunkSize;
			const size_t iEnd = std::min(iBegin + chunkSize, keyCount);

			size_t k = chunkOffsets[c];
			for (size_t i = iBegin; i < iEnd; ++i) {
				if (keys[i].key <= threshold)
					selectedKeys[k++] = keys[i];
			}
		});

		// Release the original array and take the selected one
		keys.swap(selectedKeys);
	}

	bool CollapseEdge(const size_t trinagleIndex, const size_t startVertexIndex,
			CollapseContext &ctx, ScalableVector<unsigned char> &deleted0, ScalableVector<unsigned char> &deleted1) {
		if (triangleDeleted[trinagleIndex])
			return false;
		if (triangleDirty[trinagleIndex])
			return false;

		// The triangle corner vertex indices, used all along the
		// function. Three scalars: an array would sit on the stack (the
		// dynamic corner selection below forces it out of the
		// registers) and every constant index use would become a
		// reload
		const u_int triVertex0 = triangleV[3*trinagleIndex+0];
		const u_int triVertex1 = triangleV[3*trinagleIndex+1];
		const u_int triVertex2 = triangleV[3*trinagleIndex+2];

		// The collapse endpoints, selected between the scalars (a
		// register selection, no memory)
		const u_int nextVertexIndex = TRI_NEXT[startVertexIndex];
		const u_int i0 = (startVertexIndex == 0) ? triVertex0 :
			(startVertexIndex == 1) ? triVertex1 : triVertex2;
		const u_int i1 = (nextVertexIndex == 0) ? triVertex0 :
			(nextVertexIndex == 1) ? triVertex1 : triVertex2;

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
		const Point triPoint0 = vertexP[triVertex0];
		const Point triPoint1 = vertexP[triVertex1];
		const Point triPoint2 = vertexP[triVertex2];

		const Normal triNorm0 = vertexNorm[triVertex0];
		const Normal triNorm1 = vertexNorm[triVertex1];
		const Normal triNorm2 = vertexNorm[triVertex2];

		const UV triUV0 = vertexUV[triVertex0];
		const UV triUV1 = vertexUV[triVertex1];
		const UV triUV2 = vertexUV[triVertex2];

		const Spectrum triCol0 = vertexCol[triVertex0];
		const Spectrum triCol1 = vertexCol[triVertex1];
		const Spectrum triCol2 = vertexCol[triVertex2];

		const float triAlpha0 = vertexAlpha[triVertex0];
		const float triAlpha1 = vertexAlpha[triVertex1];
		const float triAlpha2 = vertexAlpha[triVertex2];

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
		// The merge at the end of the collapse phase carries the tail into
		// the reference base when the rebuild is deferred (UpdateMesh
		// rebuilds the whole list only when the garbage accumulated
		// since the last rebuild exceeds its threshold), so the record
		// below lets the merge rewrite the start into the merged layout
		vertexTstart[i0] = u_int(tstart);
		vertexTcount[i0] = u_int(tcount);
		ctx.refAppends.push_back(RefAppend{ u_int(i0), u_int(tstart - refTid.size()) });

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
			ScalableVector<unsigned char> *deleted) const {
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
					(*deleted)[k] = 1;
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
				(*deleted)[k] = 0;

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
			const CollapseContext &ctx, ScalableVector<unsigned char> &deleted) const {
		return FlippedImpl<true>(p, i0, i1, ctx, &deleted);
	}

	// Invalidate the recorded candidates of the triangles of a vertex
	// star: their flip tests walk the star and read the slots and the
	// positions of its triangles, so any change of the star content (a
	// deletion, a rewire, a move of a co-vertex) makes the recorded
	// outcome stale. The per vertex generation dedups the walks within
	// the iteration; the plain stores of the same value, on both the
	// generation and the validity flag, are the principle 2 idempotent
	// shared writes
	void InvalidateVertexStar(const size_t v, const CollapseContext &ctx) {
		// The records of an inactive cache are never read: no walk
		if (!evalCacheActive)
			return;
		if (vertexInvalidatedGen[v] == invalidationGen)
			return;
		vertexInvalidatedGen[v] = invalidationGen;

		// The references of the vertex are contiguous: the ones in
		// the global baseline and the ones appended to the collapse
		// tail form two segments (like in FlippedImpl)
		const u_int tstart = vertexTstart[v];
		const u_int tcount = vertexTcount[v];
		const size_t baseSize = refTid.size();
		const size_t baseRefCount = (tstart < baseSize) ?
				std::min<size_t>(tcount, baseSize - tstart) : 0;

		for (size_t k = 0; k < baseRefCount; ++k)
			candidateValid[refTid[tstart + k]] = 0;
		for (size_t k = baseRefCount; k < tcount; ++k)
			candidateValid[ctx.refsTail[tstart + k - baseSize].tid] = 0;
	}

	// Update triangle connections and edge error after a edge is collapsed
	void UpdateTriangles(const size_t i0, const size_t vertexIndex,
			const ScalableVector<unsigned char> &deleted, CollapseContext &ctx) {
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

				// The triangles whose flip tests walked the deleted
				// triangle read a changed star: their recorded
				// candidates are stale. The walks of the two collapsed
				// edge endpoints are redundant with the rewire branch
				// below (the loops invalidate their stars) but the
				// generation dedup makes them free after the first one
				InvalidateVertexStar(triangleV[3*tid + 0], ctx);
				InvalidateVertexStar(triangleV[3*tid + 1], ctx);
				InvalidateVertexStar(triangleV[3*tid + 2], ctx);
				return;
			}

			triangleV[3*tid + r.tvertex] = u_int(i0);
			triangleDirty[tid] = true;
			UpdateTriangleError(tid);

			// The triangle slots (or the position of the moved vertex
			// they share) changed: the recorded candidate of this
			// triangle is stale, and so are the ones of the triangles
			// whose flip tests read it - they walk the stars of its
			// other two vertices and read their slots and positions
			candidateValid[tid] = 0;
			InvalidateVertexStar(triangleV[3*tid + TRI_NEXT[r.tvertex]], ctx);
			InvalidateVertexStar(triangleV[3*tid + TRI_PREV[r.tvertex]], ctx);

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
	// Returns true when the compaction and the reference rebuild were
	// deferred (see the accounting members)
	bool UpdateMesh(const size_t iteration) {
		// The triangle count is loaded once per mesh state, before and
		// after the compaction (the vertex count does not change): the
		// passes below write through the mesh arrays, so the compiler
		// can not hoist the vector size loads out of the loop
		// conditions and would otherwise reload the array bounds and
		// recompute the counts at every iteration (two loads, a
		// subtraction and the division by three, visible in the profile
		// of the compaction back edge)
		const size_t triangleCount = GetTriangleCount();
		size_t liveTriangleCount = triangleCount;
		const size_t vertexCount = GetVertexCount();

		// The reference rebuild and the compaction are deferred while
		// the garbage accumulated since the last rebuild (the appended
		// star segments and the orphaned entries of the deleted
		// triangles) stays under one eighth of the live references:
		// the walks then carry a negligible overhead, while the
		// O(live mesh) compaction and count/prefix/fill passes
		// dominate the cost of the drain iterations. The first
		// iteration always rebuilds (it builds the initial list) and
		// the deferred deletions stay in the arrays, skipped through
		// their flags by every pass that walks the triangles
		const size_t liveNow = rebuildLiveTriangleCount - uncompactedDeletions;
		const bool rebuildRefs = (iteration == 0) ||
				staleRefCount * 8 > liveNow * 3;

		if (rebuildRefs && iteration > 0) {
			// Compact the triangle arrays: the fields are moved one by
			// one (the in place slots are skipped)
			//
			// The move stays serial, and the parallelism stays out of
			// it entirely (not across the triangles - an in place move
			// is race free only strictly left to right, the destination
			// of a move can lag deep inside the sources another thread
			// would not have read yet - and not across the arrays
			// either: measured, one thread per array runs 0.15s slower
			// over the 9 iterations, the big array streams land on the
			// efficiency cores and the wall becomes their straggling
			// pass. The out of place alternatives pay more in scratch
			// buffer initialization and page faulting than the whole
			// serial move)
			size_t dst = 0;
			for (size_t i = 0; i < triangleCount; ++i) {
				if (triangleDeleted[i])
					continue;

				if (dst != i) {
					// The three vertex indices and the three errors are
					// contiguous, one copy moves each block (the ranges
					// never overlap: a move only happens when dst < i,
					// so the destination ends at or before the source
					// starts)
					std::copy_n(&triangleV[3*i], 3, &triangleV[3*dst]);
					std::copy_n(&triangleErr[3*i], 3, &triangleErr[3*dst]);
					triangleErrChoice[dst] = triangleErrChoice[i];
					triangleGeometryN[dst] = triangleGeometryN[i];
					triangleDirty[dst] = triangleDirty[i];
					candidateVertexIndex[dst] = candidateVertexIndex[i];
					candidateValid[dst] = candidateValid[i];
					triangleDeleted[dst] = false;
				}

				++dst;
			}

			ResizeTriangles(dst);
			liveTriangleCount = dst;
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
			for (size_t i = 0; i < vertexCount; ++i)
				vertexQ[i] = SymetricMatrix2(0.0);

			for (size_t i = 0; i < liveTriangleCount; ++i) {
				const size_t triOffset = 3*i;
				const u_int iv0 = triangleV[triOffset+0];
				const u_int iv1 = triangleV[triOffset+1];
				const u_int iv2 = triangleV[triOffset+2];

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

			tbb::parallel_for(size_t(0), liveTriangleCount, [&](size_t i) {
				UpdateTriangleError(i);
			});
		}

		if (rebuildRefs) {
		// Build the vertex -> triangles reference list (a CSR over the
		// vertices). The reference counts, the prefix offsets and the
		// fill cursors are accumulated in compact arrays (a few MB,
		// resident in the caches): the random increments of the count
		// and fill passes would otherwise touch the vertex records at
		// every step and stream the whole vertex array through the last
		// level cache. The tstart/tcount fields are written back in the
		// sequential prefix pass, which also covers the unused
		// vertices (tcount 0), so no separate initialization is needed.
		ScalableVector<u_int> vertexRefCounts(vertexCount, 0);
		for (size_t i = 0; i < liveTriangleCount; ++i) {
			const size_t triOffset = 3*i;
			++vertexRefCounts[triangleV[triOffset+0]];
			++vertexRefCounts[triangleV[triOffset+1]];
			++vertexRefCounts[triangleV[triOffset+2]];
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
		const size_t refCount = liveTriangleCount * 3;
		refTid.resize(refCount);
		refTvertex.resize(refCount);
		{
			ScalableVector<u_int> vertexRefCursors(vertexRefStarts);
			for (size_t i = 0; i < liveTriangleCount; ++i) {
				const size_t triOffset = 3*i;
				for (size_t j = 0; j < 3; ++j) {
					const u_int slot = vertexRefCursors[triangleV[triOffset+j]]++;

					refTid[slot] = u_int(i);
					refTvertex[slot] = u_int(j);
				}
			}
		}

		// The rebuild resets the garbage accounting: the reference
		// list is clean and the deleted triangles are compacted away
		staleRefCount = 0;
		uncompactedDeletions = 0;
		rebuildLiveTriangleCount = liveTriangleCount;
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
			tbb::parallel_for(size_t(0), vertexCount, [this](size_t i) {
				vertexBorder[i] = false;
			});

			tbb::parallel_for(tbb::blocked_range<size_t>(0, vertexCount, 16384),
				[&](const tbb::blocked_range<size_t> &r) {
					// Reused across the vertices of the range: the clear
					// is just a size reset, no reallocation
					ScalableVector<u_int> vcount, vids;
					for (size_t i = r.begin(); i < r.end(); ++i) {
						// The distinct link vertices of a star are the
						// vertex itself plus at most two per triangle
						// of the star, so the bound is exact and the
						// push backs below never reallocate (a reserve
						// under the capacity reached by a bigger star
						// is a no op)
						vcount.clear();
						vids.clear();
						vcount.reserve(2 * vertexTcount[i] + 1);
						vids.reserve(2 * vertexTcount[i] + 1);

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
		tbb::parallel_for(size_t(0), liveTriangleCount, [&](size_t i) {
			triangleDirty[i] = false;
		});

		return !rebuildRefs;
	}  // UpdateMesh

	// Finally compact mesh before exiting
	void CompactMesh() {
		// The counts are loaded once (the same rationale as in
		// UpdateMesh: the compaction passes write through the mesh
		// arrays in place, so the compiler can not hoist the vector
		// size loads out of the loop conditions). The triangle count
		// changes with the compaction, the vertex count does not
		const size_t triangleCount = GetTriangleCount();
		size_t liveTriangleCount;
		const size_t vertexCount = GetVertexCount();

		size_t dst = 0;

		for (size_t i = 0; i < vertexCount; ++i)
			vertexTcount[i] = 0;

		// Compact the triangle arrays: the fields are moved one by one
		// (the in place slots are skipped) and the used vertices are
		// marked
		for (size_t i = 0; i < triangleCount; ++i) {
			if (triangleDeleted[i]) continue;

			const size_t triOffset = 3*i;

			if (dst != i) {
				// The three vertex indices and the three errors are
				// contiguous, one copy moves each block (the ranges
				// never overlap: a move only happens when dst < i)
				const size_t dstOffset = 3*dst;
				std::copy_n(&triangleV[triOffset], 3, &triangleV[dstOffset]);
				std::copy_n(&triangleErr[triOffset], 3, &triangleErr[dstOffset]);
				triangleErrChoice[dst] = triangleErrChoice[i];
				triangleGeometryN[dst] = triangleGeometryN[i];
				triangleDirty[dst] = triangleDirty[i];
				candidateVertexIndex[dst] = candidateVertexIndex[i];
				candidateValid[dst] = candidateValid[i];
				triangleDeleted[dst] = false;
			}

			vertexTcount[triangleV[triOffset+0]] = 1;
			vertexTcount[triangleV[triOffset+1]] = 1;
			vertexTcount[triangleV[triOffset+2]] = 1;

			++dst;
		}
		ResizeTriangles(dst);
		liveTriangleCount = dst;

		// Compact the vertex arrays: only the output fields are moved
		// (like the record compaction: the quadrics and the flags stay
		// behind, they are not used anymore) and the new index of each
		// survivor is kept in its own tstart slot for the remap
		dst = 0;
		for (size_t i = 0; i < vertexCount; ++i) {
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
		for (size_t i = 0; i < liveTriangleCount; ++i) {
			const size_t triOffset = 3*i;
			triangleV[triOffset+0] = vertexTstart[triangleV[triOffset+0]];
			triangleV[triOffset+1] = vertexTstart[triangleV[triOffset+1]];
			triangleV[triOffset+2] = vertexTstart[triangleV[triOffset+2]];
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

		// The +1 is already applied above (with the error1/2/3): it was
		// applied a second time here for a long time, and the constant
		// dominated the geometric error by orders of magnitude on
		// dense meshes (the selection then followed the screen scale
		// or the float noise instead of the geometry)
		return std::max(error, 0.f);
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
		const size_t triOffset = 3*tid;
		unsigned char choice[3];
		const u_int triVertex0 = triangleV[triOffset+0];
		const u_int triVertex1 = triangleV[triOffset+1];
		const u_int triVertex2 = triangleV[triOffset+2];
		triangleErr[triOffset+0] = CalculateCollapseError(triVertex0, triVertex1, nullptr, &choice[0]);
		triangleErr[triOffset+1] = CalculateCollapseError(triVertex1, triVertex2, nullptr, &choice[1]);
		triangleErr[triOffset+2] = CalculateCollapseError(triVertex2, triVertex0, nullptr, &choice[2]);

		// The choice of each edge travels with the error: the candidate
		// evaluation reconstructs the collapse point from it
		triangleErrChoice[tid] = static_cast<unsigned char>(choice[0] | (choice[1] << 2) | (choice[2] << 4));

		if (edgeScreenSize > 0.f) {
			const float notVisibleScale = .5f;

			float sx[3], sy[3];
			bool visible[3];
			for (size_t j = 0; j < 3; ++j) {
				visible[j] = GetScreenPosition(triangleV[triOffset+j], &sx[j], &sy[j]);
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

				triangleErr[triOffset + j] *= scale;
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
	// The deferred candidates are moved to their own list and processed
	// as a second wave of the same iteration: a candidate whose
	// neighborhood keeps region spanning triangles is re-deferred at
	// every iteration, so giving the strip back to the next iteration
	// would freeze it at its initial density (a visible strip along
	// the region grid). The measured strip is 1-3% of the candidates
	// per iteration. The deferral is applied after the candidate
	// selection, so the deferred candidates consume their share of the
	// selection.
	//
	// Deterministic: the regions are a pure function of the vertex
	// positions (the bounding box reduction is schedule independent).
	// The decisions differ from the non deferred version, though (the
	// deferred collapses happen in the second wave, after the region
	// confined ones).
	//
	// Returns the number of deferred candidates (0 also when the
	// deferral is skipped), compacts the kept candidates in place
	// keeping the error order, and moves the deferred strip to the
	// given list (cleared first), also in error order
	size_t DeferBoundaryCandidates(ScalableVector<SimplifyRef2>& candidates,
			ScalableVector<SimplifyRef2>& strip) {
		strip.clear();

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
			// A triangle deleted by a deferred compaction spans no
			// region: it would mark its vertices seam adjacent and
			// defer candidates the compacted mesh would keep
			if (triangleDeleted[t]) {
				mixedTri[t] = 0;
				return;
			}

			const u_int r0 = regionOfVertex[triangleV[3 * t + 0]];
			const u_int r1 = regionOfVertex[triangleV[3 * t + 1]];
			const u_int r2 = regionOfVertex[triangleV[3 * t + 2]];
			mixedTri[t] = (r0 != r1) || (r1 != r2);
		});

		// A vertex is seam adjacent iff one of the triangles of its
		// star is a seam triangle (the same star the reference walk of
		// the candidate test below would scan): marked with idempotent
		// one byte writes, like the border identification (several
		// threads can store 1 in the flag of a shared vertex, no
		// update is lost). The seam triangles are rare (they only cut
		// the regions apart), so the pass streams the flags of the
		// triangles and rarely writes
		ScalableVector<unsigned char> seamAdjacentVertex(GetVertexCount(), 0);
		tbb::parallel_for(size_t(0), GetTriangleCount(), [&](size_t t) {
			if (mixedTri[t]) {
				seamAdjacentVertex[triangleV[3 * t + 0]] = 1;
				seamAdjacentVertex[triangleV[3 * t + 1]] = 1;
				seamAdjacentVertex[triangleV[3 * t + 2]] = 1;
			}
		});

		// A candidate is deferred iff one of its endpoints is seam
		// adjacent: exactly the test of the star walk it replaces (a
		// seam triangle in the star of one of the endpoints), two byte
		// loads per candidate instead of the walk of the two stars
		ScalableVector<unsigned char> deferred(candidateCount, 0);
		tbb::parallel_for(size_t(0), candidateCount, [&](size_t i) {
			const size_t tid = candidates[i].tid;
			const size_t tvertex = candidates[i].tvertex;
			deferred[i] = seamAdjacentVertex[triangleV[3 * tid + tvertex]] ||
				seamAdjacentVertex[triangleV[3 * tid + TRI_NEXT[tvertex]]];
		});

		// Skip the deferral when it would leave too few candidates
		size_t keptCount = 0;
		for (size_t i = 0; i < candidateCount; ++i)
			keptCount += 1u - deferred[i];
		if (keptCount < minKeptCandidates)
			return 0;

		// Compact the kept candidates in place and move the strip out,
		// both keeping the error order
		size_t keptIndex = 0;
		for (size_t i = 0; i < candidateCount; ++i) {
			if (deferred[i])
				strip.push_back(candidates[i]);
			else
				candidates[keptIndex++] = candidates[i];
		}
		candidates.resize(keptIndex);

		return candidateCount - keptIndex;
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

		// Candidate bearing vertices: a vertex carries a candidate iff
		// it is the endpoint of one. The flags are marked by the
		// candidates themselves with idempotent one byte writes (several
		// candidates can share an endpoint, so several threads can store
		// 1 in the flag of the same vertex: no update is lost). The
		// relation generator and the closure building below only need
		// this bearing test and the dense index: the vertex -> candidates
		// lists of the previous CSR (a serial count, prefix sum and
		// fill, plus a tid -> candidate map) were only read by the
		// closure gathering, which now scatters the candidates directly
		// (sorted and deduplicated by construction)
		const size_t vertexCount = GetVertexCount();

		ScalableVector<unsigned char> vertexHasCandidate(vertexCount, 0);
		tbb::parallel_for(size_t(0), candidateCount, [&](size_t i) {
			vertexHasCandidate[triangleV[3*candidates[i].tid + candidates[i].tvertex]] = 1;
			vertexHasCandidate[triangleV[3*candidates[i].tid + TRI_NEXT[candidates[i].tvertex]]] = 1;
		});

		// Candidate bearing vertices (a vertex carries a candidate iff
		// it is the endpoint of one), compacted into a dense index: the
		// elements of the vertex relation above
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
				// A triangle deleted by a deferred compaction links no
				// vertices: it would merge closures the compacted mesh
				// keeps apart
				if (triangleDeleted[t])
					continue;

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

		// Build the closures by scattering the candidates in ascending
		// index order (the greedy processing order): the closure of a
		// candidate is the component of its endpoints (the relation
		// generator links the two candidate bearing corners of the
		// candidate triangle, so both endpoints are in the same
		// component), and a candidate is scattered exactly once. Every
		// closure therefore receives its candidates already sorted and
		// deduplicated - the previous gather collected them from the
		// vertex lists (once per endpoint vertex) and sorted and
		// deduplicated the result
		ScalableVector<u_int> closureOfCandVertex(candVertexCount);
		tbb::parallel_for(size_t(0), classes.size(), [&](size_t c) {
			for (const size_t cv : classes[c])
				closureOfCandVertex[cv] = static_cast<u_int>(c);
		});

		ScalableVector<u_int> closureOfCandidate(candidateCount);
		tbb::parallel_for(size_t(0), candidateCount, [&](size_t i) {
			const u_int v = triangleV[3*candidates[i].tid + candidates[i].tvertex];
			closureOfCandidate[i] = closureOfCandVertex[candVertexOfVertex[v]];
		});

		// The histogram over the closures and the scatter itself stay
		// serial: the cursors of a closure would race otherwise
		ScalableVector<u_int> closureStart(classes.size() + 1, 0);
		for (size_t i = 0; i < candidateCount; ++i)
			++closureStart[closureOfCandidate[i] + 1];
		for (size_t c = 0; c < classes.size(); ++c)
			closureStart[c + 1] += closureStart[c];

		ScalableVector<u_int> flatClosures(candidateCount);
		{
			ScalableVector<u_int> closureCursor(closureStart.begin(), closureStart.end() - 1);
			for (size_t i = 0; i < candidateCount; ++i)
				flatClosures[closureCursor[closureOfCandidate[i]]++] = static_cast<u_int>(i);
		}

		// Slice the flat array into the closures (the slot ranges are
		// disjoint)
		ScalableVector<ScalableVector<u_int>> closures(classes.size());
		tbb::parallel_for(size_t(0), classes.size(), [&](size_t c) {
			closures[c].assign(flatClosures.begin() + closureStart[c],
				flatClosures.begin() + closureStart[c + 1]);
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

		// The appended star blocks collected from the split bodies
		// (moved in, never copied: the join only chains)
		std::vector<std::pair<CacheAlignedVector<SimplifyRef2>, CacheAlignedVector<RefAppend>>> appendBlocks;

		// Candidate triangles deleted by this body (for the disjointness check).
		// Cache aligned: one per thread, appended in the parallel processing
		CacheAlignedVector<u_int> deletedCandidates;

	public:
		// Constructor for the master thread. The context counter
		// starts at zero: every call counts only its own deletions and
		// the iteration accumulates the waves through applyResult
		// (pre-loading the global counter here would count the main
		// wave twice, once per wave)
		ParallelClosureProcessor(Simplify2& s,
				const ScalableVector<ScalableVector<u_int>>& c,
				const ScalableVector<SimplifyRef2>& a)
			: simplify(s), closures(c), allCandidates(a) { }

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

			// Chain the sibling's appended star block (moved, no copy):
			// its own tail and records first, then the blocks it chained
			// from its own splits. The reduce folds every split into the
			// left body, so anything not carried here is destroyed with
			// the sibling (the reduce tree splits several levels deep,
			// which would lose almost every appended reference)
			appendBlocks.emplace_back(std::move(other.ctx.refsTail),
					std::move(other.ctx.refAppends));
			appendBlocks.insert(appendBlocks.end(),
					std::make_move_iterator(other.appendBlocks.begin()),
					std::make_move_iterator(other.appendBlocks.end()));
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

			// Accumulated and not assigned: the second wave of the
			// iteration (the deferred strip) calls this again on top of
			// the main result (the counter is reset at the beginning
			// of every iteration)
			simplify.deletedTriangles += ctx.deletedCount;

			// Hand the appended star blocks over: the own tail and
			// records first (the own body), then the chained ones (the
			// splits). Serialized: applyResult runs on the master
			// processor only
			simplify.iterationRefAppends.emplace_back(std::move(ctx.refsTail),
					std::move(ctx.refAppends));
			for (auto& block : appendBlocks)
				simplify.iterationRefAppends.push_back(std::move(block));
			appendBlocks.clear();
		}

	private:
		// Process a single closure
		void ProcessClosure(const ScalableVector<u_int>& closureIndices) {
			// Scalable allocator: recreated for every closure (no caching
			// expected), resized for every candidate
			ScalableVector<unsigned char> deleted0, deleted1;

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
