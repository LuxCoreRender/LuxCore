/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)
 *
 *   This file is part of LuxCoreRender.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#ifndef _LUXRAYS_OCLHASH_H
#define	_LUXRAYS_OCLHASH_H

#include <cstdio>
#include <string>
#include <vector>

#include "luxrays/utils/utils.h"

namespace luxrays {

// The hash and the parameters to string conversion used for the OpenCL
// kernel cache keys (see oclKernelPersistentCache). They are defined here
// so they can also be used by dependency-free tools: the spirv-baker
// (build-system/spirv) must compute the same keys without linking LuxCore.

inline u_int OCLKernelHashBin(const char *s, const std::size_t size) {
	u_int hash = 0;

	for (u_int i = 0; i < size; ++i) {
		hash += *s++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

inline std::string OCLKernelHashString(const std::string &ss) {
	const u_int hash = OCLKernelHashBin(ss.c_str(), ss.length());

	char buf[9];
	sprintf(buf, "%08x", hash);

	return std::string(buf);
}

inline std::string OCLKernelParamsToString(const std::vector<std::string> &kernelsParameters) {
	std::string result;

	for (auto const &p : kernelsParameters) {
		if (result.length() != 0)
			result += " ";
		result += p;
	}

	return result;
}

}

#endif
