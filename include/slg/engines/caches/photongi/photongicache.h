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

#ifndef _SLG_PHOTONGICACHE_H
#define	_SLG_PHOTONGICACHE_H

#include <boost/atomic.hpp>
#include <boost/thread.hpp>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/utils.h"

#include "slg/slg.h"
#include "slg/samplers/metropolis.h"

namespace slg {

//------------------------------------------------------------------------------
// Photon Mapping Based GI cache
//------------------------------------------------------------------------------

class PhotonGICache;

class TracePhotonsThread {
public:
	TracePhotonsThread(PhotonGICache &pgic, const u_int index);
	virtual ~TracePhotonsThread();

	void Start();
	void Join();

	friend class PhotonGICache;

private:
	void RenderFunc();

	PhotonGICache &pgic;
	const u_int threadIndex;

	boost::thread *renderThread;
};

class PhotonGICache {
public:
	PhotonGICache(const Scene *scn, const u_int photonCount, const u_int maxPathDepth);
	virtual ~PhotonGICache();

	void Preprocess();
	
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	static PhotonGICache *FromProperties(const Scene *scn, const luxrays::Properties &cfg);

	friend class TracePhotonsThread;

private:
	void TracePhotons();

	const Scene *scene;
	
	const u_int photonCount, maxPathDepth;
	
	boost::atomic<u_int> globalCounter;
	MetropolisSamplerSharedData samplerSharedData;
};

}

#endif	/* _SLG_PHOTONGICACHE_H */
