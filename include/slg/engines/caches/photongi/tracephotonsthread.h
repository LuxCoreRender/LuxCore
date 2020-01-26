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

#ifndef _SLG_TRACEPHOTONSTHREAD_H
#define	_SLG_TRACEPHOTONSTHREAD_H

#include <vector>
#include <boost/thread.hpp>

#include "luxrays/utils/utils.h"

#include "slg/slg.h"

namespace slg {

//------------------------------------------------------------------------------
// TracePhotonsThread
//------------------------------------------------------------------------------

class Photon;
class RadiancePhoton;
class PhotonGICache;

class TracePhotonsThread {
public:
	struct RadiancePhotonEntry {
		RadiancePhotonEntry(const u_int visPartIndex,
			const luxrays::Spectrum &rad) : visibilityParticelIndex(visPartIndex), alpha(rad) {
		}

		u_int visibilityParticelIndex;
		luxrays::Spectrum alpha;
	};

	TracePhotonsThread(PhotonGICache &pgic, const u_int index,
		const u_int seedBase, const u_int photonTracedCount,
		const bool indirectCacheDone, const bool causticCacheDone,
		boost::atomic<u_int> &gPhotonsCounter, boost::atomic<u_int> &gIndirectPhotonsTraced,
		boost::atomic<u_int> &gCausticPhotonsTraced, boost::atomic<u_int> &gIndirectSize,
		boost::atomic<u_int> &gCausticSize);
	virtual ~TracePhotonsThread();

	void Start();
	void Join();

	std::vector<RadiancePhotonEntry> indirectPhotons;
	std::vector<Photon> causticPhotons;

	friend class PhotonGICache;

private:
	void UniformMutate(luxrays::RandomGenerator &rndGen, std::vector<float> &samples) const;
	void Mutate(luxrays::RandomGenerator &rndGen, const std::vector<float> &currentPathSamples,
			std::vector<float> &candidatePathSamples, const float mutationSize) const;
	bool TracePhotonPath(luxrays::RandomGenerator &rndGen,
			const std::vector<float> &samples,
			std::vector<RadiancePhotonEntry> &newIndirectPhotons,
			std::vector<Photon> &newCausticPhotons);
	void AddPhotons(const std::vector<RadiancePhotonEntry> &newIndirectPhotons,
			const std::vector<Photon> &newCausticPhotons);
	void AddPhotons(const float currentPhotonsScale,
			const std::vector<RadiancePhotonEntry> &newIndirectPhotons,
			const std::vector<Photon> &newCausticPhotons);

	void RenderFunc();

	PhotonGICache &pgic;
	const u_int threadIndex, seedBase, photonTracedCount;

	boost::atomic<u_int> &globalPhotonsCounter;
	boost::atomic<u_int> &globalIndirectPhotonsTraced;
	boost::atomic<u_int> &globalCausticPhotonsTraced;
	boost::atomic<u_int> &globalIndirectSize;
	boost::atomic<u_int> &globalCausticSize;

	boost::thread *renderThread;

	u_int sampleBootSize, sampleStepSize, sampleSize;
	bool indirectDone, causticDone;
};

}

#endif	/* _SLG_TRACEPHOTONSTHREAD_H */
