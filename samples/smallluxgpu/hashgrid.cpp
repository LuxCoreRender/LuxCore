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

HashGrid::HashGrid(HitPoints *hps) {
	hitPoints = hps;
	grid = NULL;

	RefreshMutex();
}

HashGrid::~HashGrid() {
	for (unsigned int i = 0; i < gridSize; ++i)
		delete grid[i];
	delete[] grid;
}

void HashGrid::RefreshMutex() {
	const unsigned int hitPointsCount = hitPoints->GetSize();
	const luxrays::BBox &hpBBox = hitPoints->GetBBox();

	// Calculate the size of the grid cell
	const float maxPhotonRadius2 = hitPoints->GetMaxPhotonRaidus2();
	const float cellSize = sqrtf(maxPhotonRadius2) * 2.f;
	std::cerr << "Hash grid cell size: " << cellSize << std::endl;
	invCellSize = 1.f / cellSize;

	// TODO: add a tunable parameter for hashgrid size
	gridSize = hitPointsCount;
	if (!grid) {
		grid = new std::list<HitPoint *>*[gridSize];

		for (unsigned int i = 0; i < gridSize; ++i)
			grid[i] = NULL;
	} else {
		for (unsigned int i = 0; i < gridSize; ++i) {
			delete grid[i];
			grid[i] = NULL;
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

						if (grid[hv] == NULL)
							grid[hv] = new std::list<HitPoint *>();

						grid[hv]->push_front(hp);
						++entryCount;

						/*// grid[hv]->size() is very slow to execute
						if (grid[hv]->size() > maxPathCount)
							maxPathCount = grid[hv]->size();*/
					}
				}
			}
		}
	}
	//std::cerr << "Max. hit points in a single hash grid entry: " << maxPathCount << std::endl;
	std::cerr << "Total hash grid entry: " << entryCount << std::endl;
	std::cerr << "Avg. hit points in a single hash grid entry: " << entryCount / gridSize << std::endl;

	// HashGrid debug code
	/*for (unsigned int i = 0; i < hashGridSize; ++i) {
		if (grid[i]) {
			if (grid[i]->size() > 10) {
				std::cerr << "HashGrid[" << i << "].size() = " <<grid[i]->size() << std::endl;
			}
		}
	}*/
}

void HashGrid::AddFlux(const luxrays::Point &hitPoint, const luxrays::Vector &wi,
		const luxrays::Spectrum &photonFlux) {
	// Look for eye path hit points near the current hit point
	luxrays::Vector hh = (hitPoint - hitPoints->GetBBox().pMin) * invCellSize;
	const int ix = abs(int(hh.x));
	const int iy = abs(int(hh.y));
	const int iz = abs(int(hh.z));

	std::list<HitPoint *> *hps = grid[Hash(ix, iy, iz)];
	if (hps) {
		std::list<HitPoint *>::iterator iter = hps->begin();
		while (iter != hps->end()) {
			HitPoint *hp = *iter++;

			const float dist2 = luxrays::DistanceSquared(hp->position, hitPoint);
			if ((dist2 >  hp->accumPhotonRadius2))
				continue;

			const float dot = luxrays::Dot(hp->normal, wi);
			if (dot <= 0.0001f)
				continue;

			AtomicInc(&hp->accumPhotonCount);
			luxrays::Spectrum flux = photonFlux * hp->material->f(hp->wo, wi, hp->normal) *
					dot * hp->throughput;
			AtomicAdd(&hp->accumReflectedFlux.r, flux.r);
			AtomicAdd(&hp->accumReflectedFlux.g, flux.g);
			AtomicAdd(&hp->accumReflectedFlux.b, flux.b);
		}
	}
}
