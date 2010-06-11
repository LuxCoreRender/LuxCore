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

#include "smallsppmgpu.h"
#include "displayfunc.h"

bool GetHitPointInformation(const luxrays::sdl::Scene *scene, luxrays::RandomGenerator *rndGen,
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

//------------------------------------------------------------------------------

HitPoints::HitPoints(luxrays::sdl::Scene *scn, luxrays::RandomGenerator *rndg,
		luxrays::IntersectionDevice *dev, const float a,
		const unsigned int w, const unsigned int h,
		const LookUpAccelType accelType) {
	scene = scn;
	rndGen = rndg;
	device = dev;
	rayBuffer = device->NewRayBuffer();
	alpha = a;
	width = w;
	height = h;
	pass = 0;
	lookUpAccelType = accelType;

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

	// Allocate hit points lookup accelerator
	if (lookUpAccelType == HASH_GRID)
		lookUpAccel = new HashGrid(this);
	else
		lookUpAccel = new KdTree(this);
}

HitPoints::~HitPoints() {
	delete lookUpAccel;
	delete hitPoints;
	delete rayBuffer;
}

void HitPoints::AccumulateFlux(const unsigned int photonTraced) {
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

void HitPoints::SetHitPoints() {
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

//------------------------------------------------------------------------------

HitPointsLookUpAccel::HitPointsLookUpAccel() {
}

HitPointsLookUpAccel::~HitPointsLookUpAccel() {
}

//------------------------------------------------------------------------------

HashGrid::HashGrid(HitPoints *hps) {
	hitPoints = hps;
	hashGrid = NULL;

	Refresh();
}

HashGrid::~HashGrid() {
	for (unsigned int i = 0; i < hashGridSize; ++i)
		delete hashGrid[i];
	delete hashGrid;
}

void HashGrid::Refresh() {
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
	std::cerr << "Hash grid cell size: " << cellSize << std::endl;
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

void HashGrid::AddFlux(const float alpha, const luxrays::Point &hitPoint, const luxrays::Normal &shadeN,
	const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
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

//------------------------------------------------------------------------------

KdTree::KdTree(HitPoints *hps) {
	hitPoints = hps;
	nNodes = hitPoints->GetSize();
	nextFreeNode = 1;
	nodes = NULL;
	nodeData = NULL;

	Refresh();
}

KdTree::~KdTree() {
	delete[] nodes;
	delete[] nodeData;
}

bool KdTree::CompareNode::operator ()(const HitPoint *d1, const HitPoint *d2) const {
	return (d1->position[axis] == d2->position[axis]) ? (d1 < d2) :
			(d1->position[axis] < d2->position[axis]);
}

void KdTree::RecursiveBuild(const unsigned int nodeNum, const unsigned int start,
		const unsigned int end, std::vector<HitPoint *> &buildNodes) {
	assert (nodeNum >= 0);
	assert (start >= 0);
	assert (end >= 0);
	assert (nodeNum < nNodes);
	assert (start < nNodes);
	assert (end <= nNodes);

	// Create leaf node of kd-tree if we've reached the bottom
	if (start + 1 == end) {
		nodes[nodeNum].initLeaf();
		nodeData[nodeNum] = buildNodes[start];
		return;
	}

	// Choose split direction and partition data
	// Compute bounds of data from start to end
	luxrays::BBox bound;
	for (unsigned int i = start; i < end; ++i)
		bound = luxrays::Union(bound, buildNodes[i]->position);
	unsigned int splitAxis = bound.MaximumExtent();
	unsigned int splitPos = (start + end) / 2;

	std::nth_element(&buildNodes[start], &buildNodes[splitPos],
		&buildNodes[end - 1], CompareNode(splitAxis));

	// Allocate kd-tree node and continue recursively
	nodes[nodeNum].init(buildNodes[splitPos]->position[splitAxis], splitAxis);
	nodeData[nodeNum] = buildNodes[splitPos];

	if (start < splitPos) {
		nodes[nodeNum].hasLeftChild = 1;
		unsigned int childNum = nextFreeNode++;
		RecursiveBuild(childNum, start, splitPos, buildNodes);
	}

	if (splitPos + 1 < end) {
		nodes[nodeNum].rightChild = nextFreeNode++;
		RecursiveBuild(nodes[nodeNum].rightChild, splitPos+1, end, buildNodes);
	}
}

void KdTree::Refresh() {
	delete[] nodes;
	delete[] nodeData;

	std::cerr << "Building kD-Tree with " << nNodes << " nodes" << std::endl;

	nodes = new KdNode[nNodes];
	nodeData = new HitPoint*[nNodes];
	nextFreeNode = 1;

	// Begin the KdTree building process
	std::vector<HitPoint *> buildNodes;
	buildNodes.reserve(nNodes);
	maxDistSquared = 0.f;
	for (unsigned int i = 0; i < nNodes; ++i)  {
		buildNodes.push_back(hitPoints->GetHitPoint(i));
		maxDistSquared = luxrays::Max(maxDistSquared, buildNodes[i]->accumPhotonRadius2);
	}
	std::cerr << "kD-Tree search radius: " << sqrtf(maxDistSquared) << std::endl;

	RecursiveBuild(0, 0, nNodes, buildNodes);
}

void KdTree::AddFluxImpl(const unsigned int nodeNum,
		const float alpha, const luxrays::Point &p, const luxrays::Normal &shadeN,
		const luxrays::Vector &wi, const luxrays::Spectrum &photonFlux) {
	KdNode *node = &nodes[nodeNum];

	// Process kd-tree node's children
	int axis = node->splitAxis;
	if (axis != 3) {
		float dist = p[axis] - node->splitPos;
		float dist2 = dist * dist;
		if (p[axis] <= node->splitPos) {
			if (node->hasLeftChild)
				AddFluxImpl(nodeNum + 1, alpha, p, shadeN, wi, photonFlux);
			if (dist2 < maxDistSquared && node->rightChild < nNodes)
				AddFluxImpl(node->rightChild, alpha, p, shadeN, wi, photonFlux);
		} else {
			if (node->rightChild < nNodes)
				AddFluxImpl(node->rightChild, alpha, p, shadeN, wi, photonFlux);
			if (dist2 < maxDistSquared && node->hasLeftChild)
				AddFluxImpl(nodeNum + 1, alpha, p, shadeN, wi, photonFlux);
		}
	}

	// Hand kd-tree node to processing function
	HitPoint *hp = nodeData[nodeNum];
	const float dist2 = luxrays::DistanceSquared(hp->position, p);
	// TODO: use configurable parameter for normal treshold
	if ((luxrays::Dot(hp->normal, shadeN) > 0.5f) &&
			(dist2 <=  hp->accumPhotonRadius2)) {
		hp->accumPhotonCount++;

		hp->accumReflectedFlux += photonFlux * hp->material->f(hp->wo, wi, shadeN) *
				luxrays::AbsDot(shadeN, wi) * hp->throughput;
	}
}
