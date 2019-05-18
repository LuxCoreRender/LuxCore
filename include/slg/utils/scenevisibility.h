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

#ifndef _SLG_SCENEVISIBILITY_H
#define	_SLG_SCENEVISIBILITY_H

#include <vector>

#include "slg/slg.h"
#include "slg/core/indexoctree.h"

namespace slg {

template <class T>
class SceneVisibility {
public:
	SceneVisibility(const Scene *scene, std::vector<T> &visibilityParticles,
			const u_int maxPathDepth, const u_int maxSampleCount,
			const float targetHitRate,
			const float lookUpRadius, const float lookUpNormalAngle,
			const float timeStart, const float timeEnd);
	virtual ~SceneVisibility();

	void Build();

protected:
	class TraceVisibilityThread {
	public:
		TraceVisibilityThread(SceneVisibility<T> &sv, const u_int index,
				SobolSamplerSharedData &visibilitySobolSharedData,
				IndexOctree<T> *particlesOctree, boost::mutex &particlesOctreeMutex,
				boost::atomic<u_int> &globalVisibilityParticlesCount,
				u_int &visibilityCacheLookUp, u_int &visibilityCacheHits,
				bool &visibilityWarmUp);
		virtual ~TraceVisibilityThread();

		void Start();
		void Join();

	private:
		void GenerateEyeRay(const Camera *camera, luxrays::Ray &eyeRay,
				PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const;

		void RenderFunc();

		SceneVisibility<T> &sv;
		const u_int threadIndex;

		SobolSamplerSharedData &visibilitySobolSharedData;
		IndexOctree<T> *particlesOctree;
		boost::mutex &particlesOctreeMutex;
		boost::atomic<u_int> &globalVisibilityParticlesCount;
		u_int &visibilityCacheLookUp;
		u_int &visibilityCacheHits;
		bool &visibilityWarmUp;

		boost::thread *renderThread;
	};

	virtual IndexOctree<T> *AllocOctree() const = 0;
	virtual bool ProcessHitPoint(const BSDF &bsdf, std::vector<T> &visibilityParticles) const = 0;
	virtual bool ProcessVisibilityParticle(const T &visibilityParticle, std::vector<T> &visibilityParticles,
			IndexOctree<T> *particlesOctree, const float maxDistance2) const = 0;

	const Scene *scene;
	std::vector<T> &visibilityParticles;
	const u_int maxPathDepth, maxSampleCount;	
	const float targetHitRate, lookUpRadius, lookUpNormalAngle, timeStart, timeEnd;
};

}

#endif	/* _SLG_SCENEVISIBILITY_H */
