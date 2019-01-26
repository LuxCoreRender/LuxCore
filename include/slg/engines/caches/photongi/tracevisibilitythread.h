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

#ifndef _SLG_TRACEVISIBILITYTHREAD_H
#define	_SLG_TRACEVISIBILITYTHREAD_H

#include <vector>
#include <boost/thread.hpp>

#include "luxrays/utils/utils.h"

#include "slg/slg.h"

namespace slg {

//------------------------------------------------------------------------------
// TraceVisibilityThread
//------------------------------------------------------------------------------

class PhotonGICache;

class TraceVisibilityThread {
public:
	TraceVisibilityThread(PhotonGICache &pgic, const u_int index);
	virtual ~TraceVisibilityThread();

	void Start();
	void Join();

private:
	void GenerateEyeRay(const Camera *camera, luxrays::Ray &eyeRay,
			PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const;

	void RenderFunc();

	PhotonGICache &pgic;
	const u_int threadIndex;

	boost::thread *renderThread;
};

}

#endif	/* _SLG_TRACEVISIBILITYTHREAD_H */
