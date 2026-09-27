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
#include <numeric>
#include <ranges>
#include <vector>
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
// The parent and rank entries are uint32_t/u_char (the elements are mesh
// scale indices, below 2^32): the parent array is randomly touched by
// every find, so shrinking it (and the rank, zero filled at every reduce
// body construction) reduces both the cache footprint of the searches
// and the cost of the per body construction (the parent is iota filled).
//
// The elements that became non roots are logged at link time: a reduce
// join then merges through the logged elements only (every non root
// became so at exactly one link, path halving never turns a root into a
// non root), instead of scanning all the elements.
class UnionFind {
	ScalableVector<uint32_t> parent;
	ScalableVector<u_char> rank;
	// The elements that became non roots (one entry per effective link)
	ScalableVector<uint32_t> nonRoots;

public:
	UnionFind() = default;
	explicit UnionFind(const size_t count) : parent(count), rank(count, 0) {
		std::iota(parent.begin(), parent.end(), 0u);
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
slg::Classes BuildClassesFromUnionFind(UnionFind& uf, const size_t numElements) {
	// Find the root of every element (flattening the trees along the way)
	// and count the class sizes in the same pass
	ScalableVector<size_t> classStart(numElements + 1, 0);
	for (size_t i = 0; i < numElements; ++i)
		++classStart[uf.find(i) + 1];

	// Prefix sum
	for (size_t i = 0; i < numElements; ++i)
		classStart[i + 1] += classStart[i];

	// Fill the class entries (the trees are flattened: each find is a
	// parent lookup or two)
	ScalableVector<size_t> classEntries(numElements);
	{
		ScalableVector<size_t> classCursor(classStart.begin(), classStart.end() - 1);
		for (size_t i = 0; i < numElements; ++i)
			classEntries[classCursor[uf.find(i)]++] = i;
	}

	// Slice the entries into the output classes
	slg::Classes classes;
	for (size_t r = 0; r < numElements; ++r) {
		if (classStart[r + 1] > classStart[r])
			classes.emplace_back(
					std::make_move_iterator(classEntries.begin() + classStart[r]),
					std::make_move_iterator(classEntries.begin() + classStart[r + 1]));
	}

	return classes;
}

}  // namespace

namespace slg {

// Version for callable generators: relation is a functor that takes an
// interval [r1, r2) with r1 and r2 of size_t type and returns a vector of
// Relation. Functor is evaluated in multithreaded process, which may be more
// efficient than statically compute it beforehand
Classes GroupByEquivalence(size_t numElements, RelationFunction relation) {
	// Use parallel_reduce with ParallelGroupByEquivalenceFromGenerator
	static tbb::affinity_partitioner tbb_partitioner;
	constexpr size_t grain = 1024;

	ParallelGroupByEquivalenceFromGenerator<RelationFunction> solver(
		numElements, std::move(relation));

	tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, numElements, grain),
		solver,
		tbb_partitioner
	);

	return BuildClassesFromUnionFind(solver.getResult(), numElements);
}

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
	constexpr size_t grain = 1024;

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

	// Determine grain size based on relation size
	const size_t relationSize = relation.size();
	constexpr size_t grain = 1024;

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
