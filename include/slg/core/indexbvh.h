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

#ifndef __SLG_INDEXBVH_H
#define	__SLG_INDEXBVH_H

#include <vector>

#include "luxrays/core/bvh/bvhbuild.h"
#include "luxrays/utils/serializationutils.h"

#include "slg/slg.h"

namespace slg {

//------------------------------------------------------------------------------
// Index BVH
//------------------------------------------------------------------------------

template <class T>
class IndexBvh {
public:
	IndexBvh(const std::vector<T> *entries, const float entryRadius);
	virtual ~IndexBvh();

	float GetEntryRadius() const { return entryRadius; }
	size_t GetMemoryUsage() const { return nNodes * sizeof(luxrays::ocl::IndexBVHArrayNode); }
	
	const luxrays::ocl::IndexBVHArrayNode *GetArrayNodes(u_int *count = nullptr) const {
		if (count)
			*count = nNodes;
		return arrayNodes;
	}

	friend class boost::serialization::access;

protected:
	// Used by serialization
	IndexBvh();

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & allEntries;
		ar & entryRadius;
		ar & entryRadius2;

		ar & nNodes;
		ar & boost::serialization::make_array<luxrays::ocl::IndexBVHArrayNode>(arrayNodes, nNodes);		
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & allEntries;
		ar & entryRadius;
		ar & entryRadius2;

		ar & nNodes;
		arrayNodes = new luxrays::ocl::IndexBVHArrayNode[nNodes];
		ar & boost::serialization::make_array<luxrays::ocl::IndexBVHArrayNode>(arrayNodes, nNodes);
	}
	
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	const std::vector<T> *allEntries;
	float entryRadius, entryRadius2;

	luxrays::ocl::IndexBVHArrayNode *arrayNodes;
	u_int nNodes;
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::IndexBvh)

//------------------------------------------------------------------------------
// luxrays::ocl::IndexBVHArrayNode serialization
//------------------------------------------------------------------------------

BOOST_SERIALIZATION_SPLIT_FREE(luxrays::ocl::IndexBVHArrayNode)

namespace boost {
namespace serialization {

template<class Archive>
void save(Archive &ar, const luxrays::ocl::IndexBVHArrayNode &node, const unsigned int version) {
	ar & node.nodeData;

	if (IndexBVHNodeData_IsLeaf(node.nodeData))
		ar & node.entryLeaf.entryIndex;
	else {
		ar & node.bvhNode.bboxMin[0];
		ar & node.bvhNode.bboxMin[1];
		ar & node.bvhNode.bboxMin[2];

		ar & node.bvhNode.bboxMax[0];
		ar & node.bvhNode.bboxMax[1];
		ar & node.bvhNode.bboxMax[2];
	}
}

template<class Archive>
void load(Archive &ar, luxrays::ocl::IndexBVHArrayNode &node, const unsigned int version) {
	ar & node.nodeData;

	if (IndexBVHNodeData_IsLeaf(node.nodeData))
		ar & node.entryLeaf.entryIndex;
	else {
		ar & node.bvhNode.bboxMin[0];
		ar & node.bvhNode.bboxMin[1];
		ar & node.bvhNode.bboxMin[2];

		ar & node.bvhNode.bboxMax[0];
		ar & node.bvhNode.bboxMax[1];
		ar & node.bvhNode.bboxMax[2];
	}
}

}
}

BOOST_CLASS_VERSION(luxrays::ocl::IndexBVHArrayNode, 1)
		
#endif	/* __SLG_INDEXBVH_H */
