/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_PATHOCLBASENATIVETHREAD_H
#define	_SLG_PATHOCLBASENATIVETHREAD_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/thread/thread.hpp>

#include "luxrays/devices/nativeintersectiondevice.h"
#include "slg/slg.h"

namespace slg {

class PathOCLBaseRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing CPU render threads for hybrid rendering with PathOCLBase
//------------------------------------------------------------------------------

class PathOCLBaseNativeRenderThread {
public:
	PathOCLBaseNativeRenderThread(const u_int index, luxrays::NativeIntersectionDevice *device,
			PathOCLBaseRenderEngine *re);
	virtual ~PathOCLBaseNativeRenderThread();

	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

	friend class PathOCLBaseRenderEngine;

protected:
	virtual void StartRenderThread();
	virtual void StopRenderThread();

	// Implementation specific methods
	virtual void RenderThreadImpl() = 0;

	u_int threadIndex;
	PathOCLBaseRenderEngine *renderEngine;
	luxrays::NativeIntersectionDevice *intersectionDevice;

	boost::thread *renderThread;

	bool started, editMode, threadDone;
};

}

#endif

#endif	/* _SLG_PATHOCLBASENATIVETHREAD_H */
