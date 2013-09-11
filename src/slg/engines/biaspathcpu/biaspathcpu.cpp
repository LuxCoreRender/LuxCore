/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "slg/engines/biaspathcpu/biaspathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathCPURenderEngine
//------------------------------------------------------------------------------

BiasPathCPURenderEngine::BiasPathCPURenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUTileRenderEngine(rcfg, flm, flmMutex) {
	film->SetPerPixelNormalizedBufferFlag(true);
	film->SetPerScreenNormalizedBufferFlag(false);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->Init();
}

void BiasPathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	enableProgressiveRefinement = cfg.GetBoolean("biaspath.progressiverefinement.enable", false);

	// Path depth settings
	maxPathDepth.depth = Max(1, cfg.GetInt("biaspath.pathdepth.total", 10));
	maxPathDepth.diffuseDepth = Max(0, cfg.GetInt("biaspath.pathdepth.diffuse", 1));
	maxPathDepth.glossyDepth = Max(0, cfg.GetInt("biaspath.pathdepth.glossy", 1));
	maxPathDepth.reflectionDepth = Max(0, cfg.GetInt("biaspath.pathdepth.reflection", 2));
	maxPathDepth.refractionDepth = Max(0, cfg.GetInt("biaspath.pathdepth.refraction", 2));

	// Samples settings
	aaSamples = Max(1, cfg.GetInt("biaspath.sampling.aa.size", 3));
	totalSamplesPerPixel = aaSamples * aaSamples; // Used for progressive rendering
	diffuseSamples = Max(0, cfg.GetInt("biaspath.sampling.diffuse.size", 2));
	glossySamples = Max(0, cfg.GetInt("biaspath.sampling.glossy.size", 2));
	refractionSamples = Max(0, cfg.GetInt("biaspath.sampling.refraction.size", 2));

	// Clamping settings
	clampValueEnabled = cfg.GetBoolean("biaspath.clamping.enable", true);
	clampMaxValue = Max(0.f, cfg.GetFloat("biaspath.clamping.maxvalue", 10.f));

	// Light settings
	lowLightThreashold = Max(0.f, cfg.GetFloat("biaspath.lights.lowthreshold", .001f));
	nearStartLight = Max(0.f, cfg.GetFloat("biaspath.lights.nearstart", .001f));

	CPUTileRenderEngine::StartLockLess();
}

PathDepthInfo::PathDepthInfo() {
	depth = 0;
	diffuseDepth = 0;
	glossyDepth = 0;
	reflectionDepth = 0;
	refractionDepth = 0;
}

void PathDepthInfo::IncDepths(const BSDFEvent event) {
	++depth;
	if ((event & (DIFFUSE | REFLECT)) == (DIFFUSE | REFLECT))
		++diffuseDepth;
	if ((event & (GLOSSY | REFLECT)) == (GLOSSY | REFLECT))
		++glossyDepth;
	if ((event & (SPECULAR | REFLECT)) == (SPECULAR | REFLECT))
		++reflectionDepth;
	if (event & TRANSMIT)
		++refractionDepth;
}

bool PathDepthInfo::CheckDepths(const PathDepthInfo &maxPathDepth) const {
	if ((depth > maxPathDepth.depth) ||
			(diffuseDepth > maxPathDepth.diffuseDepth) ||
			(glossyDepth > maxPathDepth.glossyDepth) ||
			(reflectionDepth > maxPathDepth.reflectionDepth) ||
			(refractionDepth > maxPathDepth.refractionDepth))
		return false;
	else
		return true;
}
