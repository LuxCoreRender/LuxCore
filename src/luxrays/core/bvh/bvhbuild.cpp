/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/bvh/bvhbuild.h"

using namespace std;

namespace luxrays {

void FreeBVH(BVHTreeNode *node) {
	if (node) {
		FreeBVH(node->leftChild);
		FreeBVH(node->rightSibling);
		delete node;
	}
}

u_int CountBVHNodes(BVHTreeNode *node) {
	if (node)
		return 1 + CountBVHNodes(node->leftChild) + CountBVHNodes(node->rightSibling);
	else
		return 0;
}

void PrintBVHNodes(ostream &stream, BVHTreeNode *node) {
	stream << "BVHNode(" << node << ")[" << endl;

	if (node) {
		stream << node->bbox << endl;
		if (node->leftChild) {
			stream << "LeftChild(" << node->leftChild << ")[" << endl;
			PrintBVHNodes(stream, node->leftChild);
			stream << "]" << endl;
		} else
			stream << "LeftChild(NULL)" << endl;

		if (node->rightSibling) {
			stream << "RightSibling(" << node->rightSibling << ")[" << endl;
			PrintBVHNodes(stream, node->rightSibling);
			stream << "]" << endl;
		} else
			stream << "RightSibling(NULL)" << endl;
	}
	
	stream << "]" << endl;
}

}
