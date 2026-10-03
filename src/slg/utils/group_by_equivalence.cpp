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

#include <ranges>
#include <unordered_map>
#include "oneapi/tbb.h"
#include "oneapi/tbb/cache_aligned_allocator.h"
#include "tsl/robin_map.h"

namespace {

using ClassMap = tsl::robin_map<size_t, std::vector<size_t>>;

// This is the classical Union-Find algorithm,
// in a parallel implementation (tbb powered)
class alignas(std::hardware_destructive_interference_size) UnionFind {
public:
    UnionFind() {}
    explicit UnionFind(const size_t count) {
		reserve(count);
	}
	UnionFind(const UnionFind& other) :
		parent(other.parent, Allocator()),
		rank(other.rank, Allocator())
	{}
	UnionFind(UnionFind&& other) :
		parent(std::move(other.parent)),
		rank(std::move(other.rank))
	{}
	UnionFind& operator=(const UnionFind& other) {
		parent = other.parent;
		rank = other.rank;
		return (*this);
	}
	UnionFind& operator=(UnionFind&& other) {
		parent = std::move(other.parent);
		rank = std::move(other.rank);
		return (*this);
	}

    // Find the root of the set containing element i
    size_t find(const size_t i) {
        if (parent.find(i) == parent.end()) {
            parent[i] = i;
            rank[i] = 0;
        }
        if (parent[i] != i) {
            parent[i] = find(parent[i]);  // Path compression
        }
        return parent[i];
    }

    // Union the sets containing elements i and j
    void unite(const size_t i, const size_t j) {
        const size_t rootI = find(i);
        const size_t rootJ = find(j);

        if (rootI != rootJ) {
            // Union by rank
            if (rank[rootI] > rank[rootJ]) {
                parent[rootJ] = rootI;
            } else if (rank[rootI] < rank[rootJ]) {
                parent[rootI] = rootJ;
            } else {
                parent[rootJ] = rootI;
                rank[rootI]++;
            }
        }
    }

	// Reserve space
	void reserve(const size_t count) {
		parent.reserve(count);
		rank.reserve(count);
	}

    // Overload the += operator to merge two UnionFind instances
    UnionFind operator+=(const UnionFind& other) {
        for (const auto& pair : other.parent) {
            unite(pair.first, pair.second);
        }
        return (*this);
    }

    size_t size() const {
		return parent.size();
	}

	// Find without compression
	size_t find_readonly(const size_t i) const {
		auto res = parent.find(i);
        if (res != parent.end()) {
			return res->second;
		} else {
			return i;
		}
	}

private:
	using Allocator =
		tbb::cache_aligned_allocator<std::pair<const size_t, size_t>>;
	using Hash = std::hash<size_t>;
	using Equal = std::equal_to<size_t>;
    std::unordered_map<size_t, size_t, Hash, Equal, Allocator> parent;
    std::unordered_map<size_t, size_t, Hash, Equal, Allocator> rank;

    friend std::ostream& operator<<(std::ostream& os, const UnionFind& uf);
};

std::ostream& operator<<(std::ostream& os, const UnionFind& uf) {
    os << "Parent: ";
    for (const auto& pair : uf.parent) {
        os << "(" << pair.first << ", " << pair.second << ") ";
    }
    os << "\nRank: ";
    for (const auto& pair : uf.rank) {
        os << "(" << pair.first << ", " << pair.second << ") ";
    }
    return os;
}

// Parallel GroupByEquivalence builder for use with tbb::parallel_reduce
// Processes equivalence relations in parallel using UnionFind
template<typename Range>
class ParallelGroupByEquivalence {
	UnionFind dsu;
	const size_t numPoints;
	Range relation;

public:
	// Constructor (plain)
	ParallelGroupByEquivalence(size_t p_numPoints, Range p_relation)
		: numPoints(p_numPoints),
		  relation(std::move(p_relation)),
		  dsu(p_numPoints)
	{}

	// Constructor (split)
	ParallelGroupByEquivalence(ParallelGroupByEquivalence& x, tbb::split)
		: numPoints(x.numPoints),
		  relation(x.relation),
		  dsu(x.numPoints)
	{}

	// Body: process a range of indices into the relation
	void operator()(const tbb::blocked_range<size_t>& r) {
		auto it = std::ranges::begin(relation);
		std::advance(it, r.begin());
		for (size_t i = r.begin(); i < r.end(); ++i, ++it) {
			const auto& [a, b] = *it;
			dsu.unite(a, b);
		}
	}

	// Reduction: merge two UnionFind instances
	void join(ParallelGroupByEquivalence& rhs) {
		if (dsu.size() < rhs.dsu.size()) {
			std::swap(dsu, rhs.dsu);
		}
		dsu += rhs.dsu;
	}

	// Access the resulting UnionFind
	UnionFind getResult() const {
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
		if (dsu.size() < rhs.dsu.size()) {
			std::swap(dsu, rhs.dsu);
		}
		dsu += rhs.dsu;
	}

	UnionFind getResult() const { return dsu; }
};


// Helper class for building ClassMap from UnionFind using tbb::parallel_reduce
class BuildClassMapFromUnionFind {
	const UnionFind& uf;
	ClassMap classMap;

public:
	BuildClassMapFromUnionFind(const UnionFind& p_uf, size_t reserveSize = 0)
		: uf(p_uf) {
		classMap.reserve(reserveSize);
	}

	BuildClassMapFromUnionFind(BuildClassMapFromUnionFind& x, tbb::split)
		: uf(x.uf) {}

	void operator()(const tbb::blocked_range<size_t>& r) {
		ClassMap localClassMap;
		for (size_t i = r.begin(); i < r.end(); ++i) {
			const size_t root = uf.find_readonly(i);
			localClassMap[root].push_back(i);
		}
		// Merge local into classMap
		for (auto& [root, indices] : localClassMap) {
			auto it = classMap.find(root);
			if (it != classMap.end()) {
				// Need to use non-const access for insert
				classMap[root].insert(
					classMap[root].end(),
					std::make_move_iterator(indices.begin()),
					std::make_move_iterator(indices.end())
				);
			} else {
				classMap[root] = std::move(indices);
			}
		}
	}

	void join(BuildClassMapFromUnionFind& rhs) {
		// Merge rhs.classMap into this->classMap
		for (auto& [root, indices] : rhs.classMap) {
			auto it = classMap.find(root);
			if (it != classMap.end()) {
				// Need to use non-const access for insert
				classMap[root].insert(
					classMap[root].end(),
					std::make_move_iterator(indices.begin()),
					std::make_move_iterator(indices.end())
				);
			} else {
				classMap[root] = std::move(indices);
			}
		}
	}

	const ClassMap& getResult() const { return classMap; }
};

// Helper class for parallel conversion of ClassMap to Classes
class ConvertClassMapToClasses {
	const ClassMap& classMap;
	slg::Classes result;
	std::vector<ClassMap::const_iterator> iters;

public:
	ConvertClassMapToClasses(const ClassMap& p_classMap)
		: classMap(p_classMap) {
		iters.reserve(classMap.size());
		for (auto it = classMap.begin(); it != classMap.end(); ++it) {
			iters.push_back(it);
		}
	}

	ConvertClassMapToClasses(ConvertClassMapToClasses& x, tbb::split)
		: classMap(x.classMap) {}

	void operator()(const tbb::blocked_range<size_t>& r) {
		const size_t start = r.begin();
		const size_t end = r.end();
		for (size_t i = start; i < end; ++i) {
			result.push_back(iters[i]->second);
		}
	}

	void join(ConvertClassMapToClasses& rhs) {
		// Move rhs.result into this->result
		result.insert(
			result.end(),
			std::make_move_iterator(rhs.result.begin()),
			std::make_move_iterator(rhs.result.end())
		);
	}

	slg::Classes getResult() && { return std::move(result); }
};

// Helper function to build Classes from a UnionFind
slg::Classes BuildClassesFromUnionFind(const UnionFind& uf, size_t numElements) {
	// Build class map from UnionFind using parallel_reduce with helper class
	static tbb::affinity_partitioner tbb_class_partitioner;
	constexpr size_t class_grain = 1024;

	BuildClassMapFromUnionFind classMapBuilder(uf,
		numElements / class_grain + 1);
	tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, numElements, class_grain),
		classMapBuilder,
		tbb_class_partitioner
	);
	// Convert ClassMap to Classes using parallel_reduce
	const ClassMap& finalClassMap = classMapBuilder.getResult();

	static tbb::affinity_partitioner tbb_convert_partitioner;

	ConvertClassMapToClasses converter(finalClassMap);
	tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, finalClassMap.size(), class_grain),
		converter,
		tbb_convert_partitioner
	);

	return std::move(converter).getResult();
}

}  // namespace

namespace slg {

// Version for callable generators: relation is a functor that takes an
// interval [r1, r2) with r1 and r2 of size_t type and returns a vector of
// Relation
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

	UnionFind uf = solver.getResult();
	return BuildClassesFromUnionFind(uf, numElements);
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

	UnionFind uf = solver.getResult();
	return BuildClassesFromUnionFind(uf, numElements);
}


}  // namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
