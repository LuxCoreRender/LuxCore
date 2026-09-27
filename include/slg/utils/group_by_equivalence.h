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

#pragma once

#include <vector>
#include <utility>
#include <ranges>
#include <functional>
#include <span>
#include <oneapi/tbb.h>

namespace slg {

using Classes = std::vector<std::vector<size_t>>;
using Relation = std::pair<size_t, size_t>;
using RelationSpan = std::span<const Relation>;
// Vector of relations, as returned by the generators, allocated with the
// TBB scalable allocator: the generator is evaluated in parallel (one call
// per chunk) and the returned buffer is consumed once by the Union-Find,
// so these short-lived allocations are served from the per-thread caches
// of tbbmalloc
using RelationVector = std::vector<Relation, tbb::scalable_allocator<Relation>>;
using RelationFunction = std::function<RelationVector(size_t, size_t)>;

// GroupByEquivalence computes the quotient set (equivalence classes) from an
// equivalence relation.
// Given a set of elements [0, numElements) and a relation (pairs of equivalent
// elements), it returns a Classes object where each inner vector contains all
// elements that are equivalent under the transitive closure of the relation.
//
// In other words, if relation contains pairs indicating which elements are
// equivalent, GroupByEquivalence groups all elements into disjoint classes
// where each class contains all elements that are transitively equivalent.
//
// Each class contains its elements in ascending order.
//
// The function relies on a parallel implementation of the Union-Find algorithm
// (using TBB) and should be fast for large equivalence relations.
//
// Two overloads are provided:
// 1. For callable generators: GroupByEquivalence(numElements, relation)
//    where relation is a RelationFunction (a std::function taking an
//    interval [r1, r2) and returning a cache-aligned vector of pairs)
// 2. For direct ranges: GroupByEquivalence(numElements, relation)
//    where relation is a RelationSpan (std::span<const Relation>)
//
// Usage examples:
//   // With a callable generator (functor or lambda)
//   auto relation = [&](size_t r1, size_t r2) {
//       slg::RelationVector pairs;
//       for (size_t i = r1; i < r2; ++i) {
//           if (condition(i)) pairs.emplace_back(i, i+1);
//       }
//       return pairs;
//   };
//   auto clusters = GroupByEquivalence(n, relation);
//
//   // With std::span
//   std::vector<std::pair<size_t, size_t>> pairs = {{0,1}, {2,3}};
//   auto clusters = GroupByEquivalence(n, std::span<const Relation>(pairs));
//
//   // Note: std::vector<Relation> is implicitly convertible to RelationSpan

// Version for callable generators: relation is a functor that takes an
// interval [r1, r2) with r1 and r2 of size_t type and returns a vector of
// Relation. Functor is evaluated in multithreaded process, which may be more
// efficient than statically compute it beforehand
Classes GroupByEquivalence(size_t numElements, RelationFunction relation);

// Version for callable generators over an iteration space distinct from
// the element space: the relation functor is invoked with subranges of
// [0, iterationCount) and returns pairs of equivalent elements of
// [0, numElements). E.g. the relations can be derived from a mesh
// triangle range while the elements are the (fewer) mesh entities
// referenced by the relations
Classes GroupByEquivalence(size_t numElements, size_t iterationCount,
	RelationFunction relation);

// Version for direct ranges: relation is a span of Relation pairs
Classes GroupByEquivalence(size_t numElements, RelationSpan relation);
}  // namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
