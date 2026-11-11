/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                   *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_SUBDIV_CORNER_DATA_H
#define	_SLG_SUBDIV_CORNER_DATA_H

#include <vector>

#include "luxrays/core/exttrianglemesh.h"

namespace slg {

// Per-corner (face-varying) varying data of a (subdivided) triangle mesh: one
// value per triangle corner (3 * triangle count per layer). Used to carry
// UV / color / alpha values through subdivision while preserving the seams
// between submeshes (a shared point can have a different value per adjacent
// submesh, which a per-point array cannot represent).
struct SubdivCornerData {
	std::vector<std::vector<luxrays::UV>> uvLayers;        // per UV layer
	std::vector<std::vector<luxrays::Spectrum>> colorLayers;  // per color layer
	std::vector<std::vector<float>> alphaLayers;           // per alpha layer
};

}

#endif	/* _SLG_SUBDIV_CORNER_DATA_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
