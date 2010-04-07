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
#include "light.h"
#include "scene.h"
#include "volume.h"

class RenderingConfig;

class Path {
public:
	enum PathState {
		EYE_VERTEX, EYE_VERTEX_VOLUME_STEP, NEXT_VERTEX, ONLY_SHADOW_RAYS
	};

	Path(Scene *scene) {
		unsigned int shadowRayCount = GetMaxShadowRaysCount(scene);

		lightPdf = new float[shadowRayCount];
		lightColor = new Spectrum[shadowRayCount];
		shadowRay = new Ray[shadowRayCount];
		currentShadowRayIndex = new unsigned int[shadowRayCount];
		if (scene->volumeIntegrator)
			volumeComp = new VolumeComputation(scene->volumeIntegrator);
		else
			volumeComp = NULL;
	}

	~Path() {
		delete[] lightPdf;
		delete[] lightColor;
		delete[] shadowRay;
		delete[] currentShadowRayIndex;

		delete volumeComp;
	}

	void Init(Scene *scene, Sampler *sampler) {
		throughput = Spectrum(1.f, 1.f, 1.f);
		radiance = Spectrum(0.f, 0.f, 0.f);
		sampler->GetNextSample(&sample);

		scene->camera->GenerateRay(&sample, &pathRay,
			sampler->GetLazyValue(&sample), sampler->GetLazyValue(&sample));
		state = EYE_VERTEX;
		depth = 0;
		specularBounce = true;
	}

	bool FillRayBuffer(Scene *scene, RayBuffer *rayBuffer) {
		const unsigned int leftSpace = rayBuffer->LeftSpace();
		// Check if the there is enough free space in the RayBuffer
		if (((state == EYE_VERTEX) && (1 > leftSpace)) ||
				((state == EYE_VERTEX_VOLUME_STEP) && (volumeComp->GetRayCount() + 1 > leftSpace)) ||
				((state == ONLY_SHADOW_RAYS) && (tracedShadowRayCount > leftSpace)) ||
				((state == NEXT_VERTEX) && (tracedShadowRayCount + 1 > leftSpace)))
			return false;

		if (state == EYE_VERTEX_VOLUME_STEP) {
			volumeComp->AddRays(rayBuffer);
		} else {
			if (state != ONLY_SHADOW_RAYS)
				currentPathRayIndex = rayBuffer->AddRay(pathRay);

			if ((state == NEXT_VERTEX) || (state == ONLY_SHADOW_RAYS)) {
				for (unsigned int i = 0; i < tracedShadowRayCount; ++i)
					currentShadowRayIndex[i] = rayBuffer->AddRay(shadowRay[i]);
			}
		}

		return true;
	}

	void AdvancePath(Scene *scene, Sampler *sampler, const RayBuffer *rayBuffer,
			SampleBuffer *sampleBuffer) {
		// Select the path ray hit
		const RayHit *rayHit;
		if ((state == EYE_VERTEX) && scene->volumeIntegrator) {
			rayHit = rayBuffer->GetRayHit(currentPathRayIndex);

			// Use Russian Roulette to check if I have to do participating media computation or not
			if (sample.GetLazyValue() <= scene->volumeIntegrator->GetRRProbability()) {
				Ray volumeRay(pathRay.o, pathRay.d, 0.f, (rayHit->index == 0xffffffffu) ? std::numeric_limits<float>::infinity() : rayHit->t);
				scene->volumeIntegrator->GenerateLiRays(scene, &sample, volumeRay, volumeComp);
				radiance += volumeComp->GetEmittedLight();

				if (volumeComp->GetRayCount() > 0) {
					// Do the EYE_VERTEX_VOLUME_STEP
					state = EYE_VERTEX_VOLUME_STEP;
					eyeHit = *(rayBuffer->GetRayHit(currentPathRayIndex));
					return;
				}
			}
		} else if (state == EYE_VERTEX_VOLUME_STEP) {
			// Add scattered light
			radiance += throughput * volumeComp->CollectResults(rayBuffer) / scene->volumeIntegrator->GetRRProbability();

			rayHit = &eyeHit;
		} else
			rayHit = rayBuffer->GetRayHit(currentPathRayIndex);

		// Finish direct light sampling
		if (((state == NEXT_VERTEX) || (state == ONLY_SHADOW_RAYS)) && (tracedShadowRayCount > 0)) {
			for (unsigned int i = 0; i < tracedShadowRayCount; ++i) {
				const RayHit *shadowRayHit = rayBuffer->GetRayHit(currentShadowRayIndex[i]);
				if (shadowRayHit->index == 0xffffffffu) {
					// Nothing was hit, light is visible
					radiance += lightColor[i] / lightPdf[i];
				}
			}
		}

		// Calculate next step
		depth++;

		const bool missed = (rayHit->index == 0xffffffffu);

		if (missed || (state == ONLY_SHADOW_RAYS) || (depth >= scene->maxPathDepth)) {
			if (missed && scene->infiniteLight && (scene->useInfiniteLightBruteForce || specularBounce)) {
				// Add the light emitted by the infinite light
				radiance += scene->infiniteLight->Le(pathRay.d) * throughput;
			}

			// Hit nothing/only shadow rays/maxdepth, terminate the path
			sampleBuffer->SplatSample(&sample, radiance);
			// Restart the path
			Init(scene, sampler);
			return;
		}

		// Something was hit
		const unsigned int currentTriangleIndex = rayHit->index;

		// Get the triangle
		const ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
		const Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

		// Get the material
		Material *triMat = scene->triangleMaterials[currentTriangleIndex];

		// Check if it is a light source
		if (triMat->IsLightSource()) {
			if (specularBounce) {
				// Only TriangleLight can be directly hit
				const TriangleLight *tLight = (TriangleLight *)triMat;
				Spectrum Le = tLight->Le(scene->objects, -pathRay.d);

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

		// Interpolate face normal
		Normal N = InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);

		SurfaceMaterial *triSurfMat = (SurfaceMaterial *)triMat;
		const Point hitPoint = pathRay(rayHit->t);
		const Vector wo = -pathRay.d;

		Spectrum surfaceColor;
		const Spectrum *colors = mesh->GetColors();
		if (colors)
			surfaceColor = InterpolateTriColor(tri, mesh->GetColors(), rayHit->b1, rayHit->b2);
		else
			surfaceColor = Spectrum(1.f, 1.f, 1.f);

		// Check if I have to apply texture mapping or normal mapping
		TexMapInstance *tm = scene->triangleTexMaps[currentTriangleIndex];
		BumpMapInstance *bm = scene->triangleBumpMaps[currentTriangleIndex];
		if (tm || bm) {
			// Interpolate UV coordinates if required
			const UV triUV = InterpolateTriUV(tri, mesh->GetUVs(), rayHit->b1, rayHit->b2);

			// Check if there is an assigned texture map
			if (tm)	{
				const TextureMap *map = tm->GetTexMap();

				// Apply texture mapping
				surfaceColor *= map->GetColor(triUV);

				// Check if the texture map has an alpha channel
				if (map->HasAlpha()) {
					const float alpha = map->GetAlpha(triUV);

					if ((alpha == 0.0f) || ((alpha < 1.f) && (sample.GetLazyValue() > alpha))) {
						pathRay = Ray(pathRay(rayHit->t + RAY_EPSILON), pathRay.d);
						state = NEXT_VERTEX;
						tracedShadowRayCount = 0;
						return;
					}
				}
			}

			// Check if there is an assigned bump map
			if (bm) {
				// Apply bump mapping
				const TextureMap *map = bm->GetTexMap();
				const UV &dudv = map->GetDuDv();

				const float b0 = map->GetColor(triUV).Filter();
				
				const UV uvdu(triUV.u + dudv.u, triUV.v);
				const float bu = map->GetColor(uvdu).Filter();
				
				const UV uvdv(triUV.u, triUV.v + dudv.v);
				const float bv = map->GetColor(uvdv).Filter();

				const float scale = bm->GetScale();
				const Vector bump(scale * (bu - b0), scale * (bv - b0), 1.f);

				Vector v1, v2;
				CoordinateSystem(Vector(N), &v1, &v2);
				N = Normalize(Normal(
						v1.x * bump.x + v2.x * bump.y + N.x * bump.z,
						v1.y * bump.x + v2.y * bump.y + N.y * bump.z,
						v1.z * bump.x + v2.z * bump.y + N.z * bump.z));
			}
		}

		// Flip the normal if required
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

		tracedShadowRayCount = 0;
		if (triSurfMat->IsDiffuse() && (scene->lights.size() > 0)) {
			// Direct light sampling: trace shadow rays

			switch (scene->lightStrategy) {
				case ALL_UNIFORM: {
					// ALL UNIFORM direct sampling light strategy
					const Spectrum lightTroughtput = throughput * surfaceColor;

					for (unsigned int j = 0; j < scene->lights.size(); ++j) {
						// Select the light to sample
						const LightSource *light = scene->lights[j];

						for (unsigned int i = 0; i < scene->shadowRayCount; ++i) {
							// Select a point on the light surface
							lightColor[tracedShadowRayCount] = light->Sample_L(
									scene->objects, hitPoint, &shadeN,
									sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
									&lightPdf[tracedShadowRayCount], &shadowRay[tracedShadowRayCount]);

							// Scale light pdf for ALL_UNIFORM strategy
							lightPdf[tracedShadowRayCount] *= scene->shadowRayCount;

							if (lightPdf[tracedShadowRayCount] <= 0.0f)
								continue;

							const Vector lwi = shadowRay[tracedShadowRayCount].d;
							lightColor[tracedShadowRayCount] *= lightTroughtput * Dot(shadeN, lwi) *
									triSurfMat->f(wo, lwi, shadeN);

							if (!lightColor[tracedShadowRayCount].Black())
								tracedShadowRayCount++;
						}
					}

					break;
				}
				case ONE_UNIFORM: {
					// ONE UNIFORM direct sampling light strategy
					const Spectrum lightTroughtput = throughput * surfaceColor;

					for (unsigned int i = 0; i < scene->shadowRayCount; ++i) {
						// Select the light to sample
						const unsigned int currentLightIndex = scene->SampleLights(sample.GetLazyValue());
						const LightSource *light = scene->lights[currentLightIndex];

						// Select a point on the light surface
						lightColor[tracedShadowRayCount] = light->Sample_L(
								scene->objects, hitPoint, &shadeN,
								sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
								&lightPdf[tracedShadowRayCount], &shadowRay[tracedShadowRayCount]);

						if (lightPdf[tracedShadowRayCount] <= 0.0f)
							continue;

						const Vector lwi = shadowRay[tracedShadowRayCount].d;
						lightColor[tracedShadowRayCount] *= lightTroughtput * Dot(shadeN, lwi) *
								triSurfMat->f(wo, lwi, shadeN);

						if (!lightColor[tracedShadowRayCount].Black())
							tracedShadowRayCount++;
					}

					// Update the light pdf according the number of shadow ray traced
					const float lightStrategyPdf = static_cast<float>(tracedShadowRayCount) / static_cast<float>(scene->lights.size());
					for (unsigned int i = 0; i < tracedShadowRayCount; ++i) {
						// Scale light pdf for ONE_UNIFORM strategy
						lightPdf[i] *= lightStrategyPdf;
					}

					break;
				}
				default:
					assert (false);
			}
		}

		//----------------------------------------------------------------------
		// Build the next vertex path ray
		//----------------------------------------------------------------------

		float fPdf;
		Vector wi;
		const Spectrum f = triSurfMat->Sample_f(wo, &wi, N, shadeN,
			sample.GetLazyValue(), sample.GetLazyValue(), sample.GetLazyValue(),
			&fPdf, specularBounce) * surfaceColor;
		if ((fPdf <= 0.f) || f.Black()) {
			if (tracedShadowRayCount > 0)
				state = ONLY_SHADOW_RAYS;
			else {
				// Terminate the path
				sampleBuffer->SplatSample(&sample, radiance);
				// Restart the path
				Init(scene, sampler);
			}

			return;
		}

		if (depth > scene->rrDepth) {
			// Russian Roulette
			if (scene->rrProb >= sample.GetLazyValue())
				throughput /= scene->rrProb;
			else {
				// Check if terminate the path or I have still to trace shadow rays
				if (tracedShadowRayCount > 0)
					state = ONLY_SHADOW_RAYS;
				else {
					// Terminate the path
					sampleBuffer->SplatSample(&sample, radiance);
					// Restart the path
					Init(scene, sampler);
				}

				return;
			}
		}

		throughput *= f / fPdf;

		pathRay.o = hitPoint;
		pathRay.d = wi;
		state = NEXT_VERTEX;
	}

private:
	static unsigned int GetMaxShadowRaysCount(const Scene *scene) {
		switch (scene->lightStrategy) {
			case ALL_UNIFORM:
				return scene->lights.size() * scene->shadowRayCount;
				break;
			case ONE_UNIFORM:
				return scene->shadowRayCount;
			default:
				throw runtime_error("Internal error in Path::GetMaxShadowRaysCount()");
		}
	}

	void CalcNextStep(Scene *scene, Sampler *sampler, const RayBuffer *rayBuffer,
		SampleBuffer *sampleBuffer);

	Sample sample;

	Spectrum throughput, radiance;
	int depth;

	float *lightPdf;
	Spectrum *lightColor;

	// Used to save eye hit during EYE_VERTEX_VOLUME_STEP
	RayHit eyeHit;

	Ray pathRay, *shadowRay;
	unsigned int currentPathRayIndex, *currentShadowRayIndex;
	unsigned int tracedShadowRayCount;
	VolumeComputation *volumeComp;

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
