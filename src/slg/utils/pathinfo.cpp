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

#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathInfo
//------------------------------------------------------------------------------

PathInfo::PathInfo() : lastBSDFEvent(SPECULAR), // SPECULAR is required to avoid MIS
		isNearlyS(false), isNearlySD(false), isNearlySDS(false) {
}

bool PathInfo::UseRR(const u_int rrDepth) const {
	return !(lastBSDFEvent & SPECULAR) && (depth.GetRRDepth() >= rrDepth);
}

bool PathInfo::IsNearlySpecular(const BSDFEvent event,
		const float glossiness, const float glossinessThreshold) {
	return (event & SPECULAR) || ((event & GLOSSY) && (glossiness <= glossinessThreshold));
}

bool PathInfo::CanBeNearlySpecular(const BSDF &bsdf, const float glossinessThreshold) {
	const BSDFEvent eventTypes = bsdf.GetEventTypes();

	return (eventTypes & SPECULAR) || ((eventTypes & GLOSSY) && (bsdf.GetGlossiness() <= glossinessThreshold));
}

//------------------------------------------------------------------------------
// EyePathInfo
//------------------------------------------------------------------------------

EyePathInfo::EyePathInfo() : isPassThroughPath(true),
		lastBSDFPdfW(1.f), lastGlossiness(0.f), lastFromVolume(false),
		isTransmittedPath(true), isNearlyCaustic(false) {
}

void EyePathInfo::AddVertex(const BSDF &bsdf,
		const BSDFEvent event, const float pdfW,
		const float glossinessThreshold) {
	//--------------------------------------------------------------------------
	// PathInfo::AddVertex() inlined here for performances
	//--------------------------------------------------------------------------

	// Increment path depth information
	depth.IncDepths(event);
	
	// Update volume information
	volume.Update(event, bsdf);

	const float glossiness = bsdf.GetGlossiness();
	const bool isNewVertexNearlySpecular = IsNearlySpecular(event, glossiness, glossinessThreshold);

	// Update isNearlySDS (must be done before isNearlySD)
	isNearlySDS = (isNearlySD || isNearlySDS) && isNewVertexNearlySpecular;

	// Update isNearlySD (must be done before isNearlyS)
	isNearlySD = isNearlyS && !isNewVertexNearlySpecular;

	// Update isNearlySpecular
	isNearlyS = ((depth.depth == 1) || isNearlyS) && isNewVertexNearlySpecular;

	// Update last path vertex information
	lastBSDFEvent = event;

	//--------------------------------------------------------------------------
	// EyePathInfo::AddVertex()
	//--------------------------------------------------------------------------

	// Update isNearlyCaustic
	//
	// Note: depth.depth has been already incremented by 1 with the depth.IncDepths(event);
	isNearlyCaustic = (depth.depth == 1) ?
		// First vertex must a nearly diffuse
		(!isNewVertexNearlySpecular) :
		// All other vertices must be nearly specular
		(isNearlyCaustic && isNewVertexNearlySpecular);
	
	// Update last path vertex information
	lastBSDFPdfW = pdfW;
	lastShadeN = bsdf.hitPoint.intoObject ? bsdf.hitPoint.shadeN : -bsdf.hitPoint.shadeN;
	lastFromVolume =  bsdf.IsVolume();
	lastGlossiness = glossiness;

	isTransmittedPath = isTransmittedPath && (event & TRANSMIT) && (event & (SPECULAR | GLOSSY));
}

bool EyePathInfo::IsCausticPath(const BSDFEvent event,
		const float glossiness, const float glossinessThreshold) const {
	// Note: the +1 is there for the event passed as method arguments
	return isNearlyCaustic && (depth.depth + 1 > 1) &&
			IsNearlySpecular(event, glossiness, glossinessThreshold);
}

//------------------------------------------------------------------------------
// LightPathInfo
//------------------------------------------------------------------------------

LightPathInfo::LightPathInfo() {
}

void LightPathInfo::AddVertex(const BSDF &bsdf, const BSDFEvent event,
		const float glossinessThreshold) {
	//--------------------------------------------------------------------------
	// PathInfo::AddVertex() inlined here for performances
	//--------------------------------------------------------------------------

	// Increment path depth information
	depth.IncDepths(event);
	
	// Update volume information
	volume.Update(event, bsdf);

	const float glossiness = bsdf.GetGlossiness();
	const bool isNewVertexNearlySpecular = IsNearlySpecular(event, glossiness, glossinessThreshold);

	// Update isNearlySDS (must be done before isNearlySD)
	isNearlySDS = (isNearlySD || isNearlySDS) && isNewVertexNearlySpecular;

	// Update isNearlySD (must be done before isNearlyS)
	isNearlySD = isNearlyS && !isNewVertexNearlySpecular;

	// Update isNearlySpecular
	isNearlyS = ((depth.depth == 1) || isNearlyS) && isNewVertexNearlySpecular;

	// Update last path vertex information
	lastBSDFEvent = event;
}

bool LightPathInfo::IsCausticPath(const BSDFEvent event,
		const float glossiness, const float glossinessThreshold) const {
	// Note: the +1 is there for the event passed as method arguments
	return isNearlyS && (depth.depth + 1 > 1) &&
			!IsNearlySpecular(event, glossiness, glossinessThreshold);
}
