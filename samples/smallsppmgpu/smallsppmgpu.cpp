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

// Based on smallppm, Progressive Photon Mapping by T. Hachisuka

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#if defined (WIN32)
#include <windows.h>
#endif

// Jens's patch for MacOS
#if defined(__APPLE__)
#include <GLut/glut.h>
#else
#include <GL/glut.h>
#endif

#include <FreeImage.h>
#include <boost/detail/container_fwd.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/sdl/scene.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/pixel/framebuffer.h"
#include "luxrays/utils/film/film.h"

#define MAX_EYE_PATH_DEPTH 16
#define MAX_PHOTON_PATH_DEPTH 16

class EyePath {
public:
	// Screen information
	unsigned int pixelIndex;

	// Eye path information
	luxrays::Ray ray;
	unsigned int depth;
	luxrays::Spectrum throughput;
};

class PhotonPath {
public:
	// The ray is stored in the RayBuffer and the index is implicitly stored
	// in the array of PhotonPath
	luxrays::Spectrum flux;
	unsigned int depth;
};

//------------------------------------------------------------------------------
// Eye path hit points
//------------------------------------------------------------------------------

enum HitPointType {
	SURFACE, CONSTANT_COLOR
};

class HitPoint {
public:
	HitPointType type;

	// Used for CONSTANT_COLOR and SURFACE type
	luxrays::Spectrum throughput;

	// Used for SURFACE type
	luxrays::Point position;
	luxrays::Vector wo;
	luxrays::Normal normal;
	const luxrays::sdl::SurfaceMaterial *material;

	unsigned long long photonCount;
	luxrays::Spectrum reflectedFlux;

	float accumPhotonRadius2;
	unsigned long long accumPhotonCount;
	luxrays::Spectrum accumReflectedFlux;

	luxrays::Spectrum accumRadiance;

	unsigned int constantHitsCount;
	unsigned int surfaceHitsCount;
	luxrays::Spectrum radiance;
};

static bool GetHitPointInformation(const luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
		luxrays::Ray *ray, const luxrays::RayHit *rayHit, luxrays::Point &hitPoint,
		luxrays::Spectrum &surfaceColor, luxrays::Normal &N, luxrays::Normal &shadeN) {
	hitPoint = (*ray)(rayHit->t);
	const unsigned int currentTriangleIndex = rayHit->index;

	// Get the triangle
	const luxrays::ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
	const luxrays::Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

	const luxrays::Spectrum *colors = mesh->GetColors();
	if (colors)
		surfaceColor = luxrays::InterpolateTriColor(tri, colors, rayHit->b1, rayHit->b2);
	else
		surfaceColor = luxrays::Spectrum(1.f, 1.f, 1.f);

	// Interpolate face normal
	N = luxrays::InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);

	// Check if I have to apply texture mapping or normal mapping
	luxrays::sdl::TexMapInstance *tm = scene->triangleTexMaps[currentTriangleIndex];
	luxrays::sdl::BumpMapInstance *bm = scene->triangleBumpMaps[currentTriangleIndex];
	luxrays::sdl::NormalMapInstance *nm = scene->triangleNormalMaps[currentTriangleIndex];
	if (tm || bm || nm) {
		// Interpolate UV coordinates if required
		const luxrays::UV triUV = luxrays::InterpolateTriUV(tri, mesh->GetUVs(), rayHit->b1, rayHit->b2);

		// Check if there is an assigned texture map
		if (tm) {
			const luxrays::sdl::TextureMap *map = tm->GetTexMap();

			// Apply texture mapping
			surfaceColor *= map->GetColor(triUV);

			// Check if the texture map has an alpha channel
			if (map->HasAlpha()) {
				const float alpha = map->GetAlpha(triUV);

				if ((alpha == 0.0f) || ((alpha < 1.f) && (rndGen->floatValue() > alpha))) {
					*ray = luxrays::Ray(hitPoint, ray->d);
					return true;
				}
			}
		}

		// Check if there is an assigned bump/normal map
		if (bm || nm) {
			if (nm) {
				// Apply normal mapping
				const luxrays::Spectrum color = nm->GetTexMap()->GetColor(triUV);

				const float x = 2.0 * (color.r - 0.5);
				const float y = 2.0 * (color.g - 0.5);
				const float z = 2.0 * (color.b - 0.5);

				luxrays::Vector v1, v2;
				luxrays::CoordinateSystem(luxrays::Vector(N), &v1, &v2);
				N = luxrays::Normalize(luxrays::Normal(
						v1.x * x + v2.x * y + N.x * z,
						v1.y * x + v2.y * y + N.y * z,
						v1.z * x + v2.z * y + N.z * z));
			}

			if (bm) {
				// Apply bump mapping
				const luxrays::sdl::TextureMap *map = bm->GetTexMap();
				const luxrays::UV &dudv = map->GetDuDv();

				const float b0 = map->GetColor(triUV).Filter();

				const luxrays::UV uvdu(triUV.u + dudv.u, triUV.v);
				const float bu = map->GetColor(uvdu).Filter();

				const luxrays::UV uvdv(triUV.u, triUV.v + dudv.v);
				const float bv = map->GetColor(uvdv).Filter();

				const float scale = bm->GetScale();
				const luxrays::Vector bump(scale * (bu - b0), scale * (bv - b0), 1.f);

				luxrays::Vector v1, v2;
				luxrays::CoordinateSystem(luxrays::Vector(N), &v1, &v2);
				N = luxrays::Normalize(luxrays::Normal(
						v1.x * bump.x + v2.x * bump.y + N.x * bump.z,
						v1.y * bump.x + v2.y * bump.y + N.y * bump.z,
						v1.z * bump.x + v2.z * bump.y + N.z * bump.z));
			}
		}
	}

	// Flip the normal if required
	if (luxrays::Dot(ray->d, N) > 0.f)
		shadeN = -N;
	else
		shadeN = N;

	return false;
}

class HitPoints {
public:
	HitPoints(luxrays::sdl::Scene *scn, luxrays::RandomGenerator *rndg,
			luxrays::IntersectionDevice *dev, const float a,
			const unsigned int w, const unsigned int h) {
		scene = scn;
		rndGen = rndg;
		device = dev;
		rayBuffer = device->NewRayBuffer();
		alpha = a;
		width = w;
		height = h;

		hitPoints = new std::vector<HitPoint>(width * height);
		SetHitPoints();

		// Calculate initial radius
		luxrays::Vector ssize = bbox.pMax - bbox.pMin;
		const float photonRadius = ((ssize.x + ssize.y + ssize.z) / 3.f) / ((width + height) / 2.f) * 2.f;

		// Expand the bounding box by used radius
		bbox.Expand(photonRadius);

		// Initialize hit points field
		const float photonRadius2 = photonRadius * photonRadius;
		for (unsigned int i = 0; i < (*hitPoints).size(); ++i) {
			HitPoint *hp = &(*hitPoints)[i];

			hp->photonCount = 0;
			hp->reflectedFlux = luxrays::Spectrum();

			hp->accumPhotonRadius2 = photonRadius2;
			hp->accumPhotonCount = 0;
			hp->accumReflectedFlux = luxrays::Spectrum();

			hp->accumRadiance = luxrays::Spectrum();
			hp->constantHitsCount = 0;
			hp->surfaceHitsCount = 0;
		}
	}

	~HitPoints() {
		delete hitPoints;
		delete rayBuffer;
	}

	HitPoint *GetHitPoint(const unsigned int index) {
		return &(*hitPoints)[index];
	}

	const unsigned int GetSize() const {
		return hitPoints->size();
	}

	const luxrays::BBox GetBBox() const {
		return bbox;
	}

	void AccumulateFlux(const unsigned int photonTraced) {
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
						const float g = alpha * pcount / (hp->photonCount * alpha + hp->accumPhotonCount);
						hp->photonCount = pcount;
						hp->reflectedFlux = (hp->reflectedFlux + hp->accumReflectedFlux) * g;

						hp->accumPhotonRadius2 *= g;
						hp->accumPhotonCount = 0;
						hp->accumReflectedFlux = luxrays::Spectrum();
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

	void Recast(const unsigned int photonTraced) {
		AccumulateFlux(photonTraced);

		SetHitPoints();
	}

private:
	void SetHitPoints() {
		std::list<EyePath *> todoEyePaths;

		// Generate eye rays
		std::cerr << "Building eye paths rays:" << std::endl;
		std::cerr << "  0/" << height << std::endl;
		double lastPrintTime = luxrays::WallClockTime();
		for (unsigned int y = 0; y < height; ++y) {
			if (luxrays::WallClockTime() - lastPrintTime > 2.0) {
				std::cerr << "  " << y << "/" << height << std::endl;
				lastPrintTime = luxrays::WallClockTime();
			}

			for (unsigned int x = 0; x < width; ++x) {
				EyePath *eyePath = new EyePath();

				const float scrX = x + rndGen->floatValue() - 0.5f;
				const float scrY = y + rndGen->floatValue() - 0.5f;
				scene->camera->GenerateRay(scrX, scrY, width, height, &eyePath->ray,
						rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue());

				eyePath->pixelIndex = x + y * width;
				eyePath->depth = 0;
				eyePath->throughput = luxrays::Spectrum(1.f, 1.f, 1.f);

				todoEyePaths.push_front(eyePath);
			}
		}

		// Iterate through all eye paths
		std::cerr << "Building eye paths hit points: " << std::endl;
		bool done;
		lastPrintTime = luxrays::WallClockTime();
		// Note: (todoEyePaths.size() > 0) is extremly slow to execute
		unsigned int todoEyePathCount = width * height;
		std::cerr << "  " << todoEyePathCount / 1000 << "k eye paths left" << std::endl;
		while(todoEyePathCount > 0) {
			if (luxrays::WallClockTime() - lastPrintTime > 2.0) {
				std::cerr << "  " << todoEyePathCount / 1000 << "k eye paths left" << std::endl;
				lastPrintTime = luxrays::WallClockTime();
			}

			std::list<EyePath *>::iterator todoEyePathsIterator = todoEyePaths.begin();
			while (todoEyePathsIterator != todoEyePaths.end()) {
				EyePath *eyePath = *todoEyePathsIterator;

				// Check if we reached the max path depth
				if (eyePath->depth > MAX_EYE_PATH_DEPTH) {
					// Add an hit point
					HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
					hp.type = CONSTANT_COLOR;
					hp.throughput = luxrays::Spectrum();

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

					const luxrays::RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];

					if (rayHit->Miss()) {
						// Add an hit point
						HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
						hp.type = CONSTANT_COLOR;
						if (scene->infiniteLight)
							hp.throughput = scene->infiniteLight->Le(eyePath->ray.d) * eyePath->throughput;
						else
							hp.throughput = luxrays::Spectrum();

						// Free the eye path
						delete *todoEyePathsIterator;
						todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
						--todoEyePathCount;
					} else {
						// Something was hit
						luxrays::Point hitPoint;
						luxrays::Spectrum surfaceColor;
						luxrays::Normal N, shadeN;
						if (GetHitPointInformation(scene, rndGen, &eyePath->ray, rayHit, hitPoint,
								surfaceColor, N, shadeN))
							continue;

						// Get the material
						const unsigned int currentTriangleIndex = rayHit->index;
						const luxrays::sdl::Material *triMat = scene->triangleMaterials[currentTriangleIndex];

						if (triMat->IsLightSource()) {
							// Add an hit point
							HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
							hp.type = CONSTANT_COLOR;
							const luxrays::sdl::TriangleLight *tLight = (luxrays::sdl::TriangleLight *)triMat;
							hp.throughput = tLight->Le(scene, -eyePath->ray.d) * eyePath->throughput;

							// Free the eye path
							delete *todoEyePathsIterator;
							todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
							--todoEyePathCount;
						} else {
							// Build the next vertex path ray
							const luxrays::sdl::SurfaceMaterial *triSurfMat = (luxrays::sdl::SurfaceMaterial *)triMat;

							float fPdf;
							luxrays::Vector wi;
							bool specularBounce;
							const luxrays::Spectrum f = triSurfMat->Sample_f(-eyePath->ray.d, &wi, N, shadeN,
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
								eyePath->ray = luxrays::Ray(hitPoint, wi);
							} else {
								// Add an hit point
								HitPoint &hp = (*hitPoints)[eyePath->pixelIndex];
								hp.type = SURFACE;
								hp.material = (luxrays::sdl::SurfaceMaterial *)triMat;
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
		bbox = luxrays::BBox();
		for (unsigned int i = 0; i < (*hitPoints).size(); ++i) {
			HitPoint *hp = &(*hitPoints)[i];

			if (hp->type == SURFACE)
				bbox = luxrays::Union(bbox, hp->position);
		}
		std::cerr << bbox << std::endl;
	}

	luxrays::sdl::Scene *scene;
	luxrays::RandomGenerator *rndGen;
	luxrays::IntersectionDevice *device;
	luxrays::RayBuffer *rayBuffer;
	// double instead of float because photon counters declared as int 64bit
	double alpha;
	unsigned int width;
	unsigned int height;

	luxrays::BBox bbox;
	std::vector<HitPoint> *hitPoints;
};

//------------------------------------------------------------------------------

class HashGrid {
public:
	HashGrid(HitPoints *hps) {
		hitPoints = hps;
		hashGrid = NULL;

		Rehash();
	}

	~HashGrid() {
		for (unsigned int i = 0; i < hashGridSize; ++i)
			delete hashGrid[i];
		delete hashGrid;
	}

	void Rehash() {
		const unsigned int hitPointsCount = hitPoints->GetSize();
		const luxrays::BBox &hpBBox = hitPoints->GetBBox();

		// Calculate the size of the grid cell
		float maxPhotonRadius2 = 0.f;
		for (unsigned int i = 0; i < hitPointsCount; ++i) {
			HitPoint *hp = hitPoints->GetHitPoint(i);

			if (hp->type == SURFACE)
				maxPhotonRadius2 = luxrays::Max(maxPhotonRadius2, hp->accumPhotonRadius2);
		}

		const float cellSize = sqrtf(maxPhotonRadius2) * 2.f;
		std::cerr << "Hash grid cell size: " << cellSize <<std::endl;
		invCellSize = 1.f / cellSize;

		// TODO: add a tunable parameter for hashgrid size
		hashGridSize = hitPointsCount;
		if (!hashGrid) {
			hashGrid = new std::list<HitPoint *>*[hashGridSize];

			for (unsigned int i = 0; i < hashGridSize; ++i)
				hashGrid[i] = NULL;
		} else {
			for (unsigned int i = 0; i < hashGridSize; ++i) {
				delete hashGrid[i];
				hashGrid[i] = NULL;
			}
		}

		std::cerr << "Building hit points hash grid:" << std::endl;
		std::cerr << "  0k/" << hitPointsCount / 1000 << "k" <<std::endl;
		//unsigned int maxPathCount = 0;
		double lastPrintTime = luxrays::WallClockTime();
		unsigned long long entryCount = 0;
		for (unsigned int i = 0; i < hitPointsCount; ++i) {
			if (luxrays::WallClockTime() - lastPrintTime > 2.0) {
				std::cerr << "  " << i / 1000 << "k/" << hitPointsCount / 1000 << "k" <<std::endl;
				lastPrintTime = luxrays::WallClockTime();
			}

			HitPoint *hp = hitPoints->GetHitPoint(i);

			if (hp->type == SURFACE) {
				const float photonRadius = sqrtf(hp->accumPhotonRadius2);
				const luxrays::Vector rad(photonRadius, photonRadius, photonRadius);
				const luxrays::Vector bMin = ((hp->position - rad) - hpBBox.pMin) * invCellSize;
				const luxrays::Vector bMax = ((hp->position + rad) - hpBBox.pMin) * invCellSize;

				for (int iz = abs(int(bMin.z)); iz <= abs(int(bMax.z)); iz++) {
					for (int iy = abs(int(bMin.y)); iy <= abs(int(bMax.y)); iy++) {
						for (int ix = abs(int(bMin.x)); ix <= abs(int(bMax.x)); ix++) {
							int hv = Hash(ix, iy, iz);

							if (hashGrid[hv] == NULL)
								hashGrid[hv] = new std::list<HitPoint *>();

							hashGrid[hv]->push_front(hp);
							++entryCount;

							/*// hashGrid[hv]->size() is very slow to execute
							if (hashGrid[hv]->size() > maxPathCount)
								maxPathCount = hashGrid[hv]->size();*/
						}
					}
				}
			}
		}
		//std::cerr << "Max. hit points in a single hash grid entry: " << maxPathCount << std::endl;
		std::cerr << "Total hash grid entry: " << entryCount << std::endl;
		std::cerr << "Avg. hit points in a single hash grid entry: " << entryCount / hashGridSize << std::endl;

		// HashGrid debug code
		/*for (unsigned int i = 0; i < hashGridSize; ++i) {
			if (hashGrid[i]) {
				if (hashGrid[i]->size() > 10) {
					std::cerr << "HashGrid[" << i << "].size() = " <<hashGrid[i]->size() << std::endl;
				}
			}
		}*/
	}

	void AddFlux(const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
		const luxrays::Vector wi, const luxrays::Spectrum photonFlux) {
		// Look for eye path hit points near the current hit point
		luxrays::Vector hh = (hitPoint - hitPoints->GetBBox().pMin) * invCellSize;
		const int ix = abs(int(hh.x));
		const int iy = abs(int(hh.y));
		const int iz = abs(int(hh.z));

		std::list<HitPoint *> *hps = hashGrid[Hash(ix, iy, iz)];
		if (hps) {
			std::list<HitPoint *>::iterator iter = hps->begin();
			while (iter != hps->end()) {
				HitPoint *hp = *iter++;
				luxrays::Vector v = hp->position - hitPoint;
				// TODO: use configurable parameter for normal treshold
				if ((luxrays::Dot(hp->normal, shadeN) > 0.5f) &&
						(luxrays::Dot(v, v) <=  hp->accumPhotonRadius2)) {
					hp->accumPhotonCount++;

					hp->accumReflectedFlux += photonFlux * hp->material->f(hp->wo, wi, shadeN) *
							luxrays::AbsDot(shadeN, wi) * hp->throughput;
				}
			}
		}
	}

private:
	unsigned int Hash(const int ix, const int iy, const int iz) {
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % hashGridSize;
	}

	HitPoints *hitPoints;
	unsigned int hashGridSize;
	float invCellSize;
	std::list<HitPoint *> **hashGrid;
};

//------------------------------------------------------------------------------

static std::string SPPMG_LABEL = "LuxRays SmallSPPMGPU v" LUXRAYS_VERSION_MAJOR "." LUXRAYS_VERSION_MINOR " (LuxRays demo: http://www.luxrender.net)";

static luxrays::Context *ctx = NULL;
static luxrays::sdl::Scene *scene = NULL;

static double startTime = 0.0;
static unsigned int scrRefreshInterval = 2000;

static unsigned int imgWidth = 640;
static unsigned int imgHeight = 480;
static std::string imgFileName = "image.png";
static float photonAlpha = 0.7f;
static unsigned int stochasticInterval = 2000000;

static luxrays::utils::Film *film = NULL;
static luxrays::SampleBuffer *sampleBuffer = NULL;

static boost::thread *renderThread = NULL;
static unsigned long long photonTraced = 0;

static HitPoints *hitPoints = NULL;
static HashGrid *hashGrid = NULL;

//------------------------------------------------------------------------------

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

//------------------------------------------------------------------------------

static void UpdateFrameBuffer() {
	if (!film)
		return;

	film->Reset();

	for (unsigned int i = 0; i < hitPoints->GetSize(); ++i) {
		HitPoint *hp = hitPoints->GetHitPoint(i);
		const float scrX = i % imgWidth;
		const float scrY = i / imgWidth;
		sampleBuffer->SplatSample(scrX, scrY, hp->radiance);

		if (sampleBuffer->IsFull()) {
			// Splat all samples on the film
			film->SplatSampleBuffer(true, sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
	}

	if (sampleBuffer->GetSampleCount() > 0) {
		// Splat all samples on the film
		film->SplatSampleBuffer(true, sampleBuffer);
		sampleBuffer = film->GetFreeSampleBuffer();
	}
}

//------------------------------------------------------------------------------

static void InitPhotonPath(luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
	PhotonPath *photonPath, luxrays::Ray *ray) {
	// Select one light source
	float lpdf;
	const luxrays::sdl::LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lpdf);

	// Initialize the photon path
	float pdf;
	photonPath->flux = light->Sample_L(scene,
		rndGen->floatValue(), rndGen->floatValue(),
		rndGen->floatValue(), rndGen->floatValue(),
		&pdf, ray);
	photonPath->flux /= pdf * lpdf;
	photonPath->depth = 0;

	photonTraced++;
}

static void TracePhotonsThread(luxrays::RandomGenerator *rndGen, luxrays::IntersectionDevice *device) {
	std::cerr << "Tracing photon paths" << std::endl;

	luxrays::RayBuffer *rayBuffer = device->NewRayBuffer();

	// Generate photons from light sources
	std::vector<PhotonPath> photonPaths(rayBuffer->GetSize());
	luxrays::Ray *rays = rayBuffer->GetRayBuffer();
	for (unsigned int i = 0; i < photonPaths.size(); ++i) {
		// Note: there is some assumption here about how the
		// rayBuffer->ReserveRay() work
		InitPhotonPath(scene, rndGen, &photonPaths[i], &rays[rayBuffer->ReserveRay()]);
	}

	startTime = luxrays::WallClockTime();
	unsigned long long lastEyePathRecast = photonTraced;
	while (!boost::this_thread::interruption_requested()) {
		// Trace the rays
		device->PushRayBuffer(rayBuffer);
		rayBuffer = device->PopRayBuffer();

		for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i) {
			PhotonPath *photonPath = &photonPaths[i];
			luxrays::Ray *ray = &rayBuffer->GetRayBuffer()[i];
			const luxrays::RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];

			if (rayHit->Miss()) {
				// Re-initialize the photon path
				InitPhotonPath(scene, rndGen, photonPath, ray);
			} else {
				// Something was hit
				luxrays::Point hitPoint;
				luxrays::Spectrum surfaceColor;
				luxrays::Normal N, shadeN;
				if (GetHitPointInformation(scene, rndGen, ray, rayHit, hitPoint,
						surfaceColor, N, shadeN))
					continue;

				// Get the material
				const unsigned int currentTriangleIndex = rayHit->index;
				const luxrays::sdl::Material *triMat = scene->triangleMaterials[currentTriangleIndex];

				if (triMat->IsLightSource()) {
					// Re-initialize the photon path
					InitPhotonPath(scene, rndGen, photonPath, ray);
				} else {
					const luxrays::sdl::SurfaceMaterial *triSurfMat = (luxrays::sdl::SurfaceMaterial *)triMat;

					float fPdf;
					luxrays::Vector wi;
					bool specularBounce;
					const luxrays::Spectrum f = triSurfMat->Sample_f(-ray->d, &wi, N, shadeN,
							rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
							false, &fPdf, specularBounce) * surfaceColor;

					if (!specularBounce)
						hashGrid->AddFlux(photonAlpha, hitPoint, shadeN, -ray->d, photonPath->flux);

					// Check if we reached the max. depth
					if (photonPath->depth < MAX_PHOTON_PATH_DEPTH) {
						// Build the next vertex path ray
						if ((fPdf <= 0.f) || f.Black()) {
							// Re-initialize the photon path
							InitPhotonPath(scene, rndGen, photonPath, ray);
						} else {
							photonPath->depth++;
							photonPath->flux *= f / fPdf;

							// Russian Roulette
							const float p = 0.75f;
							if (photonPath->depth < 3) {
								*ray = luxrays::Ray(hitPoint, wi);
							} else if (rndGen->floatValue() < p) {
								photonPath->flux /= p;
								*ray = luxrays::Ray(hitPoint, wi);
							} else {
								// Re-initialize the photon path
								InitPhotonPath(scene, rndGen, photonPath, ray);
							}
						}
					} else {
						// Re-initialize the photon path
						InitPhotonPath(scene, rndGen, photonPath, ray);
					}
				}
			}
		}

		// Check if it is time to do an HashGrid re-hash
		// TODO: add a parameter to tune rehashing intervals
		if (photonTraced - lastEyePathRecast > stochasticInterval) {
			hitPoints->Recast(photonTraced);

			hashGrid->Rehash();

			lastEyePathRecast = photonTraced;
			std::cerr << "Tracing photon paths" << std::endl;
		}
	}

	delete rayBuffer;
	delete rndGen;
}

//------------------------------------------------------------------------------

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, imgHeight - 15, imgWidth - 1, imgHeight - 1);
	glRecti(0, 0, imgWidth - 1, 18);
	glDisable(GL_BLEND);

	// Title
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, imgHeight - 10);
	PrintString(GLUT_BITMAP_8_BY_13, SPPMG_LABEL.c_str());

	// Stats
	glRasterPos2i(4, 5);
	char captionBuffer[512];
	const double elapsedTime = luxrays::WallClockTime() - startTime;
	const unsigned int kPhotonsSec = photonTraced / (elapsedTime * 1000.f);
	sprintf(captionBuffer, "[Photons %.2fM][Avg. photons/sec % 4dK][Elapsed time %dsecs]",
		float(photonTraced / 1000000.0), kPhotonsSec, int(elapsedTime));
	PrintString(GLUT_BITMAP_8_BY_13, captionBuffer);
}

static void DisplayFunc(void) {
	if (film) {
		film->UpdateScreenBuffer();
		glRasterPos2i(0, 0);
		glDrawPixels(imgWidth, imgHeight, GL_RGB, GL_FLOAT, film->GetScreenBuffer());

		PrintCaptions();
	} else
		glClear(GL_COLOR_BUFFER_BIT);

	glutSwapBuffers();
}

static void KeyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p':
			film->UpdateScreenBuffer();
			film->Save(imgFileName);
			break;
		case 27: { // Escape key
			// Stop photon tracing thread

			if (renderThread) {
				renderThread->interrupt();
				renderThread->join();
				delete renderThread;
			}

			film->UpdateScreenBuffer();
			film->Save(imgFileName);
			film->FreeSampleBuffer(sampleBuffer);
			delete film;

			ctx->Stop();
			delete scene;

			delete hashGrid;
			delete hitPoints;

			delete ctx;

			std::cerr << "Done." << std::endl;
			exit(EXIT_SUCCESS);
			break;
		}
		default:
			break;
	}

	DisplayFunc();
}

static void TimerFunc(int value) {
	UpdateFrameBuffer();

	glutPostRedisplay();

	glutTimerFunc(scrRefreshInterval, TimerFunc, 0);
}

static void InitGlut(int argc, char *argv[], const unsigned int width, const unsigned int height) {
	glutInit(&argc, argv);

	glutInitWindowSize(width, height);
	// Center the window
	unsigned int scrWidth = glutGet(GLUT_SCREEN_WIDTH);
	unsigned int scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
	if ((scrWidth + 50 < width) || (scrHeight + 50 < height))
		glutInitWindowPosition(0, 0);
	else
		glutInitWindowPosition((scrWidth - width) / 2, (scrHeight - height) / 2);

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow(SPPMG_LABEL.c_str());
}

static void RunGlut(const unsigned int width, const unsigned int height) {
	glutKeyboardFunc(KeyFunc);
	glutDisplayFunc(DisplayFunc);

	glutTimerFunc(scrRefreshInterval, TimerFunc, 0);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, width, height);
	glLoadIdentity();
	glOrtho(0.f, width - 1.f,
			0.f, height - 1.f, -1.f, 1.f);

	glutMainLoop();
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
	std::cerr << SPPMG_LABEL << std::endl;

	try {
		//----------------------------------------------------------------------
		// Parse command line options
		//----------------------------------------------------------------------

		std::cerr << "Usage: " << argv[0] << " [options] [scene file]" << std::endl <<
				" -w [image width]" << std::endl <<
				" -e [image height]" << std::endl <<
				" -a [photon alpha]" << std::endl <<
				" -r [screen refresh interval]" << std::endl <<
				" -i [image file name]" << std::endl <<
				" -s [stochastic photon count refresh]" << std::endl <<
				" -h <display this help and exit>" << std::endl;

		std::string sceneFileName = "scenes/cornell/cornell.scn";
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') exit(EXIT_SUCCESS);

				else if (argv[i][1] == 'e') imgHeight = atoi(argv[++i]);

				else if (argv[i][1] == 'w') imgWidth = atoi(argv[++i]);

				else if (argv[i][1] == 'a') photonAlpha = atof(argv[++i]);

				else if (argv[i][1] == 'r') scrRefreshInterval = atoi(argv[++i]);

				else if (argv[i][1] == 'i') imgFileName = argv[++i];

				else if (argv[i][1] == 's') stochasticInterval = atoi(argv[++i]);

				else {
					std::cerr << "Invalid option: " << argv[i] << std::endl;
					exit(EXIT_FAILURE);
				}
			} else {
				std::string s = argv[i];
				if ((s.length() >= 4) && (s.substr(s.length() - 4) == ".scn")) {
					sceneFileName = s;
				} else
					throw std::runtime_error("Unknow file extension: " + s);
			}
		}

		//--------------------------------------------------------------------------
		// Init GLUT
		//--------------------------------------------------------------------------

		InitGlut(argc, argv, imgWidth, imgHeight);

		//--------------------------------------------------------------------------
		// Create the LuxRays context
		//--------------------------------------------------------------------------

		ctx = new luxrays::Context(DebugHandler);

		// Looks for the first GPU device
		std::vector<luxrays::DeviceDescription *> interDevDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());
		//luxrays::DeviceDescription::FilterOne(interDevDescs);

		luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, interDevDescs);

		//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL, interDevDescs);
		//luxrays::OpenCLDeviceDescription::Filter(luxrays::OCL_DEVICE_TYPE_GPU, interDevDescs);

		if (interDevDescs.size() < 1) {
			std::cerr << "Unable to find a GPU or CPU intersection device" << std::endl;
			return (EXIT_FAILURE);
		}
		interDevDescs.resize(1);

		std::cerr << "Selected intersection device: " << interDevDescs[0]->GetName();
		std::vector<luxrays::IntersectionDevice *> devices = ctx->AddIntersectionDevices(interDevDescs);
		luxrays::IntersectionDevice *device = devices[0];

		luxrays::RandomGenerator *rndGen = new luxrays::RandomGenerator();
		rndGen->init(7);

		//----------------------------------------------------------------------
		// Allocate the Film
		//----------------------------------------------------------------------

		std::vector<luxrays::DeviceDescription *> pixelDevDecs = ctx->GetAvailableDeviceDescriptions();
		luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, pixelDevDecs);
		film = new luxrays::utils::LuxRaysFilm(ctx, imgWidth, imgHeight, pixelDevDecs[0]);
		//film->SetFilterType(luxrays::FILTER_NONE);
		sampleBuffer = film->GetFreeSampleBuffer();

		//----------------------------------------------------------------------
		// Read the scene
		//----------------------------------------------------------------------

		scene = new luxrays::sdl::Scene(ctx, sceneFileName);
		scene->camera->Update(imgWidth, imgHeight);

		// Set the Luxrays SataSet
		ctx->SetDataSet(scene->dataSet);
		ctx->Start();

		//----------------------------------------------------------------------
		// Build the EyePaths list
		//----------------------------------------------------------------------

		hitPoints = new HitPoints(scene, rndGen, device, photonAlpha, imgWidth, imgHeight);

		//----------------------------------------------------------------------
		// Build the hash grid of EyePaths hit points
		//----------------------------------------------------------------------

		hashGrid = new HashGrid(hitPoints);

		//----------------------------------------------------------------------
		// Start photon tracing thread
		//----------------------------------------------------------------------

		renderThread = new boost::thread(boost::bind(TracePhotonsThread, rndGen, device));

		//----------------------------------------------------------------------
		// Start GLUT loop
		//----------------------------------------------------------------------

		RunGlut(imgWidth, imgHeight);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		std::cerr << "OpenCL ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
#endif
	} catch (std::runtime_error err) {
		std::cerr << "RUNTIME ERROR: " << err.what() << std::endl;
	} catch (std::exception err) {
		std::cerr << "ERROR: " << err.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
