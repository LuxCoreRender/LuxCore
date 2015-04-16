/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <boost/format.hpp>

#include "slg/engines/bidirvmcpu/bidirvmcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void HashGrid::Build(vector<vector<PathVertexVM> > &pathsVertices, const float radius) {
	// Reset statistic counters
	//mergeHitsV2V = 0;
	//mergeHitsV2S = 0;
	//mergeHitsS2S = 0;

	radius2 = radius * radius;

	// Build the vertices bounding box
	vertexCount = 0;
	vertexBBox = BBox();
	for (u_int i = 0; i < pathsVertices.size(); ++i) {
		vertexCount += pathsVertices[i].size();

		for (u_int j = 0; j < pathsVertices[i].size(); ++j)
			vertexBBox = Union(vertexBBox, pathsVertices[i][j].bsdf.hitPoint.p);
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

			cellEnds[Hash(vertex->bsdf.hitPoint.p)]++;
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

			const int targetIdx = cellEnds[Hash(vertex->bsdf.hitPoint.p)]++;
            lightVertices[targetIdx] = vertex;
		}
	}
}

void HashGrid::Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const {
	if ((vertexCount <= 0) ||
			!vertexBBox.Inside(eyeVertex.bsdf.hitPoint.p))
		return;

	const Vector distMin = eyeVertex.bsdf.hitPoint.p - vertexBBox.pMin;
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
	const float distance2 = (lightVertex->bsdf.hitPoint.p - eyeVertex.bsdf.hitPoint.p).LengthSquared();

	if (distance2 <= radius2) {
		float eyeBsdfPdfW, eyeBsdfRevPdfW;
		BSDFEvent eyeEvent;
		// I need to remove the dotN term from the result (see below)
		Spectrum eyeBsdfEval = eyeVertex.bsdf.Evaluate(lightVertex->bsdf.hitPoint.fixedDir,
				&eyeEvent, &eyeBsdfPdfW, &eyeBsdfRevPdfW);
		if(eyeBsdfEval.Black())
			return;
		
		// Volume BSDF doesn't multiply BSDF::Evaluate() by dotN so I need
		// to remove the term only if it isn't a Volume
		if (!eyeVertex.bsdf.IsVolume())
			eyeBsdfEval /= AbsDot(lightVertex->bsdf.hitPoint.fixedDir, eyeVertex.bsdf.hitPoint.geometryN);

		BiDirVMCPURenderEngine *engine = (BiDirVMCPURenderEngine *)thread->renderEngine;
		if (eyeVertex.depth >= engine->rrDepth) {
			// Russian Roulette
			const float prob = RenderEngine::RussianRouletteProb(eyeBsdfEval, engine->rrImportanceCap);
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

		// Statistics
		/*if (eyeVertex.bsdf.IsVolume()) {
			if (lightVertex->bsdf.IsVolume())
				++mergeHitsV2V;
			else
				++mergeHitsV2S;
		} else {
			if (lightVertex->bsdf.IsVolume())
				++mergeHitsV2S;
			else
				++mergeHitsS2S;			
		}*/
	}
}

/*void HashGrid::PrintStatistics() const {
	const double mergeTotal = mergeHitsV2V + mergeHitsV2S + mergeHitsS2S;

	if (mergeTotal == 0.f)
		cout << "Volume2Volume = 0  Volume2Surface = 0  Volume2Volume = 0\n";
	else
		cout << boost::format("Volume2Volume = %d (%.2f%%)  Volume2Surface = %d (%.2f%%)  Surface2Surface = %d (%.2f%%)") %
				mergeHitsV2V % (100.0 * mergeHitsV2V / mergeTotal) %
				mergeHitsV2S % (100.0 * mergeHitsV2S / mergeTotal) %
				mergeHitsS2S % (100.0 * mergeHitsS2S / mergeTotal) << "\n";
}*/
