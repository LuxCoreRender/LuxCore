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

enum EyePathState {
	TO_TRACE, HIT, CONSTANT_COLOR
};

class EyePath {
public:
	EyePathState state;

	// Screen information
	float scrX, scrY;

	// Eye path information
	luxrays::Ray ray;
	luxrays::RayHit rayHit;
	unsigned int depth;
	luxrays::Spectrum throughput;
	luxrays::Spectrum constantColor;

	// hit point information
	luxrays::Point position;
	luxrays::Normal normal;
	const luxrays::sdl::SurfaceMaterial *material;

	float photonRadius2;
	unsigned int accumPhotonCount;
	luxrays::Spectrum accumReflectedFlux;
};

class PhotonPath {
public:
	// The ray is stored in the RayBuffer and the index is implicitly stored
	// in the array of PhotonPath
	luxrays::Spectrum flux;
	unsigned int depth;
};

class HashGrid {
public:
	HashGrid(const float s, const luxrays::BBox bb, std::vector<std::vector<EyePath *> > *grid) {
		invHashSize = s;
		gridBBox = bb;
		hashGrid = grid;
	}

	~HashGrid() {
		delete hashGrid;
	}

	float invHashSize;
	luxrays::BBox gridBBox;
	std::vector<std::vector<EyePath *> > *hashGrid;
};

//------------------------------------------------------------------------------

static std::string SPPMG_LABEL = "LuxRays SmallPPMGPU v" LUXRAYS_VERSION_MAJOR "." LUXRAYS_VERSION_MINOR " (LuxRays demo: http://www.luxrender.net)";

static luxrays::Context *ctx = NULL;
static luxrays::sdl::Scene *scene = NULL;

static double startTime = 0.0;
static unsigned int scrRefreshInterval = 2000;

static unsigned int imgWidth = 640;
static unsigned int imgHeight = 480;
static unsigned int imgSuperSampling = 2;
static std::string imgFileName = "image.png";
static luxrays::utils::Film *film = NULL;
static luxrays::SampleBuffer *sampleBuffer = NULL;

static boost::thread *renderThread = NULL;
static unsigned int photonTraced = 0;

static std::vector<EyePath> *eyePathsPtr = NULL;

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

	std::vector<EyePath> &eyePaths = *eyePathsPtr;
	for (unsigned int i = 0; i < eyePaths.size(); ++i) {
		EyePath *eyePath = &eyePaths[i];

		switch (eyePath->state) {
			case CONSTANT_COLOR: {
				sampleBuffer->SplatSample(eyePath->scrX, eyePath->scrY, eyePath->constantColor);
				break;
			}
			case HIT: {
				const luxrays::Spectrum rad = (eyePath->accumPhotonCount == 0) ? luxrays::Spectrum() :
					(eyePath->accumReflectedFlux / (M_PI * eyePath->photonRadius2 * photonTraced));
				sampleBuffer->SplatSample(eyePath->scrX, eyePath->scrY, rad);
				break;
			}
			default:
				assert(false);
				break;
		}

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

static std::vector<EyePath> *BuildEyePaths(luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
	luxrays::IntersectionDevice *device, luxrays::RayBuffer *rayBuffer,
	const unsigned int width, const unsigned int height) {
	std::vector<EyePath> *eyePathsPtr = new std::vector<EyePath>(width * height * imgSuperSampling * imgSuperSampling);
	std::vector<EyePath> &eyePaths = *eyePathsPtr;
	std::list<EyePath *> todoEyePaths;

	// Generate eye rays
	std::cerr << "Building eye paths rays with " << imgSuperSampling << "x" << imgSuperSampling << " super-sampling:" << std::endl;
	std::cerr << "  0/" << height << std::endl;
	double lastPrintTime = luxrays::WallClockTime();
	unsigned int index = 0;
	const float invImgSuperSampling = 1.f / imgSuperSampling;
	for (unsigned int y = 0; y < height; ++y) {
		if (luxrays::WallClockTime() - lastPrintTime > 2.0) {
			std::cerr << "  " << y << "/" << height << std::endl;
			lastPrintTime = luxrays::WallClockTime();
		}

		for (unsigned int x = 0; x < width; ++x) {
			for (unsigned int sy = 0; sy < imgSuperSampling; ++sy) {
				for (unsigned int sx = 0; sx < imgSuperSampling; ++sx) {
					EyePath *eyePath = &eyePaths[index++];

					eyePath->scrX = x + (sx + rndGen->floatValue()) * invImgSuperSampling - 0.5f;
					eyePath->scrY = y + (sy + rndGen->floatValue()) * invImgSuperSampling - 0.5f;
					scene->camera->GenerateRay(eyePath->scrX, eyePath->scrY, width, height, &eyePath->ray,
							rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue());

					eyePath->depth = 0;
					eyePath->throughput = luxrays::Spectrum(1.f, 1.f, 1.f);
					eyePath->constantColor = luxrays::Spectrum(0.f, 0.f, 0.f);
					eyePath->state = TO_TRACE;

					todoEyePaths.push_back(eyePath);
				}
			}
		}
	}

	// Iterate through all eye paths
	std::cerr << "Building eye paths hit points: " << std::endl;
	bool done;
	lastPrintTime = luxrays::WallClockTime();
	// Note: (todoEyePaths.size() > 0) is extremly slow to execute
	unsigned int todoEyePathCount = width * height * imgSuperSampling * imgSuperSampling;
	std::cerr << "  " << todoEyePathCount / 1000 << "k eye paths left" << std::endl;
	while(todoEyePathCount > 0) {
		if (luxrays::WallClockTime() - lastPrintTime > 2.0) {
			std::cerr << "  " << todoEyePathCount / 1000 << "k eye paths left" << std::endl;
			lastPrintTime = luxrays::WallClockTime();
		}

		std::list<EyePath *>::iterator todoEyePathsIterator = todoEyePaths.begin();
		while (todoEyePathsIterator != todoEyePaths.end()) {
			EyePath *eyePath = *todoEyePathsIterator;

			assert (eyePath->state == TO_TRACE);

			// Check if we reached the max path depth
			if (eyePath->depth > MAX_EYE_PATH_DEPTH) {
				todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
				--todoEyePathCount;
				eyePath->state = CONSTANT_COLOR;
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

				assert (eyePath->state == TO_TRACE);

				const luxrays::RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];

				if (rayHit->Miss()) {
					todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
					--todoEyePathCount;
					if (scene->infiniteLight)
						eyePath->constantColor = scene->infiniteLight->Le(eyePath->ray.d) * eyePath->throughput;

					eyePath->state = CONSTANT_COLOR;
				} else {
					// Something was hit
					const luxrays::Point hitPoint = eyePath->ray(rayHit->t);
					const unsigned int currentTriangleIndex = rayHit->index;

					// Get the triangle
					const luxrays::ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
					const luxrays::Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

					// Get the material
					const luxrays::sdl::Material *triMat = scene->triangleMaterials[currentTriangleIndex];

					luxrays::Spectrum surfaceColor;
					const luxrays::Spectrum *colors = mesh->GetColors();
					if (colors)
						surfaceColor = luxrays::InterpolateTriColor(tri, colors, rayHit->b1, rayHit->b2);
					else
						surfaceColor = luxrays::Spectrum(1.f, 1.f, 1.f);

					// Interpolate face normal
					luxrays::Normal N = luxrays::InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);

					// Flip the normal if required
					luxrays::Normal shadeN;
					if (luxrays::Dot(eyePath->ray.d, N) > 0.f)
						shadeN = -N;
					else
						shadeN = N;

					if (triMat->IsLightSource()) {
						todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
						--todoEyePathCount;

						const luxrays::sdl::TriangleLight *tLight = (luxrays::sdl::TriangleLight *)triMat;
						eyePath->constantColor = tLight->Le(scene->objects, -eyePath->ray.d) * eyePath->throughput;
						eyePath->state = CONSTANT_COLOR;
					} else if (triMat->IsDiffuse()) {
						todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
						--todoEyePathCount;

						eyePath->rayHit = *rayHit;
						eyePath->material = (luxrays::sdl::SurfaceMaterial *)triMat;
						eyePath->throughput *= surfaceColor;
						eyePath->position = hitPoint;
						eyePath->normal = shadeN;
						eyePath->state = HIT;
					} else {
						// Build the next vertex path ray
						const luxrays::sdl::SurfaceMaterial *triSurfMat = (luxrays::sdl::SurfaceMaterial *)triMat;

						float fPdf;
						luxrays::Vector wi;
						bool specularBounce;
						const luxrays::Spectrum f = triSurfMat->Sample_f(-eyePath->ray.d, &wi, N, shadeN,
								rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
								true, &fPdf, specularBounce) * surfaceColor;
						if ((fPdf <= 0.f) || f.Black()) {
							todoEyePathsIterator = todoEyePaths.erase(todoEyePathsIterator);
							--todoEyePathCount;

							eyePath->state = CONSTANT_COLOR;
						} else {
							++todoEyePathsIterator;

							eyePath->throughput *= f / fPdf;
							eyePath->ray = luxrays::Ray(hitPoint, wi);
						}
					}
				}
			}

			rayBuffer->Reset();
		}
	}

	return eyePathsPtr;
}

static unsigned int Hash(const int ix, const int iy, const int iz, const unsigned int hashGridSize) {
	return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % hashGridSize;
}

static HashGrid *BuildHashGrid(
	std::vector<EyePath *> &hitPoints,
	const unsigned int width, const unsigned int height) {
	// Calculate hit points bounding box
	std::cerr << "Building hit points bounding box: ";
	luxrays::BBox hpBBox;
	for (unsigned int i = 0; i < hitPoints.size(); ++i)
		hpBBox = luxrays::Union(hpBBox, hitPoints[i]->position);
	std::cerr << hpBBox << std::endl;

	// Calculate initial radius
	luxrays::Vector ssize = hpBBox.pMax - hpBBox.pMin;
	const float photonRadius = ((ssize.x + ssize.y + ssize.z) / 3.f) / ((width * imgSuperSampling + height * imgSuperSampling) / 2.f) * 2.f;

	// Expand the bounding box by used radius
	hpBBox.Expand(photonRadius);

	// Initialize hit points field
	const float photonRadius2 = photonRadius * photonRadius;
	for (unsigned int i = 0; i < hitPoints.size(); ++i) {
		EyePath *eyePath = hitPoints[i];

		eyePath->photonRadius2 = photonRadius2;
		eyePath->accumPhotonCount = 0;
		eyePath->accumReflectedFlux = luxrays::Spectrum();
	}

	const float hashs = 1.f / (photonRadius * 2.f);
	// TODO: add a tunable parameter for hashgrid size
	const unsigned int hashGridSize = hitPoints.size();
	// TODO: optimize std::vector<std::vector<EyePath *> > type to save memory
	std::vector<std::vector<EyePath *> > *hashGridPtr = new std::vector<std::vector<EyePath *> >(hashGridSize);
	std::vector<std::vector<EyePath *> > &hashGrid = *hashGridPtr;

	std::cerr << "Building hit points hash grid...";
	const luxrays::Vector vphotonRadius(photonRadius, photonRadius, photonRadius);
	unsigned int maxPathCount = 0;
	for (unsigned int i = 0; i < hitPoints.size(); ++i) {
		EyePath *eyePath = hitPoints[i];

		const luxrays::Vector bMin = ((eyePath->position - vphotonRadius) - hpBBox.pMin) * hashs;
		const luxrays::Vector bMax = ((eyePath->position + vphotonRadius) - hpBBox.pMin) * hashs;

		for (int iz = abs(int(bMin.z)); iz <= abs(int(bMax.z)); iz++) {
			for (int iy = abs(int(bMin.y)); iy <= abs(int(bMax.y)); iy++) {
				for (int ix = abs(int(bMin.x)); ix <= abs(int(bMax.x)); ix++) {
					int hv = Hash(ix, iy, iz, hashGridSize);
					hashGrid[hv].push_back(eyePath);

					if (hashGrid[hv].size() > maxPathCount)
						maxPathCount = hashGrid[hv].size();
				}
			}
		}
	}
	std::cerr << "Done" << std::endl;
	std::cerr << "Max path count in a single hash grid entry: " << maxPathCount << std::endl;

	return new HashGrid(hashs, hpBBox, hashGridPtr);
}

static void InitPhotonPath(luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
	PhotonPath *photonPath, luxrays::Ray *ray) {
	// Select one light source
	const unsigned int lightIndex = scene->SampleLights(rndGen->floatValue());
	const luxrays::sdl::LightSource *light = scene->lights[lightIndex];

	// Initialize the photon path
	float pdf;
	photonPath->flux = light->Sample_L(scene->objects,
		rndGen->floatValue(), rndGen->floatValue(),
		rndGen->floatValue(), rndGen->floatValue(),
		&pdf, ray);
	photonPath->flux *= scene->lights.size() / pdf;
	photonPath->depth = 0;

	photonTraced++;
}

static void TracePhotonsThread(luxrays::RandomGenerator *rndGen,
	luxrays::IntersectionDevice *device, luxrays::RayBuffer *rayBuffer,
	HashGrid *hashGrid, const float alpha) {
	std::cerr << "Tracing photon paths" << std::endl;

	// Generate photons from light sources
	std::vector<PhotonPath> photonPaths(rayBuffer->GetSize());
	luxrays::Ray *rays = rayBuffer->GetRayBuffer();
	for (unsigned int i = 0; i < photonPaths.size(); ++i) {
		// Note: there is some assumption here about how the
		// rayBuffer->ReserveRay() work
		InitPhotonPath(scene, rndGen, &photonPaths[i], &rays[rayBuffer->ReserveRay()]);
	}

	startTime = luxrays::WallClockTime();
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
				const luxrays::Point hitPoint = (*ray)(rayHit->t);
				const unsigned int currentTriangleIndex = rayHit->index;

				// Get the triangle
				const luxrays::ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
				const luxrays::Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

				// Get the material
				const luxrays::sdl::Material *triMat = scene->triangleMaterials[currentTriangleIndex];

				luxrays::Spectrum surfaceColor;
				const luxrays::Spectrum *colors = mesh->GetColors();
				if (colors)
					surfaceColor = luxrays::InterpolateTriColor(tri, colors, rayHit->b1, rayHit->b2);
				else
					surfaceColor = luxrays::Spectrum(1.f, 1.f, 1.f);

				// Interpolate face normal
				luxrays::Normal N = luxrays::InterpolateTriNormal(tri, mesh->GetNormal(), rayHit->b1, rayHit->b2);

				// Flip the normal if required
				luxrays::Normal shadeN;
				if (luxrays::Dot(ray->d, N) > 0.f)
					shadeN = -N;
				else
					shadeN = N;

				if (triMat->IsLightSource()) {
					// Re-initialize the photon path
					InitPhotonPath(scene, rndGen, photonPath, ray);
				} else {
					if (triMat->IsDiffuse()) {
						// Look for eye path hit points near the current hit point
						luxrays::Vector hh = (hitPoint - hashGrid->gridBBox.pMin) * hashGrid->invHashSize;
						const int ix = abs(int(hh.x));
						const int iy = abs(int(hh.y));
						const int iz = abs(int(hh.z));

						std::vector<EyePath *> &eyePaths = (*(hashGrid->hashGrid))[Hash(ix, iy, iz, hashGrid->hashGrid->size())];
						for (unsigned int j = 0; j < eyePaths.size(); ++j) {
							EyePath *eyePath = eyePaths[j];
							luxrays::Vector v = eyePath->position - hitPoint;
							if ((luxrays::Dot(eyePath->normal, shadeN) > luxrays::RAY_EPSILON) &&
									(luxrays::Dot(v, v) <=  eyePath->photonRadius2)) {
								const float g = (eyePath->accumPhotonCount * alpha + alpha) / (eyePath->accumPhotonCount * alpha + 1.f);
								eyePath->photonRadius2 *= g;
								eyePath->accumPhotonCount++;

								luxrays::Spectrum f = eyePath->material->f(-eyePath->ray.d, -ray->d, shadeN);
								eyePath->accumReflectedFlux = (eyePath->accumReflectedFlux + photonPath->flux * f * eyePath->throughput) * g;
							}
						}
					}

					// Check if we reached the max. depth
					if (photonPath->depth < MAX_PHOTON_PATH_DEPTH) {
						// Build the next vertex path ray
						const luxrays::sdl::SurfaceMaterial *triSurfMat = (luxrays::sdl::SurfaceMaterial *)triMat;

						float fPdf;
						luxrays::Vector wi;
						bool specularBounce;
						const luxrays::Spectrum f = triSurfMat->Sample_f(-ray->d, &wi, N, shadeN,
								rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
								false, &fPdf, specularBounce) * surfaceColor;
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
	const double photonsSec = photonTraced / (luxrays::WallClockTime() - startTime);
	sprintf(captionBuffer, "[Photons %.2fM][Avg. photons/sec % 4dK]", float(photonTraced) / 1000000.f, int(photonsSec / 1000.f));
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
				" -s [super-sampling count]" << std::endl <<
				" -h <display this help and exit>" << std::endl;

		std::string sceneFileName = "scenes/cornell/cornell.scn";
		float photonAlpha = 0.7f;
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') exit(EXIT_SUCCESS);

				else if (argv[i][1] == 'e') imgHeight = atoi(argv[++i]);

				else if (argv[i][1] == 'w') imgWidth = atoi(argv[++i]);

				else if (argv[i][1] == 'a') photonAlpha = atof(argv[++i]);

				else if (argv[i][1] == 'r') scrRefreshInterval = atoi(argv[++i]);

				else if (argv[i][1] == 'i') imgFileName = argv[++i];

				else if (argv[i][1] == 's') imgSuperSampling = luxrays::Max(2, atoi(argv[++i]));

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

		luxrays::RayBuffer *rayBuffer = device->NewRayBuffer();

		luxrays::RandomGenerator *rndGen = new luxrays::RandomGenerator();
		rndGen->init(7);

		//----------------------------------------------------------------------
		// Read the scene
		//----------------------------------------------------------------------

		scene = new luxrays::sdl::Scene(ctx, sceneFileName);
		scene->camera->Update(imgWidth, imgHeight);

		// Set the Luxrays SataSet
		ctx->SetDataSet(scene->dataSet);
		ctx->Start();

		//----------------------------------------------------------------------
		// Allocate the Film
		//----------------------------------------------------------------------

		std::vector<luxrays::DeviceDescription *> pixelDevDecs = ctx->GetAvailableDeviceDescriptions();
		luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, pixelDevDecs);
		film = new luxrays::utils::LuxRaysFilm(ctx, imgWidth, imgHeight, pixelDevDecs[0]);
		sampleBuffer = film->GetFreeSampleBuffer();

		//----------------------------------------------------------------------
		// Build the EyePaths list
		//----------------------------------------------------------------------

		eyePathsPtr = BuildEyePaths(scene, rndGen, device, rayBuffer, imgWidth, imgHeight);
		std::vector<EyePath> &eyePaths = *eyePathsPtr;

		//----------------------------------------------------------------------
		// Build the hash grid of EyePaths hit points
		//----------------------------------------------------------------------

		// Build the list of EyePaths hit points
		std::vector<EyePath *> hitPoints;
		for (unsigned int i = 0; i < eyePaths.size(); ++i) {
			EyePath *eyePath = &eyePaths[i];

			if (eyePath->state == HIT)
				hitPoints.push_back(eyePath);
		}

		HashGrid *hashGrid = BuildHashGrid(hitPoints, imgWidth, imgHeight);

		//----------------------------------------------------------------------
		// Start photon tracing thread
		//----------------------------------------------------------------------

		renderThread = new boost::thread(boost::bind(TracePhotonsThread, rndGen, device, rayBuffer, hashGrid, photonAlpha));

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
