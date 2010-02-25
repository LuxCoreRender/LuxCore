/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#ifndef _PATH_H
#define	_PATH_H

#include "smalllux.h"
#include "trianglemat.h"
#include "scene.h"

class RenderingConfig;

class Path {
public:
	enum PathState {
		EYE_VERTEX, NEXT_VERTEX
	};

	Path(Scene *scene) {
		lightPdf = new float[scene->shadowRayCount];
		lightColor = new Spectrum[scene->shadowRayCount];
		shadowRay = new Ray[scene->shadowRayCount];
		currentShadowRayIndex = new unsigned int[scene->shadowRayCount];
	}

	~Path() {
		delete[] lightPdf;
		delete[] lightColor;
		delete[] shadowRay;
		delete[] currentShadowRayIndex;
	}

	void Init(Scene *scene, Sampler *sampler) {
		throughput = Spectrum(1.f, 1.f, 1.f);
		radiance = Spectrum(0.f, 0.f, 0.f);
		sampler->GetNextSample(&sample);

		scene->camera->GenerateRay(&sample, &pathRay);
		state = EYE_VERTEX;
		depth = 0;
		specularBounce = true;
	}

	void FillRayBuffer(RayBuffer *rayBuffer) {
		currentPathRayIndex = rayBuffer->AddRay(pathRay);
		if (state == NEXT_VERTEX) {
			for (unsigned int i = 0; i < tracedShadowRayCount; ++i)
				currentShadowRayIndex[i] = rayBuffer->AddRay(shadowRay[i]);
		}
	}

	void AdvancePath(Scene *scene, Sampler *sampler, const RayBuffer *rayBuffer,
			SampleBuffer *sampleBuffer) {
		const RayHit *rayHit = rayBuffer->GetRayHit(currentPathRayIndex);

		if ((state == NEXT_VERTEX) && (tracedShadowRayCount > 0)) {
			for (unsigned int i = 0; i < tracedShadowRayCount; ++i) {
				const RayHit *shadowRayHit = rayBuffer->GetRayHit(currentShadowRayIndex[i]);
				if (shadowRayHit->index == 0xffffffffu) {
					// Nothing was hit, light is visible
					radiance += lightColor[i] / lightPdf[i];
				}
			}
		}

		if (rayHit->index == 0xffffffffu) {
			// Hit nothing, terminate the path
			sampleBuffer->SplatSample(&sample, radiance);
			// Restart the path
			Init(scene, sampler);
			return;
		}

		// Calculate next step
		depth++;

		// Check if I have to stop
		if (depth >= scene->maxPathDepth) {
			// Too depth, terminate the path
			sampleBuffer->SplatSample(&sample, radiance);
			// Restart the ray
			Init(scene, sampler);
			return;
		}

		// Something was hit
		const unsigned int currentTriangleIndex = rayHit->index;

		// Get the triangle
		const ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
		const Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

		// Get the material
		Material *triMat = scene->triangleMatirials[currentTriangleIndex];

		// Check if it is a light source
		if (triMat->IsLightSource()) {
			if (specularBounce) {
				const Point hitPoint = pathRay(rayHit->t);

				const TriangleLight *tLight = (TriangleLight *)triMat;
				float lPdf;
				const Spectrum Le = tLight->Le(scene->objects, pathRay.o, hitPoint, &lPdf);

				// Using 0.1 instead of 0.0 to cut down fireflies
				if ((lPdf > 0.1f) && !Le.Black())
					radiance += Le * throughput;
			}

			// Terminate the path
			sampleBuffer->SplatSample(&sample, radiance);
			// Restart the path
			Init(scene, sampler);
			return;
		}

		//----------------------------------------------------------------------
		// Build the shadow rays (if required)
		//----------------------------------------------------------------------

		const Normal N = InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);
		Normal shadeN;
		float RdotShadeN = Dot(pathRay.d, N);

		// Flip shade  normal if required
		if (RdotShadeN > 0.f) {
			// Flip shade  normal
			shadeN = -N;
		} else {
			shadeN = N;
			RdotShadeN = -RdotShadeN;
		}

		SurfaceMaterial *triSurfMat = (SurfaceMaterial *)triMat;
		const Point hitPoint = pathRay(rayHit->t);
		const Vector wi = -pathRay.d;
		const Spectrum triInterpCol = InterpolateTriColor(tri, mesh->GetColors(), rayHit->b1, rayHit->b2);

		tracedShadowRayCount = 0;
		if (triSurfMat->IsLambertian()) {
			// Direct light sampling

			// Trace shadow rays
			const float lightStrategyPdf = static_cast<float>(scene->shadowRayCount) / static_cast<float>(scene->lights.size());
			const Spectrum lightTroughtput = throughput * triInterpCol * RdotShadeN;
			for (unsigned int i = 0; i < scene->shadowRayCount; ++i) {
				// Select the light to sample
				const unsigned int currentLightIndex = scene->SampleLights(sample.GetLazyValue());
				const TriangleLight *light = scene->lights[currentLightIndex];

				// Select a point on the light surface
				lightColor[tracedShadowRayCount] = light->Sample_L(
						scene->objects, hitPoint, shadeN,
						sample.GetLazyValue(), sample.GetLazyValue(),
						&lightPdf[tracedShadowRayCount], &shadowRay[tracedShadowRayCount]);

				lightColor[tracedShadowRayCount] *=  lightTroughtput *
						triSurfMat->f(wi, shadowRay[tracedShadowRayCount].d, shadeN);

				// Scale light pdf for ONE_UNIFORM strategy
				lightPdf[tracedShadowRayCount] *= lightStrategyPdf;

				// Using 0.1 instead of 0.0 to cut down fireflies
				if ((lightPdf[tracedShadowRayCount] > 0.1f) && !lightColor[tracedShadowRayCount].Black())
					tracedShadowRayCount++;
			}
		}

		//----------------------------------------------------------------------
		// Build the next vertex path ray
		//----------------------------------------------------------------------

		float fPdf;
		Vector wo;
		const Spectrum f = triSurfMat->Sample_f(wi, &wo, N, shadeN,
			sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
			&fPdf, specularBounce) * triInterpCol;

		const float dp = RdotShadeN / fPdf;
		/*if (depth > 2) {
			// Russian Roulette
			const float p = Min(1.f, f.filter() * AbsDot(shadeN, pathRay.d));
			if (p > sample.GetLazyValue())
				throughput /= p;
			else {
				// Terminate the path
				sampleBuffer->SplatSample(&sample, radiance);
				// Restart the path
				Init(scene, sampler);
				return;
			}
		}*/

		throughput *= dp;
		throughput *= f;

		pathRay.o = hitPoint;
		pathRay.d = wo;
		state = NEXT_VERTEX;
	}

private:
	void CalcNextStep(Scene *scene, Sampler *sampler, const RayBuffer *rayBuffer,
		SampleBuffer *sampleBuffer);

	Sample sample;

	Spectrum throughput, radiance;
	int depth;

	float *lightPdf;
	Spectrum *lightColor;

	Ray pathRay, *shadowRay;
	unsigned int currentPathRayIndex, *currentShadowRayIndex;
	unsigned int tracedShadowRayCount;
	PathState state;
	bool specularBounce;
};

class PathIntegrator {
public:
	PathIntegrator(Scene *s, Sampler *samp, SampleBuffer *sb);
	~PathIntegrator();

	void ReInit();

	size_t PathCount() const { return paths.size(); }
	void ClearPaths();

	void FillRayBuffer(RayBuffer *rayBuffer);
	void AdvancePaths(const RayBuffer *rayBuffer);

	double statsRenderingStart;
	double statsTotalSampleCount;

private:
	Sampler *sampler;
	Scene *scene;
	SampleBuffer *sampleBuffer;
	int firstPath, lastPath;
	vector<Path *> paths;
};

#endif	/* _PATH_H */
