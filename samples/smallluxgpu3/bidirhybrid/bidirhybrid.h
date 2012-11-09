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

#ifndef _BIDIRHYBRID_H
#define	_BIDIRHYBRID_H

#include "smalllux.h"
#include "renderengine.h"
#include "bidirhybrid/bidirhybrid.h"

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render threads
//------------------------------------------------------------------------------

class BiDirHybridRenderEngine;

class BiDirHybridRenderThread {
public:
	BiDirHybridRenderThread(const unsigned int index, const unsigned int seedBase,
			IntersectionDevice *device, BiDirHybridRenderEngine *re);
	~BiDirHybridRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	friend class BiDirHybridRenderEngine;

private:
	static void RenderThreadImpl(BiDirHybridRenderThread *renderThread);

	void StartRenderThread();
	void StopRenderThread();

	boost::thread *renderThread;

	IntersectionDevice *intersectionDevice;

	unsigned int threadIndex, seed;
	BiDirHybridRenderEngine *renderEngine;
	double samplesCount;

	bool started, editMode;
};

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render engine
//------------------------------------------------------------------------------

class BiDirHybridRenderEngine : public OCLRenderEngine {
public:
	BiDirHybridRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~BiDirHybridRenderEngine();

	RenderEngineType GetEngineType() const { return BIDIRHYBRID; }

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirHybridRenderThread;

private:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	void UpdateFilmLockLess();

	void UpdateSamplesCount() {
		double count = 0.0;
		for (size_t i = 0; i < renderThreads.size(); ++i)
			count += renderThreads[i]->samplesCount;
		samplesCount = count;
	}

	vector<IntersectionDevice *> devices;
	vector<BiDirHybridRenderThread *> renderThreads;
};

#endif	/* _BIDIRHYBRID_H */
