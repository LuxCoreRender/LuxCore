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

#include "smalllux.h"
#include "hitpoints.h"
#include "sppm.h"

bool GetHitPointInformation(const Scene *scene, RandomGenerator *rndGen,
		Ray *ray, const RayHit *rayHit, Point &hitPoint,
		Spectrum &surfaceColor, Normal &N, Normal &shadeN) {
	hitPoint = (*ray)(rayHit->t);
	const unsigned int currentTriangleIndex = rayHit->index;

	// Get the triangle
	const ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
	const Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

	const Spectrum *colors = mesh->GetColors();
	if (colors)
		surfaceColor = InterpolateTriColor(tri, colors, rayHit->b1, rayHit->b2);
	else
		surfaceColor = Spectrum(1.f, 1.f, 1.f);

	// Interpolate face normal
	N = InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);

	// Check if I have to apply texture mapping or normal mapping
	TexMapInstance *tm = scene->triangleTexMaps[currentTriangleIndex];
	BumpMapInstance *bm = scene->triangleBumpMaps[currentTriangleIndex];
	NormalMapInstance *nm = scene->triangleNormalMaps[currentTriangleIndex];
	if (tm || bm || nm) {
		// Interpolate UV coordinates if required
		const UV triUV = InterpolateTriUV(tri, mesh->GetUVs(), rayHit->b1, rayHit->b2);

		// Check if there is an assigned texture map
		if (tm) {
			const TextureMap *map = tm->GetTexMap();

			// Apply texture mapping
			surfaceColor *= map->GetColor(triUV);

			// Check if the texture map has an alpha channel
			if (map->HasAlpha()) {
				const float alpha = map->GetAlpha(triUV);

				if ((alpha == 0.0f) || ((alpha < 1.f) && (rndGen->floatValue() > alpha))) {
					*ray = Ray(hitPoint, ray->d);
					return true;
				}
			}
		}

		// Check if there is an assigned bump/normal map
		if (bm || nm) {
			if (nm) {
				// Apply normal mapping
				const Spectrum color = nm->GetTexMap()->GetColor(triUV);

				const float x = 2.0 * (color.r - 0.5);
				const float y = 2.0 * (color.g - 0.5);
				const float z = 2.0 * (color.b - 0.5);

				Vector v1, v2;
				CoordinateSystem(Vector(N), &v1, &v2);
				N = Normalize(Normal(
						v1.x * x + v2.x * y + N.x * z,
						v1.y * x + v2.y * y + N.y * z,
						v1.z * x + v2.z * y + N.z * z));
			}

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
	}

	// Flip the normal if required
	if (Dot(ray->d, N) > 0.f)
		shadeN = -N;
	else
		shadeN = N;

	return false;
}

//------------------------------------------------------------------------------

HitPoints::HitPoints(SPPMRenderEngine *engine, RandomGenerator *rndGen,
			IntersectionDevice *dev, RayBuffer *rayBuffer) {
	renderEngine = engine;
	device = dev;
	pass = 0;

	const unsigned int width = renderEngine->film->GetWidth();
	const unsigned int height = renderEngine->film->GetHeight();
	hitPoints = new std::vector<HitPoint>(width * height);
	SetHitPoints(rndGen, rayBuffer);

	// Calculate initial radius
	Vector ssize = bbox.pMax - bbox.pMin;
	const float photonRadius = renderEngine->photonStartRadiusScale * ((ssize.x + ssize.y + ssize.z) / 3.f) / ((width + height) / 2.f) * 2.f;

	// Expand the bounding box by used radius
	bbox.Expand(photonRadius);

	// Initialize hit points field
	const float photonRadius2 = photonRadius * photonRadius;
	for (unsigned int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		hp->photonCount = 0;
		hp->reflectedFlux = Spectrum();

		hp->accumPhotonRadius2 = photonRadius2;
		hp->accumPhotonCount = 0;
		hp->accumReflectedFlux = Spectrum();

		hp->accumRadiance = Spectrum();
		hp->constantHitsCount = 0;
		hp->surfaceHitsCount = 0;
		hp->radiance = Spectrum();
	}

	// Allocate hit points lookup accelerator
	switch (renderEngine->lookupAccelType) {
		case HASH_GRID:
			lookUpAccel = new HashGrid(this);
			break;
		case KD_TREE:
			lookUpAccel = new KdTree(this);
			break;
		case HYBRID_HASH_GRID:
			lookUpAccel = new HybridHashGrid(this);
			break;
		default:
			assert (false);
	}
}

HitPoints::~HitPoints() {
	delete lookUpAccel;
	delete hitPoints;
}

void HitPoints::AccumulateFlux(const unsigned long long photonTraced) {
	std::cerr << "Accumulate photons flux" << std::endl;

	for (unsigned int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		switch (hp->type) {
			case CONSTANT_COLOR:
				hp->accumRadiance += hp->throughput;
				hp->constantHitsCount += 1;
				break;
			case SURFACE:
				if ((hp->accumPhotonCount > 0)) {
					const unsigned long long pcount = hp->photonCount + hp->accumPhotonCount;
					const double alpha = renderEngine->photonAlpha;
					const float g = alpha * pcount / (hp->photonCount * alpha + hp->accumPhotonCount);
					hp->photonCount = pcount;
					hp->reflectedFlux = (hp->reflectedFlux + hp->accumReflectedFlux) * g;

					hp->accumPhotonRadius2 *= g;
					hp->accumPhotonCount = 0;
					hp->accumReflectedFlux = Spectrum();
				}

				hp->surfaceHitsCount += 1;
				break;
			default:
				assert (false);
		}

		const unsigned int count = hp->constantHitsCount + hp->surfaceHitsCount;
		if (count > 0) {
			const double k = 1.0 / (M_PI * hp->accumPhotonRadius2 * photonTraced);
			hp->radiance = (hp->accumRadiance + hp->surfaceHitsCount * hp->reflectedFlux * k) / count;
		}
	}
}

void HitPoints::SetHitPoints(RandomGenerator *rndGen, RayBuffer *rayBuffer) {
	std::list<EyePath *> todoEyePaths;
	SLGScene *scene = renderEngine->scene;
	const unsigned int width = renderEngine->film->GetWidth();
	const unsigned int height = renderEngine->film->GetHeight();

	// Generate eye rays
	std::cerr << "Building eye paths rays:" << std::endl;
	std::cerr << "  0/" << height << std::endl;
	double lastPrintTime = WallClockTime();
	for (unsigned int y = 0; y < height; ++y) {
		if (WallClockTime() - lastPrintTime > 2.0) {
			std::cerr << "  " << y << "/" << height << std::endl;
			lastPrintTime = WallClockTime();
		}

		for (unsigned int x = 0; x < width; ++x) {
			EyePath *eyePath = new EyePath();

			const float scrX = x + rndGen->floatValue() - 0.5f;
			const float scrY = y + rndGen->floatValue() - 0.5f;
			scene->camera->GenerateRay(scrX, scrY, width, height, &eyePath->ray,
					rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue());

			eyePath->pixelIndex = x + y * width;
			eyePath->depth = 0;
			eyePath->throughput = Spectrum(1.f, 1.f, 1.f);

			todoEyePaths.push_front(eyePath);
		}
	}

	// Iterate through all eye paths
	std::cerr << "Building eye paths hit points: " << std::endl;
	bool done;
	lastPrintTime = WallClockTime();
	// Note: (todoEyePaths.size() > 0) is extremly slow to execute
	unsigned int todoEyePathCount = width * height;
	std::cerr << "  " << todoEyePathCount / 1000 << "k eye paths left" << std::endl;
	while(todoEyePathCount > 0) {
		if (WallClockTime() - lastPrintTime > 2.0) {
			std::cerr << "  " << todoEyePathCount / 1000 << "k eye paths left" << std::endl;
			lastPrintTime = WallClockTime();
		}

		std::list<EyePath *>::iterator todoEyePathsIterator = todoEyePaths.begin();
		while (todoEyePathsIterator != todoEyePaths.end()) {
			EyePath *eyePath = *todoEyePathsIterator;

			// Check if we reached the max path depth
			if (eyePath->depth > renderEngine->maxEyePathDepth) {
				// Add an hit point
				HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
				hp.type = CONSTANT_COLOR;
				hp.throughput = Spectrum();
				// Free the eye path
				delete *todoEyePathsIterator;
				todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
				--todoEyePathCount;
			} else {
				eyePath->depth++;
				rayBuffer->AddRay(eyePath->ray);
				done = false;
				if (rayBuffer->IsFull())
					break;

				++todoEyePathsIterator;
			}
		}

		if (rayBuffer->GetRayCount() > 0) {
			// Trace the rays
			device->PushRayBuffer(rayBuffer);
			rayBuffer = device->PopRayBuffer();

			// Update the paths
			todoEyePathsIterator = todoEyePaths.begin();
			for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i) {
				EyePath *eyePath = *todoEyePathsIterator;

				const RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];

				if (rayHit->Miss()) {
					// Add an hit point
					HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
					hp.type = CONSTANT_COLOR;
					if (scene->infiniteLight)
						hp.throughput = scene->infiniteLight->Le(eyePath->ray.d) * eyePath->throughput;
					else
						hp.throughput = Spectrum();

					// Free the eye path
					delete *todoEyePathsIterator;
					todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
					--todoEyePathCount;
				} else {
					// Something was hit
					Point hitPoint;
					Spectrum surfaceColor;
					Normal N, shadeN;
					if (GetHitPointInformation(scene, rndGen, &eyePath->ray, rayHit, hitPoint,
							surfaceColor, N, shadeN)) {
						++todoEyePathsIterator;
						continue;
					}

					// Get the material
					const unsigned int currentTriangleIndex = rayHit->index;
					const Material *triMat = scene->triangleMaterials[currentTriangleIndex];

					if (triMat->IsLightSource()) {
						// Add an hit point
						HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
						hp.type = CONSTANT_COLOR;
						const TriangleLight *tLight = (TriangleLight *)triMat;
						hp.throughput = tLight->Le(scene, -eyePath->ray.d) * eyePath->throughput;

						// Free the eye path
						delete *todoEyePathsIterator;
						todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
						--todoEyePathCount;
					} else {
						// Build the next vertex path ray
						const SurfaceMaterial *triSurfMat = (SurfaceMaterial *)triMat;

						float fPdf;
						Vector wi;
						bool specularBounce;
						const Spectrum f = triSurfMat->Sample_f(-eyePath->ray.d, &wi, N, shadeN,
								rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
								false, &fPdf, specularBounce) * surfaceColor;
						if ((fPdf <= 0.f) || f.Black()) {
							// Add an hit point
							HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
							hp.type = CONSTANT_COLOR;
							hp.throughput = luxrays::Spectrum();

							// Free the eye path
							delete *todoEyePathsIterator;
							todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
							--todoEyePathCount;
						} else if (specularBounce || (!triSurfMat->IsDiffuse())) {
							++todoEyePathsIterator;

							eyePath->throughput *= f / fPdf;
							eyePath->ray = Ray(hitPoint, wi);
						} else {
							// Add an hit point
							HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
							hp.type = SURFACE;
							hp.material = (SurfaceMaterial *)triMat;
							hp.throughput = eyePath->throughput * surfaceColor;
							hp.position = hitPoint;
							hp.wo = -eyePath->ray.d;
							hp.normal = shadeN;

							// Free the eye path
							delete *todoEyePathsIterator;
							todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
							--todoEyePathCount;
						}
					}
				}
			}

			rayBuffer->Reset();
		}
	}

	// Calculate hit points bounding box
	std::cerr << "Building hit points bounding box: ";
	bbox = BBox();
	for (unsigned int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		if (hp->type == SURFACE)
			bbox = Union(bbox, hp->position);
	}
	std::cerr << bbox << std::endl;
}
