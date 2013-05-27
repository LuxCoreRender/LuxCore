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

#include <algorithm>

#include "slg/slg.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathhybrid/pathhybrid.h"

#include "luxrays/core/intersectiondevice.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathState
//------------------------------------------------------------------------------

//static inline float MIS(const float a) {
//	//return a; // Balance heuristic
//	return a * a; // Power heuristic
//}

const unsigned int sampleEyeBootSize = 6;
const unsigned int sampleEyeStepSize = 11;

PathState::PathState(PathHybridRenderThread *renderThread,
		Film *film, luxrays::RandomGenerator *rndGen) : HybridRenderState(renderThread, film, rndGen), sampleResults(1) {
	PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)renderThread->renderEngine;

	sampleResults[0].type = PER_PIXEL_NORMALIZED;

	// Setup the sampler
	const unsigned int sampleSize = 
		sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize; // For each eye path and eye vertex
	sampler->RequestSamples(sampleSize);
}

void PathState::GenerateRays(HybridRenderThread *renderThread) {
	PathHybridRenderThread *thread = (PathHybridRenderThread *)renderThread;
	PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = thread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	sampleResults[0].screenX = std::min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
	sampleResults[0].screenY = std::min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
	Ray eyeRay;
	camera->GenerateRay(sampleResults[0].screenX, sampleResults[0].screenY, &eyeRay,
		sampler->GetSample(2), sampler->GetSample(3));

	thread->PushRay(eyeRay);
}

double PathState::CollectResults(HybridRenderThread *renderThread) {
	PathHybridRenderThread *thread = (PathHybridRenderThread *)renderThread;
	
	const Ray *ray;
	const RayHit *rayHit;
	thread->PopRay(&ray, &rayHit);

	if (rayHit->Miss()) {
		sampleResults[0].radiance = Spectrum();
		sampleResults[0].alpha = 1.f;
	} else {
		sampleResults[0].radiance = Spectrum(1.f);
		sampleResults[0].alpha = 0.f;
	}

	sampler->NextSample(sampleResults);

	return 1.0;
}
