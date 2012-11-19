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

#include "bidirvmcpu/bidirvmcpu.h"

HashGrid::HashGrid(vector<vector<PathVertexVM> > &pathsVertices, const float radius) {
	radius2 = radius * radius;

	// Build the vertices bounding box
	u_int vertexCount = 0;
	for (u_int i = 0; i < pathsVertices.size(); ++i) {
		vertexCount += pathsVertices[i].size();

		for (u_int j = 0; j < pathsVertices[i].size(); ++j)
			vertexBBox = Union(vertexBBox, pathsVertices[i][j].bsdf.hitPoint);
	}

	if (vertexCount <= 0)
		return;

	// Calculate the size of the grid cell
	const float cellSize = radius * 2.f;
	invCellSize = 1.f / cellSize;

	gridSize = vertexCount;
	grid.resize(gridSize, NULL);

	for (u_int i = 0; i < pathsVertices.size(); ++i) {
		for (u_int j = 0; j < pathsVertices[i].size(); ++j) {
			PathVertexVM *vertex = &pathsVertices[i][j];

			const Vector rad(radius, radius, radius);
			const Vector bMin = ((vertex->bsdf.hitPoint - rad) - vertexBBox.pMin) * invCellSize;
			const Vector bMax = ((vertex->bsdf.hitPoint + rad) - vertexBBox.pMin) * invCellSize;

			for (int iz = abs(int(bMin.z)); iz <= abs(int(bMax.z)); ++iz) {
				for (int iy = abs(int(bMin.y)); iy <= abs(int(bMax.y)); ++iy) {
					for (int ix = abs(int(bMin.x)); ix <= abs(int(bMax.x)); ++ix) {
						int hv = Hash(ix, iy, iz);

						if (grid[hv] == NULL)
							grid[hv] = new list<PathVertexVM *>();

						grid[hv]->push_front(vertex);
					}
				}
			}
		}
	}
}

HashGrid::~HashGrid() {
	for (unsigned int i = 0; i < gridSize; ++i)
		delete grid[i];
}

void HashGrid::Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const {
	Vector hh = (eyeVertex.bsdf.hitPoint - vertexBBox.pMin) * invCellSize;
	const int ix = abs(int(hh.x));
	const int iy = abs(int(hh.y));
	const int iz = abs(int(hh.z));

	const list<PathVertexVM *> *lightVertices = grid[Hash(ix, iy, iz)];

	if (lightVertices) {
		BiDirVMCPURenderEngine *engine = (BiDirVMCPURenderEngine *)thread->renderEngine;

		list<PathVertexVM *>::const_iterator iter = lightVertices->begin();
		while (iter != lightVertices->end()) {
			const PathVertexVM *lightVertex = *iter++;

			const float distance2 = (lightVertex->bsdf.hitPoint - eyeVertex.bsdf.hitPoint).LengthSquared();
			if (distance2 <= radius2) {
				// It is a valid light vertex

				float eyeBsdfPdfW, eyeBsdfRevPdfW;
				BSDFEvent eyeEvent;
				const Spectrum eyeBsdfEval = eyeVertex.bsdf.Evaluate(lightVertex->bsdf.fixedDir,
						&eyeEvent, &eyeBsdfPdfW, &eyeBsdfRevPdfW);
				if(eyeBsdfEval.Black())
					continue;

				if (eyeVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = Max(eyeBsdfEval.Filter(), engine->rrImportanceCap);
					eyeBsdfPdfW *= prob;
					eyeBsdfRevPdfW *= prob; // TODO: fix
				}

				// MIS weights
				const float weightLight = lightVertex->dVCM * thread->misVcWeightFactor +
					lightVertex->dVM * BiDirVMCPURenderThread::MIS(eyeBsdfPdfW);
				const float weightCamera = eyeVertex.dVCM * thread->misVcWeightFactor +
					eyeVertex.dVM * BiDirVMCPURenderThread::MIS(eyeBsdfRevPdfW);
				const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

				*radiance += (thread->vmNormalization * misWeight) *
						eyeVertex.throughput * eyeBsdfEval * lightVertex->throughput;
			}
		}
	}
}
