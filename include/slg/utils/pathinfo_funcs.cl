#line 2 "pathinfo_funcs.cl"

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

//------------------------------------------------------------------------------
// EyePathInfo
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void EyePathInfo_Init(__global EyePathInfo *pathInfo) {
	PathDepthInfo_Init(&pathInfo->depth);
	PathVolumeInfo_Init(&pathInfo->volume);
	
	pathInfo->isPassThroughPath = true;

	pathInfo->lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
	pathInfo->lastBSDFPdfW = 1.f;
	pathInfo->lastGlossiness = 0.f;
	pathInfo->lastFromVolume = false;
	pathInfo->isTransmittedPath = true;

	pathInfo->isNearlyCaustic = false;
	pathInfo->isNearlyS = false;
	pathInfo->isNearlySD = false;
	pathInfo->isNearlySDS = false;
}

OPENCL_FORCE_INLINE bool EyePathInfo_UseRR(__global EyePathInfo *pathInfo, const uint rrDepth) {
	return !(pathInfo->lastBSDFEvent & SPECULAR) &&
			(PathDepthInfo_GetRRDepth(&pathInfo->depth) >= rrDepth);
}

OPENCL_FORCE_INLINE bool EyePathInfo_IsNearlySpecular(__global EyePathInfo *pathInfo,
		const BSDFEvent event, const float glossiness, const float glossinessThreshold) {
	return (event & SPECULAR) || ((event & GLOSSY) && (glossiness <= glossinessThreshold));
}

OPENCL_FORCE_INLINE bool EyePathInfo_CanBeNearlySpecular(__global EyePathInfo *pathInfo,
		__global const BSDF *bsdf, const float glossinessThreshold
		MATERIALS_PARAM_DECL) {
	const BSDFEvent eventTypes = BSDF_GetEventTypes(bsdf MATERIALS_PARAM);

	return (eventTypes & SPECULAR) || ((eventTypes & GLOSSY) && (BSDF_GetGlossiness(bsdf MATERIALS_PARAM) <= glossinessThreshold));
}

OPENCL_FORCE_INLINE void EyePathInfo_AddVertex(__global EyePathInfo *pathInfo,
		__global const BSDF *bsdf, const BSDFEvent event, const float pdfW,
		const float glossinessThreshold
		MATERIALS_PARAM_DECL) {
	//--------------------------------------------------------------------------
	// PathInfo::AddVertex() inlined here for performances
	//--------------------------------------------------------------------------

	// Increment path depth informations
	PathDepthInfo_IncDepths(&pathInfo->depth, event);

	// Update volume information
	PathVolumeInfo_Update(&pathInfo->volume, event, bsdf MATERIALS_PARAM);

	const float glossiness = BSDF_GetGlossiness(bsdf MATERIALS_PARAM);
	const bool isNewVertexNearlySpecular = EyePathInfo_IsNearlySpecular(pathInfo,
			event, glossiness, glossinessThreshold);

	// Update isNearlySDS (must be done before isNearlySD)
	pathInfo->isNearlySDS = (pathInfo->isNearlySD || pathInfo->isNearlySDS) && isNewVertexNearlySpecular;

	// Update isNearlySD (must be done before isNearlyS)
	pathInfo->isNearlySD = pathInfo->isNearlyS && !isNewVertexNearlySpecular;

	// Update isNearlySpecular
	pathInfo->isNearlyS = ((pathInfo->depth.depth == 1) || pathInfo->isNearlyS) && isNewVertexNearlySpecular;

	// Update last path vertex information
	pathInfo->lastBSDFEvent = event;

	//--------------------------------------------------------------------------
	// EyePathInfo::AddVertex()
	//--------------------------------------------------------------------------

	// Update 
	//
	// Note: depth.depth has been already incremented by 1 with the depth.IncDepths(event);
	pathInfo->isNearlyCaustic = (pathInfo->depth.depth == 1) ? 
		// First vertex must a nearly diffuse
		(!isNewVertexNearlySpecular) :
		// All other vertices must be nearly specular
		(pathInfo->isNearlyCaustic && isNewVertexNearlySpecular);

	// Update last path vertex information
	pathInfo->lastBSDFPdfW = pdfW;
	const float3 shadeN = VLOAD3F(&bsdf->hitPoint.shadeN.x);
	VSTORE3F(bsdf->hitPoint.intoObject ? shadeN : -shadeN, &pathInfo->lastShadeN.x);
	pathInfo->lastFromVolume =  bsdf->isVolume;
	pathInfo->lastGlossiness = glossiness;
	
	pathInfo->isTransmittedPath = pathInfo->isTransmittedPath && (event & TRANSMIT);
}

OPENCL_FORCE_INLINE bool EyePathInfo_IsCausticPath(__global EyePathInfo *pathInfo) {
	return pathInfo->isNearlyCaustic && (pathInfo->depth.depth > 1);
}

OPENCL_FORCE_INLINE bool EyePathInfo_IsCausticPathWithEvent(__global EyePathInfo *pathInfo,
		const BSDFEvent event, const float glossiness, const float glossinessThreshold) {
	// Note: the +1 is there for the event passed as method arguments
	return pathInfo->isNearlyCaustic && (pathInfo->depth.depth + 1 > 1) &&
			EyePathInfo_IsNearlySpecular(pathInfo, event, glossiness, glossinessThreshold);
}
