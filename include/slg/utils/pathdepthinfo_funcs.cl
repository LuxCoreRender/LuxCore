#line 2 "pathinfo_funcs.cl"

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

//------------------------------------------------------------------------------
// PathDepthInfo
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void PathDepthInfo_Init(__global PathDepthInfo *depthInfo) {
	depthInfo->depth = 0;
	depthInfo->diffuseDepth = 0;
	depthInfo->glossyDepth = 0;
	depthInfo->specularDepth = 0;
}

OPENCL_FORCE_INLINE void PathDepthInfo_IncDepths(__global PathDepthInfo *depthInfo, const BSDFEvent event) {
	++(depthInfo->depth);
	if (event & DIFFUSE)
		++(depthInfo->diffuseDepth);
	if (event & GLOSSY)
		++(depthInfo->glossyDepth);
	if (event & SPECULAR)
		++(depthInfo->specularDepth);
}

OPENCL_FORCE_INLINE bool PathDepthInfo_IsLastPathVertex(__global PathDepthInfo *depthInfo,
		__constant PathDepthInfo *maxDepthInfo, const BSDFEvent event) {
	return (depthInfo->depth + 1 >= maxDepthInfo->depth) ||
			((event & DIFFUSE) && (depthInfo->diffuseDepth + 1 >= maxDepthInfo->diffuseDepth)) ||
			((event & GLOSSY) && (depthInfo->glossyDepth + 1 >= maxDepthInfo->glossyDepth)) ||
			((event & SPECULAR) && (depthInfo->specularDepth + 1 >= maxDepthInfo->specularDepth));
}

OPENCL_FORCE_INLINE bool PathDepthInfo_CheckComponentDepths(__constant PathDepthInfo *maxDepthInfo,
		const BSDFEvent component) {
	return ((maxDepthInfo->diffuseDepth > 0) && (component & DIFFUSE)) ||
			((maxDepthInfo->glossyDepth > 0) && (component & GLOSSY)) ||
			((maxDepthInfo->specularDepth > 0) && (component & SPECULAR));
}

OPENCL_FORCE_INLINE uint PathDepthInfo_GetRRDepth(__global PathDepthInfo *depthInfo) {
	return depthInfo->diffuseDepth + depthInfo->glossyDepth;
}
