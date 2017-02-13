/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "slg/utils/pathdepthinfo.h"
#include "slg/film/sampleresult.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathDepthInfo
//------------------------------------------------------------------------------

PathDepthInfo::PathDepthInfo() {
	depth = 0;
	diffuseDepth = 0;
	glossyDepth = 0;
	specularDepth = 0;
}

void PathDepthInfo::IncDepths(const BSDFEvent event) {
	++depth;
	if (event & DIFFUSE)
		++diffuseDepth;
	if (event & GLOSSY)
		++glossyDepth;
	if (event & SPECULAR)
		++specularDepth;
}

bool PathDepthInfo::IsLastPathVertex(const PathDepthInfo &maxPathDepth, const BSDFEvent possibleEvents) const {
	return (depth + 1 >= maxPathDepth.depth) ||
			((possibleEvents & DIFFUSE) && (diffuseDepth + 1 >= maxPathDepth.diffuseDepth)) ||
			((possibleEvents & GLOSSY) && (glossyDepth + 1 >= maxPathDepth.glossyDepth)) ||
			((possibleEvents & SPECULAR) && (specularDepth + 1 >= maxPathDepth.specularDepth));
}
