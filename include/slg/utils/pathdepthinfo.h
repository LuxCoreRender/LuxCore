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

#ifndef _SLG_PATHDEPTHINFO_H
#define	_SLG_PATHDEPTHINFO_H

#include <ostream>

#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/utils/pathdepthinfo_types.cl"
}

class PathDepthInfo {
public:
	PathDepthInfo();
	~PathDepthInfo() { }

	void IncDepths(const BSDFEvent event);
	bool IsLastPathVertex(const PathDepthInfo &maxPathDepth,
		const BSDFEvent event) const;
	u_int GetRRDepth() const;

	u_int depth, diffuseDepth, glossyDepth, specularDepth;
};

inline std::ostream &operator<<(std::ostream &os, const PathDepthInfo &pdi) {
	os << "PathDepthInfo[" << pdi.depth << ", " << pdi.diffuseDepth << ", " << pdi.glossyDepth << ", " << pdi.specularDepth << "]";
	return os;
}

}

#endif	/* _SLG_PATHDEPTHINFO_H */
