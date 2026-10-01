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

#include "slg/utils/group_by_equivalence.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>
#include <algorithm>
#include "oneapi/tbb.h"

namespace {

// Vector with the TBB scalable allocator (tbbmalloc): the UnionFind arrays
// are allocated per reduce body (one pair per thread, constructed inside
// the parallel execution), and tbbmalloc serves each thread from its own
// cache, avoiding contention on the shared heap
template<typename T>
using ScalableVector = std::vector<T, tbb::scalable_allocator<T>>;

// Classical Union-Find (disjoint set union) on flat arrays: the elements
// form the dense range [0, numElements), so the parent and rank structures
// are plain vectors instead of hash maps.
//
// The parent and rank entries are uint32_t/unsigned char (the elements are mesh
// scale indices, below 2^32): the parent array is randomly touched by
// every find, so shrinking it (and the rank, zero filled at every reduce
// body construction) reduces both the cache footprint of the searches
// and the cost of the per body construction (the parent is iota filled).
//
// The elements that became non roots are logged at link time: a reduce
// join then merges through the logged elements only (every non root
// became so at exactly one link, path halving never turns a root into a
// non root), instead of scanning all the elements.
// Deleter for the flat arrays allocated with the TBB scalable allocator
// (scalable_malloc/scalable_free are global functions of
// scalable_allocator.h)
struct ScalableDelete {
	template<typename T>
	void operator()(T *p) const { ::scalable_free(p); }
};

class UnionFind {
	// The parent and rank are flat arrays instead of vectors: a vector
	// cannot be sized without value initializing its entries, so the
	// identity parent would be written twice at every reduce body
	// construction (zero fill, then iota)
	std::unique_ptr<uint32_t[], ScalableDelete> parent;
	std::unique_ptr<unsigned char[], ScalableDelete> rank;
	// The elements that became non roots (one entry per effective link)
	ScalableVector<uint32_t> nonRoots;

public:
	UnionFind() = default;
	explicit UnionFind(const size_t count) :
			parent(static_cast<uint32_t *>(::scalable_malloc(count * sizeof(uint32_t)))),
			rank(static_cast<unsigned char *>(::scalable_malloc(count * sizeof(unsigned char)))) {
		std::iota(parent.get(), parent.get() + count, 0u);
		std::memset(rank.get(), 0, count);
		// Every element becomes a non root at most once, so the log can
		// never outgrow the element count: reserving it upfront avoids
		// the growth reallocations of the links (the pages of the
		// reserved tail materialize only when touched)
		nonRoots.reserve(count);
	}

	// Find the root of the set containing i (path halving)
	size_t find(size_t i) {
		while (parent[i] != i) {
			parent[i] = parent[parent[i]];
			i = parent[i];
		}
		return i;
	}
	// The root of the element, without touching the parents: the
	// trees are read only once the reduce is over, and the parallel
	// classes build walks them from many threads at once
	size_t findRoot(size_t i) const {
		while (parent[i] != i)
			i = parent[i];
		return i;
	}


	// Union the sets containing i and j (union by rank)
	void unite(const size_t i, const size_t j) {
		const size_t rootI = find(i);
		const size_t rootJ = find(j);

		if (rootI == rootJ)
			return;
		if (rank[rootI] < rank[rootJ]) {
			parent[rootI] = static_cast<uint32_t>(rootJ);
			nonRoots.push_back(static_cast<uint32_t>(rootI));
		} else if (rank[rootI] > rank[rootJ]) {
			parent[rootJ] = static_cast<uint32_t>(rootI);
			nonRoots.push_back(static_cast<uint32_t>(rootJ));
		} else {
			parent[rootJ] = static_cast<uint32_t>(rootI);
			nonRoots.push_back(static_cast<uint32_t>(rootJ));
			rank[rootI]++;
		}
	}

	// Merge another UnionFind into this one: unite every logged non root
	// with its (forest edge) parent, which merges all the components
	// (the roots are reached through their members)
	UnionFind& operator+=(const UnionFind& other) {
		for (const uint32_t x : other.nonRoots)
			unite(x, other.parent[x]);
		return *this;
	}
};

// Processes the equivalence relations in parallel: each thread unions its
// range of relations into its own UnionFind, merged by the reduce step.
// The UnionFind trees are valid at any time (only complete merges).
template<typename Range>
class ParallelGroupByEquivalence {
	UnionFind dsu;
	const size_t numPoints;
	Range relation;

public:
	ParallelGroupByEquivalence(size_t p_numPoints, Range p_relation)
		: dsu(p_numPoints), numPoints(p_numPoints), relation(std::move(p_relation)) {}

	ParallelGroupByEquivalence(ParallelGroupByEquivalence& x, tbb::split)
		: dsu(x.numPoints), numPoints(x.numPoints), relation(x.relation) {}

	// Body: process a range of indices into the relation
	void operator()(const tbb::blocked_range<size_t>& r) {
		auto it = std::ranges::begin(relation);
		std::advance(it, r.begin());
		for (size_t i = r.begin(); i < r.end(); ++i, ++it) {
			const auto& [a, b] = *it;
			dsu.unite(a, b);
		}
	}

	// Reduction: merge the sibling UnionFind into this one
	void join(ParallelGroupByEquivalence& rhs) {
		dsu += rhs.dsu;
	}

	UnionFind& getResult() {
		return dsu;
	}
};

// Helper class for building UnionFind from a callable generator
// using tbb::parallel_reduce
template<typename Generator>
class ParallelGroupByEquivalenceFromGenerator {
	const size_t numPoints;
	Generator relation;
	UnionFind dsu;

public:
	ParallelGroupByEquivalenceFromGenerator(size_t p_numPoints,
		Generator p_relation)
		: numPoints(p_numPoints),
		  relation(std::move(p_relation)),
		  dsu(p_numPoints) {}

	ParallelGroupByEquivalenceFromGenerator(
		ParallelGroupByEquivalenceFromGenerator& x, tbb::split)
		: numPoints(x.numPoints),
		  relation(x.relation),
		  dsu(x.numPoints) {}

	void operator()(const tbb::blocked_range<size_t>& r) {
		// Get pairs for this range
		auto pairsRange = relation(r.begin(), r.end());

		// Process each pair
		for (const auto& [i, j] : pairsRange) {
			dsu.unite(i, j);
		}
	}

	void join(ParallelGroupByEquivalenceFromGenerator<Generator>& rhs) {
		dsu += rhs.dsu;
	}

	UnionFind& getResult() {
		return dsu;
	}
};

// Build the classes from a final UnionFind: flatten the trees (single
// threaded: the union-find is not used concurrently anymore), then count
// the class sizes, prefix sum and fill (CSR): no hashing and no per class
// allocation until the final slicing. The elements end up in ascending
// order inside each class.
slg::Classes BuildClassesFromUnionFind(const UnionFind& uf, const size_t numElements) {
	if (numElements == 0)
		return {};

	// The roots of the elements and the root flags, in one parallel
	// pass: the reduce is over, so the parent trees are read only and
	// every element can walk to its root from any thread (the serial
	// version halved the paths along the way, which would race)
	ScalableVector<uint32_t> rootOfElement(numElements);
	ScalableVector<unsigned char> isRoot(numElements, 0);
	tbb::parallel_for(size_t(0), numElements, [&](size_t i) {
		const uint32_t root = static_cast<uint32_t>(uf.findRoot(i));

		rootOfElement[i] = root;
		isRoot[root] = 1;
	});

	// The dense class ids: the exclusive prefix over the root flags
	// (a root receives the number of the roots before it, so the ids
	// follow the ascending root order - the very order of the serial
	// slicing - and the class id domain is small where the root id
	// domain is element sized)
	class DenseClassIdScan {
		ScalableVector<uint32_t> &classIdOfRoot;
		const ScalableVector<unsigned char> &isRoot;
		size_t count;

	public:
		DenseClassIdScan(ScalableVector<uint32_t> &p_classIdOfRoot,
				const ScalableVector<unsigned char> &p_isRoot)
			: classIdOfRoot(p_classIdOfRoot), isRoot(p_isRoot), count(0) { }
		DenseClassIdScan(DenseClassIdScan &other, tbb::split)
			: classIdOfRoot(other.classIdOfRoot), isRoot(other.isRoot),
			  count(0) { }

		void operator()(const tbb::blocked_range<size_t> &range, tbb::pre_scan_tag) {
			for (size_t r = range.begin(); r < range.end(); ++r)
				count += isRoot[r];
		}

		void operator()(const tbb::blocked_range<size_t> &range, tbb::final_scan_tag) {
			for (size_t r = range.begin(); r < range.end(); ++r) {
				classIdOfRoot[r] = static_cast<uint32_t>(count);
				count += isRoot[r];
			}
		}

		void reverse_join(DenseClassIdScan &rhs) {
			count += rhs.count;
		}

		void assign(DenseClassIdScan &rhs) {
			count = rhs.count;
		}

		size_t getCount() const {
			return count;
		}
	};

	ScalableVector<uint32_t> classIdOfRoot(numElements);
	DenseClassIdScan classIdScan(classIdOfRoot, isRoot);
	tbb::parallel_scan(tbb::blocked_range<size_t>(0, numElements, 16384), classIdScan);
	const size_t classCount = classIdScan.getCount();

	// The scatter of the elements into the class segments: the fixed
	// chunk boundary pattern over the dense class ids (count, prefix,
	// disjoint scatter). The chunks cover ascending element ranges
	// and scatter in ascending order, so every class receives its
	// elements in the ascending order of the serial fill - no atomic
	// and no post sort (the histogram matrix would be GBs over the
	// root ids, it stays in the MBs over the dense ones). The small
	// element counts fall back to a single chunk: the serial scatter
	const size_t classChunkCount = (numElements < 262144) ? 1 :
			std::min<size_t>(64, std::max<size_t>(1,
					(size_t(1) << 24) / (classCount + 1)));
	const size_t classChunkSize = (numElements + classChunkCount - 1) / classChunkCount;

	// The map to the dense ids and the per chunk histograms, in one
	// pass (the roots of the elements are rewritten in place: only
	// the class ids are needed from here on)
	ScalableVector<uint32_t> chunkCounts(classChunkCount * classCount, 0);
	tbb::parallel_for(size_t(0), classChunkCount, [&](size_t k) {
		const size_t iBegin = k * classChunkSize;
		const size_t iEnd = std::min(iBegin + classChunkSize, numElements);

		uint32_t * const counts = chunkCounts.data() + k * classCount;
		for (size_t i = iBegin; i < iEnd; ++i) {
			const uint32_t classId = classIdOfRoot[rootOfElement[i]];

			rootOfElement[i] = classId;
			++counts[classId];
		}
	});

	// The per class totals
	ScalableVector<size_t> classStart(classCount + 1, 0);
	tbb::parallel_for(tbb::blocked_range<size_t>(0, classCount, 4096),
			[&](const tbb::blocked_range<size_t> &w) {
		for (size_t c = w.begin(); c < w.end(); ++c) {
			size_t total = 0;
			for (size_t k = 0; k < classChunkCount; ++k)
				total += chunkCounts[k * classCount + c];
			classStart[c + 1] = total;
		}
			});
	for (size_t c = 0; c < classCount; ++c)
		classStart[c + 1] += classStart[c];

	// The cross chunk prefix, in place: the histogram rows become
	// the per chunk scatter cursors
	tbb::parallel_for(tbb::blocked_range<size_t>(0, classCount, 4096),
			[&](const tbb::blocked_range<size_t> &w) {
		ScalableVector<uint32_t> running(w.size());
		for (size_t c = w.begin(); c < w.end(); ++c)
			running[c - w.begin()] = static_cast<uint32_t>(classStart[c]);

		for (size_t k = 0; k < classChunkCount; ++k) {
			uint32_t * const row = chunkCounts.data() + k * classCount;
			for (size_t c = w.begin(); c < w.end(); ++c) {
				const uint32_t count = row[c];

				row[c] = running[c - w.begin()];
				running[c - w.begin()] += count;
			}
		}
			});

	// The scatter itself
	ScalableVector<size_t> classEntries(numElements);
	tbb::parallel_for(size_t(0), classChunkCount, [&](size_t k) {
		const size_t iBegin = k * classChunkSize;
		const size_t iEnd = std::min(iBegin + classChunkSize, numElements);

		uint32_t * const cursor = chunkCounts.data() + k * classCount;
		for (size_t i = iBegin; i < iEnd; ++i)
			classEntries[cursor[rootOfElement[i]]++] = i;
	});

	// Slice the entries into the output classes (the slot ranges are
	// disjoint)
	slg::Classes classes(classCount);
	tbb::parallel_for(size_t(0), classCount, [&](size_t c) {
		classes[c].assign(
				std::make_move_iterator(classEntries.begin() + classStart[c]),
				std::make_move_iterator(classEntries.begin() + classStart[c + 1]));
	});

	return classes;
}

// Grain for the parallel_reduce of the equivalence grouping: every reduce
// body constructs a whole UnionFind over the element space (an identity
// parent array of numElements entries, a cost independent of the body
// range), so the body count must stay in the vicinity of the thread count
// or the constructions dominate the grouping (a fine grain makes TBB
// create hundreds of bodies, each paying the full construction)
size_t GroupingGrain(const size_t iterationCount) {
	return std::max<size_t>(1024,
		iterationCount / size_t(2 * tbb::this_task_arena::max_concurrency()));
}

}  // namespace

namespace slg {

// Version for callable generators over an iteration space distinct from
// the element space: the relation functor is invoked with subranges of
// [0, iterationCount) and returns pairs of equivalent elements of
// [0, numElements). E.g. the relations can be derived from a mesh
// triangle range while the elements are the (fewer) mesh entities
// referenced by the relations
Classes GroupByEquivalence(size_t numElements, size_t iterationCount,
	RelationFunction relation) {
	// Use parallel_reduce with ParallelGroupByEquivalenceFromGenerator
	static tbb::affinity_partitioner tbb_partitioner;
	const size_t grain = GroupingGrain(iterationCount);

	ParallelGroupByEquivalenceFromGenerator<RelationFunction> solver(
		numElements, std::move(relation));

	tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, iterationCount, grain),
		solver,
		tbb_partitioner
	);

	return BuildClassesFromUnionFind(solver.getResult(), numElements);
}

// Version for direct ranges: relation is a span of Relation pairs
Classes GroupByEquivalence(size_t numElements, RelationSpan relation) {
	// Use parallel_reduce with ParallelGroupByEquivalence
	static tbb::affinity_partitioner tbb_partitioner;

	const size_t relationSize = relation.size();
	const size_t grain = GroupingGrain(relationSize);

	ParallelGroupByEquivalence<RelationSpan> solver(
		numElements, std::move(relation));

	tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, relationSize, grain),
		solver,
		tbb_partitioner
	);

	return BuildClassesFromUnionFind(solver.getResult(), numElements);
}


}  // namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
