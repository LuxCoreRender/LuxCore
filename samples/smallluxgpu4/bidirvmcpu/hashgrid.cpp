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

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

void HashGrid::Build(vector<vector<PathVertexVM> > &pathsVertices, const float radius) {
	radius2 = radius * radius;

	// Build the vertices bounding box
	vertexCount = 0;
	vertexBBox = BBox();
	for (u_int i = 0; i < pathsVertices.size(); ++i) {
		vertexCount += pathsVertices[i].size();

		for (u_int j = 0; j < pathsVertices[i].size(); ++j)
			vertexBBox = Union(vertexBBox, pathsVertices[i][j].bsdf.hitPoint);
	}

	if (vertexCount <= 0)
		return;

	vertexBBox.Expand(radius + DEFAULT_EPSILON_STATIC);

	// Calculate the size of the grid cell
	const float cellSize = radius * 2.f;
	invCellSize = 1.f / cellSize;

	gridSize = vertexCount;	
	cellEnds.resize(gridSize);
	fill(cellEnds.begin(), cellEnds.end(), 0);
	lightVertices.resize(gridSize, NULL);

	for (u_int i = 0, k = 0; i < pathsVertices.size(); ++i) {
		for (u_int j = 0; j < pathsVertices[i].size(); ++j, ++k) {
			const PathVertexVM *vertex = &pathsVertices[i][j];

			cellEnds[Hash(vertex->bsdf.hitPoint)]++;
		}
	}

	int sum = 0;
	for (u_int i = 0; i < cellEnds.size(); ++i) {
		int temp = cellEnds[i];
		cellEnds[i] = sum;
		sum += temp;
	}
	
	for (u_int i = 0; i < pathsVertices.size(); ++i) {
		for (u_int j = 0; j < pathsVertices[i].size(); ++j) {
			const PathVertexVM *vertex = &pathsVertices[i][j];

			const int targetIdx = cellEnds[Hash(vertex->bsdf.hitPoint)]++;
            lightVertices[targetIdx] = vertex;
		}
	}
}

void HashGrid::Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const {
	if ((vertexCount <= 0) ||
			!vertexBBox.Inside(eyeVertex.bsdf.hitPoint))
		return;

	const Vector distMin = eyeVertex.bsdf.hitPoint - vertexBBox.pMin;
	const Vector cellPoint = invCellSize * distMin;
	const Vector coordFloor(floorf(cellPoint.x), floorf(cellPoint.y), floorf(cellPoint.z));

	const int px = int(coordFloor.x);
	const int py = int(coordFloor.y);
	const int pz = int(coordFloor.z);

	const Vector fractCoord = cellPoint - coordFloor;

	const int pxo = px + ((fractCoord.x < .5f) ? -1 : +1);
	const int pyo = py + ((fractCoord.y < .5f) ? -1 : +1);
	const int pzo = pz + ((fractCoord.z < .5f) ? -1 : +1);

	int i0, i1;
	HashRange(Hash(px, py, pz), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(px, py, pzo), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(px, pyo, pz), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(px, pyo, pzo), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(pxo, py, pz), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(pxo, py, pzo), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(pxo, pyo, pz), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
	HashRange(Hash(pxo, pyo, pzo), &i0, &i1);
	Process(thread, eyeVertex, i0, i1, radiance);
}

void HashGrid::Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, const int i0, const int i1,
		Spectrum *radiance) const {
	for (int i = i0; i < i1; ++i) {
		const PathVertexVM *lightVertex = lightVertices[i];
		Process(thread, eyeVertex, lightVertex, radiance);
	}
}

void HashGrid::Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, const PathVertexVM *lightVertex,
		Spectrum *radiance) const {
	const float distance2 = (lightVertex->bsdf.hitPoint - eyeVertex.bsdf.hitPoint).LengthSquared();

	if (distance2 <= radius2) {
		float eyeBsdfPdfW, eyeBsdfRevPdfW;
		BSDFEvent eyeEvent;
		const Spectrum eyeBsdfEval = eyeVertex.bsdf.Evaluate(lightVertex->bsdf.fixedDir,
				&eyeEvent, &eyeBsdfPdfW, &eyeBsdfRevPdfW);
		if(eyeBsdfEval.Black())
			return;

		BiDirVMCPURenderEngine *engine = (BiDirVMCPURenderEngine *)thread->renderEngine;
		if (eyeVertex.depth >= engine->rrDepth) {
			// Russian Roulette
			const float prob = Max(eyeBsdfEval.Filter(), engine->rrImportanceCap);
			eyeBsdfPdfW *= prob;
			eyeBsdfRevPdfW *= prob; // Note: SmallVCM uses light prob here
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
