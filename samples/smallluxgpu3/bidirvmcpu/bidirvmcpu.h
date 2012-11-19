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

class BiDirVMCPURenderThread : public BiDirCPURenderThread {
public:
	BiDirVMCPURenderThread(BiDirVMCPURenderEngine *engine, const u_int index,
			IntersectionDevice *device, const u_int seedVal);

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
