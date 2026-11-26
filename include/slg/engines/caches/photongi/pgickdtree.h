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

#include "slg/core/indexkdtree.h"

namespace luxrays {
class Point;
class Normal;
}

namespace slg {

class PGICVisibilityParticle;

class PGICKdTree : public IndexKdTree<PGICVisibilityParticle> {
public:
	PGICKdTree(const std::vector<PGICVisibilityParticle> & allEntries);
	virtual ~PGICKdTree() = default;

	size_t GetNearestEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume, const float radius2, const float normalCosAngle) const;
	void GetAllNearEntries(std::vector<size_t> &allNearEntryIndices,
			const luxrays::Point &p, const luxrays::Normal &n, const bool isVolume,
			const float radius2, const float normalCosAngle) const;

private:
	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive &ar, const unsigned int version);
};

}  // Namespace slg

// Serialization
namespace boost { namespace serialization {

template<class Archive>
void save_construct_data(
	Archive &, const slg::PGICKdTree *, unsigned int
);

template<class Archive>
void load_construct_data(
	Archive &, slg::PGICKdTree *, unsigned int
);

}}  // namespaces

BOOST_CLASS_VERSION(slg::PGICKdTree, 1)

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
