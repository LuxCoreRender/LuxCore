/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _SLG_BIDIRVMCPU_H
#define	_SLG_BIDIRVMCPU_H

#include "slg/slg.h"
#include "slg/engines/bidircpu/bidircpu.h"

namespace slg {

//------------------------------------------------------------------------------
// Bidirectional path tracing with Vertex Merging CPU render engine
//------------------------------------------------------------------------------

class BiDirVMCPURenderEngine;
class BiDirVMCPURenderThread;

class HashGrid {
public:
	HashGrid() { }
	~HashGrid() { }

	u_int GetVertexCount() const { return vertexCount; }

	void Build(std::vector<std::vector<PathVertexVM> > &pathsVertices, const float radius);

	void Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, luxrays::Spectrum *radiance) const;

	//void PrintStatistics() const;

private:
	void Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, const int i0, const int i1,
		luxrays::Spectrum *radiance) const;
	void Process(const BiDirVMCPURenderThread *thread,
		const PathVertexVM &eyeVertex, const PathVertexVM *lightVertex,
		luxrays::Spectrum *radiance) const;

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

	u_int Hash(const luxrays::Point &p) const {
		const luxrays::Vector distMin = p - vertexBBox.pMin;

		return Hash(
				luxrays::Float2Int(invCellSize * distMin.x),
				luxrays::Float2Int(invCellSize * distMin.y),
				luxrays::Float2Int(invCellSize * distMin.z));
    }

	float radius2;
	u_int gridSize;
	float invCellSize;
	luxrays::BBox vertexBBox;
	u_int vertexCount;

	std::vector<const PathVertexVM *> lightVertices;
    std::vector<int> cellEnds;

	// Statistics
	//mutable u_int mergeHitsV2V; // merge Volume with Volume path vertex
	//mutable u_int mergeHitsV2S; // merge Volume with Surface path vertex
	//mutable u_int mergeHitsS2S; // merge Surface with Surface path vertex
};

class BiDirVMCPURenderThread : public BiDirCPURenderThread {
public:
	BiDirVMCPURenderThread(BiDirVMCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class HashGrid;
	friend class BiDirVMCPURenderEngine;

private:
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&BiDirVMCPURenderThread::RenderFuncVM, this); }

	void RenderFuncVM();
};

class BiDirVMCPURenderEngine : public BiDirCPURenderEngine {
public:
	BiDirVMCPURenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return BIDIRVMCPU; }
	static std::string GetObjectTag() { return "BIDIRVMCPU"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

	friend class BiDirVMCPURenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual void StartLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index, luxrays::IntersectionDevice *device) {
		return new BiDirVMCPURenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_BIDIRVMCPU_H */
