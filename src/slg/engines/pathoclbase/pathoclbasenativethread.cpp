/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "luxrays/core/intersectiondevice.h"
#include "slg/slg.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "luxrays/utils/thread.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseNativeRenderThread
//------------------------------------------------------------------------------

PathOCLBaseNativeRenderThread::PathOCLBaseNativeRenderThread(const u_int index,
		NativeIntersectionDeviceRef device, PathOCLBaseRenderEngine *re) :
	intersectionDevice(device)
{
	threadIndex = index;
	renderEngine = re;

	renderThread = NULL;
	started = false;
	editMode = false;
	threadDone = false;
}

PathOCLBaseNativeRenderThread::~PathOCLBaseNativeRenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();
}

void PathOCLBaseNativeRenderThread::Start() {
	started = true;
	
	StartRenderThread();
}

void PathOCLBaseNativeRenderThread::Interrupt() {
	if (renderThread)
		renderThread->request_stop();
}

void PathOCLBaseNativeRenderThread::Stop() {
	StopRenderThread();

	started = false;

	// Film is deleted in the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLBaseNativeRenderThread::StartRenderThread() {
	threadDone = false;

	// Create the thread for the rendering
	renderThread = std::make_unique<luxrays::JThread>(
		std::bind_front(&PathOCLBaseNativeRenderThread::RenderThreadImpl, this)
	);
	SetThreadName(renderThread, "LxPathOCLNative");
}

void PathOCLBaseNativeRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->request_stop();
		renderThread->join();
	}
}

void PathOCLBaseNativeRenderThread::BeginSceneEdit() {
	StopRenderThread();
}

void PathOCLBaseNativeRenderThread::EndSceneEdit(const EditActionList &editActions) {
	// Reset statistics in order to be more accurate
	intersectionDevice.ResetPerformaceStats();

	StartRenderThread();
}

bool PathOCLBaseNativeRenderThread::HasDone() const {
	return (renderThread == NULL) || threadDone;
}

void PathOCLBaseNativeRenderThread::WaitForDone() const {
	if (renderThread)
		renderThread->join();
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
