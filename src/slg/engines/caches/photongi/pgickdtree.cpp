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

#include "slg/engines/caches/photongi/pgickdtree.h"
#include "slg/engines/caches/photongi/photongicache.h"

#include <boost/serialization/export.hpp>


using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PGCIKdTree
//------------------------------------------------------------------------------

PGICKdTree::PGICKdTree(const vector<PGICVisibilityParticle> & entries) :
		IndexKdTree(entries) {
}

size_t PGICKdTree::GetNearestEntry(
		const Point &p, const Normal &n, const bool isVolume,
		const float radius2, const float normalCosAngle) const {
	const int stackSize = 128;

	size_t nodeIndexStack[stackSize];

	int stackCurrentIndex = 0;
	nodeIndexStack[stackCurrentIndex] = 0;

	size_t nearestEntryIndex = NULL_INDEX;
	float nearestMaxDistance2 = radius2;
	while (stackCurrentIndex >= 0) {
		// Pop the current node form the stack
		const size_t currentNodeIndex = nodeIndexStack[stackCurrentIndex--];

		const IndexKdTreeArrayNode &node = arrayNodes[currentNodeIndex];

		const size_t axis = KdTreeNodeData_GetAxis(node.nodeData);

		// Add check of the children if it is not a leaf
		if (axis != 3) {
			const float distance2 = Sqr(p[axis] - node.splitPos);

			if (p[axis] <= node.splitPos) {
				if (KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}

				const size_t rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if ((distance2 < nearestMaxDistance2) && (rightChildIndex != KdTreeNodeData_NULL_INDEX)) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}
			} else {
				const size_t rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if (rightChildIndex != KdTreeNodeData_NULL_INDEX) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}

				if ((distance2 < nearestMaxDistance2) && KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}
			}
		}

		// Check the current node
		const PGICVisibilityParticle &entry = allEntries[node.index];
		const float distance2 = DistanceSquared(entry.p, p);
		if ((distance2 < nearestMaxDistance2) && (entry.isVolume == isVolume) &&
					(isVolume || (Dot(n, entry.n) > normalCosAngle))) {
			// I have found a valid entry

			nearestEntryIndex = node.index;
			nearestMaxDistance2 = distance2;
		}
	}

	return nearestEntryIndex;
}

void PGICKdTree::GetAllNearEntries(std::vector<size_t> &allNearEntryIndices,
		const Point &p, const Normal &n, const bool isVolume,
		const float radius2, const float normalCosAngle) const {
	const int stackSize = 128;

	size_t nodeIndexStack[stackSize];

	int stackCurrentIndex = 0;
	nodeIndexStack[stackCurrentIndex] = 0;

	while (stackCurrentIndex >= 0) {
		// Pop the current node form the stack
		const size_t currentNodeIndex = nodeIndexStack[stackCurrentIndex--];

		const IndexKdTreeArrayNode &node = arrayNodes[currentNodeIndex];

		const size_t axis = KdTreeNodeData_GetAxis(node.nodeData);

		// Add check of the children if it is not a leaf
		if (axis != 3) {
			const float distance2 = Sqr(p[axis] - node.splitPos);

			if (p[axis] <= node.splitPos) {
				if (KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}

				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if ((distance2 < radius2) && (rightChildIndex != KdTreeNodeData_NULL_INDEX)) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}
			} else {
				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if (rightChildIndex != KdTreeNodeData_NULL_INDEX) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}

				if ((distance2 < radius2) && KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;

					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}
			}
		}

		// Check the current node
		const PGICVisibilityParticle &entry = allEntries[node.index];
		const float distance2 = DistanceSquared(entry.p, p);
		if ((distance2 < radius2) && (entry.isVolume == isVolume) &&
					(isVolume || (Dot(n, entry.n) > normalCosAngle))) {
			// I have found a valid entry

			allNearEntryIndices.push_back(node.index);
		}
	}
}


template<class Archive>
void PGICKdTree::serialize(Archive &ar, unsigned int version)
{}


namespace boost { namespace serialization {

template<class Archive>
void save_construct_data(
    Archive & ar, const slg::PGICKdTree * t, unsigned int file_version
){
    // save data required to construct instance
    ar << & t->GetAllEntries();
}

template<class Archive>
void load_construct_data(
    Archive & ar,
	slg::PGICKdTree * t,
	unsigned int file_version
) {
    // retrieve data from archive required to construct new instance
	const std::vector<slg::PGICVisibilityParticle> * entries;
    ar >> entries;
    // invoke inplace constructor to initialize instance of my_class
    ::new(t)slg::PGICKdTree(*entries);
}

} }  // namespace boost::serialization

// Explicit instanciations
template
void boost::serialization::save_construct_data<>(
	boost::archive::binary_oarchive&, slg::PGICKdTree const*, unsigned int
);

template
void boost::serialization::save_construct_data<>(
	boost::archive::text_oarchive&, slg::PGICKdTree const*, unsigned int
);

template
void boost::serialization::load_construct_data<>(
	boost::archive::binary_iarchive&, slg::PGICKdTree*, unsigned int
);

template
void boost::serialization::load_construct_data<>(
	boost::archive::text_iarchive&, slg::PGICKdTree*, unsigned int
);

template
void slg::PGICKdTree::serialize<>(boost::archive::text_oarchive&, unsigned int);

template
void slg::PGICKdTree::serialize<>(boost::archive::binary_oarchive&, unsigned int);

template
void slg::PGICKdTree::serialize<>(boost::archive::text_iarchive&, unsigned int);

template
void slg::PGICKdTree::serialize<>(boost::archive::binary_iarchive&, unsigned int);

BOOST_CLASS_EXPORT(slg::PGICKdTree)
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
