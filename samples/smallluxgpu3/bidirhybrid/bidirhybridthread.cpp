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
#include "renderconfig.h"
#include "bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/utils.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

//------------------------------------------------------------------------------
// BiDirHybridRenderThread
//------------------------------------------------------------------------------

BiDirHybridRenderThread::BiDirHybridRenderThread(const unsigned int index,
		const unsigned int seedBase, IntersectionDevice *device,
		BiDirHybridRenderEngine *re) {
	intersectionDevice = device;
	seed = seedBase;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;

	samplesCount = 0.0;

	started = false;
	editMode = false;
}

BiDirHybridRenderThread::~BiDirHybridRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();
}

void BiDirHybridRenderThread::Start() {
	started = true;
	StartRenderThread();
}

void BiDirHybridRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void BiDirHybridRenderThread::Stop() {
	StopRenderThread();
	started = false;
}

void BiDirHybridRenderThread::StartRenderThread() {
	samplesCount = 0.0;

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(BiDirHybridRenderThread::RenderThreadImpl, this));
}

void BiDirHybridRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void BiDirHybridRenderThread::BeginEdit() {
	StopRenderThread();
}

void BiDirHybridRenderThread::EndEdit(const EditActionList &editActions) {
	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

void BiDirHybridRenderThread::RenderThreadImpl(BiDirHybridRenderThread *renderThread) {
	SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	try {
		while (!boost::this_thread::interruption_requested()) {
		}

		SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}
}
