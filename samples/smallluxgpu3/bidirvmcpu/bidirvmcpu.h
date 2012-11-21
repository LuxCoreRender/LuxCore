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

#ifndef _BIDIRVMCPU_H
#define	_BIDIRVMCPU_H

#include "smalllux.h"

#include "bidircpu/bidircpu.h"

//------------------------------------------------------------------------------
// Bidirectional path tracing with Vertex Merging CPU render engine
//------------------------------------------------------------------------------

class BiDirVMCPURenderEngine;
class BiDirVMCPURenderThread;

class HashGrid {
public:
	HashGrid() { }
	~HashGrid() { }

	void Build(vector<vector<PathVertexVM> > &pathsVertices, const float radius);

	void Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const;

private:
	void Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, const int i0, const int i1,
		Spectrum *radiance) const;
	void Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, const PathVertexVM *lightVertex,
		Spectrum *radiance) const;

	void HashRange(const u_int i, int *i0, int *i1) const {
		if (i == 0) {
			*i0 = 0;
			*i1 = cellEnds[0];
		} else {
			*i0 = cellEnds[i - 1];
			*i1 = cellEnds[i];
		}
	}

	u_int Hash(const int ix, const int iy, const int iz) const {
		return (u_int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % gridSize;
	}

	u_int Hash(const Point &p) const {
		const Vector distMin = p - vertexBBox.pMin;

		return Hash(
				Float2Int(invCellSize * distMin.x),
				Float2Int(invCellSize * distMin.y),
				Float2Int(invCellSize * distMin.z));
    }

	float radius2;
	u_int gridSize;
	float invCellSize;
	BBox vertexBBox;
	u_int vertexCount;

	vector<const PathVertexVM *> lightVertices;
    vector<int> cellEnds;
};

class BiDirVMCPURenderThread : public BiDirCPURenderThread {
public:
	BiDirVMCPURenderThread(BiDirVMCPURenderEngine *engine, const u_int index,
			IntersectionDevice *device, const u_int seedVal);

	friend class HashGrid;
	friend class BiDirVMCPURenderEngine;

private:
	boost::thread *AllocRenderThread() { return new boost::thread(&BiDirVMCPURenderThread::RenderFuncVM, this); }

	void RenderFuncVM();
};

class BiDirVMCPURenderEngine : public BiDirCPURenderEngine {
public:
	BiDirVMCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRVMCPU; }

	friend class BiDirVMCPURenderThread;

private:
	CPURenderThread *NewRenderThread(const u_int index, IntersectionDevice *device,
			const u_int seedVal) {
		return new BiDirVMCPURenderThread(this, index, device, seedVal);
	}
};

#endif	/* _BIDIRVMCPU_H */
