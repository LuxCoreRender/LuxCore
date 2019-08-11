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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

#include "slg/lights/strategies/distributionlightstrategy.h"
#include "slg/lights/constantinfinitelight.h"
#include "slg/lights/distantlight.h"
#include "slg/lights/infinitelight.h"
#include "slg/lights/laserlight.h"
#include "slg/lights/mappointlight.h"
#include "slg/lights/pointlight.h"
#include "slg/lights/projectionlight.h"
#include "slg/lights/sharpdistantlight.h"
#include "slg/lights/sky2light.h"
#include "slg/lights/spotlight.h"
#include "slg/lights/sunlight.h"
#include "slg/lights/trianglelight.h"
#include "slg/lights/spherelight.h"
#include "slg/lights/mapspherelight.h"
#include "slg/lights/strategies/dlscache.h"
#include "slg/lights/strategies/dlscacheimpl/dlscbvh.h"

using namespace std;
using namespace luxrays;
using namespace slg;


void CompiledScene::AddEnabledLightCode() {
	// Optionally include the code for the specified lights in order to reduce
	// the number of OpenCL kernel compilation that may be required

	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_IL))) {
		usedLightSourceTypes.insert(TYPE_IL);
		hasEnvLights = true;
	}
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_SUN))) {
		usedLightSourceTypes.insert(TYPE_SUN);
		hasEnvLights = true;
	}
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_TRIANGLE)))
		usedLightSourceTypes.insert(TYPE_TRIANGLE);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_POINT)))
		usedLightSourceTypes.insert(TYPE_POINT);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_MAPPOINT)))
		usedLightSourceTypes.insert(TYPE_MAPPOINT);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_SPOT)))
		usedLightSourceTypes.insert(TYPE_SPOT);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_PROJECTION)))
		usedLightSourceTypes.insert(TYPE_PROJECTION);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_IL_CONSTANT))) {
		usedLightSourceTypes.insert(TYPE_IL_CONSTANT);
		hasEnvLights = true;
	}
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_SHARPDISTANT))) {
		usedLightSourceTypes.insert(TYPE_SHARPDISTANT);
	}
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_DISTANT))) {
		usedLightSourceTypes.insert(TYPE_DISTANT);
	}
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_IL_SKY2))) {
		usedLightSourceTypes.insert(TYPE_IL_SKY2);
		hasEnvLights = true;
	}
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_LASER)))
		usedLightSourceTypes.insert(TYPE_LASER);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_SPHERE)))
		usedLightSourceTypes.insert(TYPE_SPHERE);
	if (enabledCode.count(LightSource::LightSourceType2String(TYPE_MAPSPHERE)))
		usedLightSourceTypes.insert(TYPE_MAPSPHERE);
}

void CompiledScene::CompileDLSC(const LightStrategyDLSCache *dlscLightStrategy) {
	if (dlscLightStrategy->UseRTMode()) {
		dlscAllEntries.clear();
		dlscAllEntries.shrink_to_fit();

		dlscDistributionIndexToLightIndex.clear();
		dlscDistributionIndexToLightIndex.shrink_to_fit();

		dlscDistributions.clear();
		dlscDistributions.shrink_to_fit();

		dlscBVHArrayNode.clear();
		dlscBVHArrayNode.shrink_to_fit();

		return;
	}

	dlscRadius2 = dlscLightStrategy->GetEntryRadius() * dlscLightStrategy->GetEntryRadius();
	dlscNormalCosAngle = cosf(Radians(dlscLightStrategy->GetEntryNormalAngle()));

	// Compile all cache entries
	const DLSCBvh *bvh = dlscLightStrategy->GetBVH();
	const std::vector<DLSCacheEntry> *allEntries = bvh->GetAllEntries();
	const u_int entriesCount = allEntries->size();
	
	dlscAllEntries.resize(entriesCount);
	dlscDistributionIndexToLightIndex.clear();
	dlscDistributions.clear();
	for (u_int i = 0; i < entriesCount; ++i) {
		const DLSCacheEntry &entry = (*allEntries)[i];
		slg::ocl::DLSCacheEntry &oclEntry = dlscAllEntries[i];

		ASSIGN_VECTOR(oclEntry.p, entry.p);
		ASSIGN_NORMAL(oclEntry.n, entry.n);
		
		oclEntry.isVolume = entry.isVolume;

		if (entry.IsDirectLightSamplingDisabled()) {
			oclEntry.distributionIndexToLightIndexOffset = NULL_INDEX;
			oclEntry.lightsDistributionOffset = NULL_INDEX;
		} else {
			// Compile the distributionIndexToLightIndex table
			oclEntry.distributionIndexToLightIndexOffset = dlscDistributionIndexToLightIndex.size();
			for (auto index : entry.distributionIndexToLightIndex)
				dlscDistributionIndexToLightIndex.push_back(index);

			// Compile the light Distribution1D
			const u_int size = dlscDistributions.size();
			oclEntry.lightsDistributionOffset = size;

			u_int distributionSize;
			float *dist = CompileDistribution1D(entry.lightsDistribution, &distributionSize);

			const u_int distributionSize4 = distributionSize / sizeof(float);
			dlscDistributions.resize(size + distributionSize4);
			oclEntry.distributionIndexToLightIndexSize = distributionSize4;

			copy(dist, dist + distributionSize4,
					&dlscDistributions[size]);

			delete[] dist;
		}
	}
	
	// Compile the DLSC BVH
	u_int nNodes;
	const slg::ocl::IndexBVHArrayNode *nodes = bvh->GetArrayNodes(&nNodes);
	dlscBVHArrayNode.resize(nNodes);
	copy(&nodes[0], &nodes[0] + nNodes, dlscBVHArrayNode.begin());

	dlscAllEntries.shrink_to_fit();
	dlscDistributionIndexToLightIndex.shrink_to_fit();
	dlscDistributions.shrink_to_fit();
	dlscBVHArrayNode.shrink_to_fit();
}

void CompiledScene::CompileLightStrategy() {
	dlscRadius2 = 0.f;
	dlscNormalCosAngle = 0.f;
			
	//--------------------------------------------------------------------------
	// Compile lightDistribution
	//--------------------------------------------------------------------------

	const LightStrategy *illuminateLightStrategy = scene->lightDefs.GetIlluminateLightStrategy();
	
	// Check if it is an DistributionLightStrategy
	const DistributionLightStrategy *distributionIllumLightStrategy = dynamic_cast<const DistributionLightStrategy *>(illuminateLightStrategy);
	if (distributionIllumLightStrategy) {
		delete[] lightsDistribution;
		lightsDistribution = CompileDistribution1D(distributionIllumLightStrategy->GetLightsDistribution(),
				&lightsDistributionSize);
	} else {
		// Check if it is an LightStrategyDLSCache
		
		const LightStrategyDLSCache *dlscLightStrategy = dynamic_cast<const LightStrategyDLSCache *>(illuminateLightStrategy);
		if (dlscLightStrategy) {
			delete[] lightsDistribution;
			lightsDistribution = CompileDistribution1D(dlscLightStrategy->GetLightsDistribution(),
					&lightsDistributionSize);
			
			CompileDLSC(dlscLightStrategy);
		} else
			throw runtime_error("Unsupported illuminate light strategy in CompiledScene::CompileLights()");
	}

	//--------------------------------------------------------------------------
	// Compile infiniteLightDistribution
	//--------------------------------------------------------------------------

	const LightStrategy *infiniteLightStrategy = scene->lightDefs.GetInfiniteLightStrategy();

	// Check if it is an DistributionLightStrategy
	const DistributionLightStrategy *distributionInfLightStrategy = dynamic_cast<const DistributionLightStrategy *>(infiniteLightStrategy);
	if (distributionInfLightStrategy) {
		delete[] infiniteLightSourcesDistribution;
		infiniteLightSourcesDistribution = CompileDistribution1D(distributionInfLightStrategy->GetLightsDistribution(),
				&infiniteLightSourcesDistributionSize);
	} else {
		// Check if it is an LightStrategyDLSCache
		
		const LightStrategyDLSCache *dlscLightStrategy = dynamic_cast<const LightStrategyDLSCache *>(illuminateLightStrategy);
		if (dlscLightStrategy) {
			delete[] infiniteLightSourcesDistribution;
			infiniteLightSourcesDistribution = CompileDistribution1D(dlscLightStrategy->GetLightsDistribution(),
					&infiniteLightSourcesDistributionSize);
		} else
			throw runtime_error("Unsupported infinite light strategy in CompiledScene::CompileLights()");
	}
}

void CompiledScene::CompileELVC(const EnvLightVisibilityCache *visibilityMapCache) {
	if (!visibilityMapCache) {
		elvcAllEntries.clear();
		elvcAllEntries.shrink_to_fit();

		elvcDistributions.clear();
		elvcDistributions.shrink_to_fit();

		elvcBVHArrayNode.clear();
		elvcBVHArrayNode.shrink_to_fit();

		return;
	}

	elvcRadius2 = visibilityMapCache->GetParams().visibility.lookUpRadius *
			visibilityMapCache->GetParams().visibility.lookUpRadius;
	elvcNormalCosAngle = cosf(Radians(visibilityMapCache->GetParams().visibility.lookUpNormalAngle));

	// Compile all cache entries
	const ELVCBvh *bvh = visibilityMapCache->GetBVH();
	const std::vector<ELVCacheEntry> *allEntries = bvh->GetAllEntries();
	const u_int entriesCount = allEntries->size();
	
	elvcAllEntries.resize(entriesCount);
	elvcDistributions.clear();
	for (u_int i = 0; i < entriesCount; ++i) {
		const ELVCacheEntry &entry = (*allEntries)[i];
		slg::ocl::ELVCacheEntry &oclEntry = elvcAllEntries[i];

		ASSIGN_VECTOR(oclEntry.p, entry.p);
		ASSIGN_NORMAL(oclEntry.n, entry.n);
		
		oclEntry.isVolume = entry.isVolume;

		if (!entry.visibilityMap)
			oclEntry.distributionOffset = NULL_INDEX;
		else {
			// Compile the light Distribution2D
			const u_int size = elvcDistributions.size();
			oclEntry.distributionOffset = size;

			u_int distributionSize;
			float *dist = CompileDistribution2D(entry.visibilityMap, &distributionSize);

			const u_int distributionSize4 = distributionSize / sizeof(float);
			elvcDistributions.resize(size + distributionSize4);

			copy(dist, dist + distributionSize4,
					&elvcDistributions[size]);

			delete[] dist;
		}
	}
	
	// Compile the ELVC BVH
	u_int nNodes;
	const slg::ocl::IndexBVHArrayNode *nodes = bvh->GetArrayNodes(&nNodes);
	elvcBVHArrayNode.resize(nNodes);
	copy(&nodes[0], &nodes[0] + nNodes, elvcBVHArrayNode.begin());
	
	elvcAllEntries.shrink_to_fit();
	elvcDistributions.shrink_to_fit();
	elvcBVHArrayNode.shrink_to_fit();
}

void CompiledScene::CompileLights() {
	SLG_LOG("Compile Lights");
	wasLightsCompiled = true;

	//--------------------------------------------------------------------------
	// Translate lights
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();
	
	usedLightSourceTypes.clear();
	hasEnvLights = false;
	hasTriangleLightWithVertexColors = false;
	AddEnabledLightCode();

	const vector<LightSource *> &lightSources = scene->lightDefs.GetLightSources();
	const u_int lightCount = lightSources.size();
	lightDefs.resize(lightCount);
	envLightIndices.clear();
	envLightDistributions.clear();
	
	CompileELVC(nullptr);

	for (u_int i = 0; i < lightSources.size(); ++i) {
		const LightSource *l = lightSources[i];
		usedLightSourceTypes.insert(l->GetType());

		slg::ocl::LightSource *oclLight = &lightDefs[i];
		oclLight->lightSceneIndex = l->lightSceneIndex;
		oclLight->lightID = l->GetID();
		oclLight->visibility =
				(l->IsVisibleIndirectDiffuse() ? DIFFUSE : NONE) |
				(l->IsVisibleIndirectGlossy() ? GLOSSY : NONE) |
				(l->IsVisibleIndirectSpecular() ? SPECULAR : NONE);
		oclLight->isDirectLightSamplingEnabled = l->IsDirectLightSamplingEnabled();

		hasEnvLights |= l->IsEnvironmental();

		switch (l->GetType()) {
			case TYPE_TRIANGLE: {
				const TriangleLight *tl = (const TriangleLight *)l;

				const ExtMesh *mesh = tl->mesh;
				const Triangle *tri = &(mesh->GetTriangles()[tl->triangleIndex]);

				// Check if I have a triangle light source with vertex colors
				if (mesh->HasColors())
					hasTriangleLightWithVertexColors = true;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_TRIANGLE;

				// TriangleLight data
				ASSIGN_VECTOR(oclLight->triangle.v0, mesh->GetVertex(0.f, tri->v[0]));
				ASSIGN_VECTOR(oclLight->triangle.v1, mesh->GetVertex(0.f, tri->v[1]));
				ASSIGN_VECTOR(oclLight->triangle.v2, mesh->GetVertex(0.f, tri->v[2]));
				const Normal geometryN = mesh->GetGeometryNormal(0.f, tl->triangleIndex);
				ASSIGN_VECTOR(oclLight->triangle.geometryN, geometryN);
				if (mesh->HasNormals()) {
					ASSIGN_VECTOR(oclLight->triangle.n0, mesh->GetShadeNormal(0.f, tl->triangleIndex, 0));
					ASSIGN_VECTOR(oclLight->triangle.n1, mesh->GetShadeNormal(0.f, tl->triangleIndex, 1));
					ASSIGN_VECTOR(oclLight->triangle.n2, mesh->GetShadeNormal(0.f, tl->triangleIndex, 2));
				} else {
					ASSIGN_VECTOR(oclLight->triangle.n0, geometryN);
					ASSIGN_VECTOR(oclLight->triangle.n1, geometryN);
					ASSIGN_VECTOR(oclLight->triangle.n2, geometryN);
				}
				if (mesh->HasUVs()) {
					ASSIGN_UV(oclLight->triangle.uv0, mesh->GetUV(tri->v[0]));
					ASSIGN_UV(oclLight->triangle.uv1, mesh->GetUV(tri->v[1]));
					ASSIGN_UV(oclLight->triangle.uv2, mesh->GetUV(tri->v[2]));
				} else {
					const UV zero;
					ASSIGN_UV(oclLight->triangle.uv0, zero);
					ASSIGN_UV(oclLight->triangle.uv1, zero);
					ASSIGN_UV(oclLight->triangle.uv2, zero);
				}
				if (mesh->HasColors()) {
					ASSIGN_SPECTRUM(oclLight->triangle.rgb0, mesh->GetColor(tri->v[0]));
					ASSIGN_SPECTRUM(oclLight->triangle.rgb1, mesh->GetColor(tri->v[1]));
					ASSIGN_SPECTRUM(oclLight->triangle.rgb2, mesh->GetColor(tri->v[2]));					
				} else {
					const Spectrum one(1.f);
					ASSIGN_SPECTRUM(oclLight->triangle.rgb0, one);
					ASSIGN_SPECTRUM(oclLight->triangle.rgb1, one);
					ASSIGN_SPECTRUM(oclLight->triangle.rgb2, one);
				}
				if (mesh->HasAlphas()) {
					oclLight->triangle.alpha0 = mesh->GetAlpha(tri->v[0]);
					oclLight->triangle.alpha1 = mesh->GetAlpha(tri->v[1]);
					oclLight->triangle.alpha2 = mesh->GetAlpha(tri->v[2]);
				} else {
					oclLight->triangle.alpha0 = 1.f;
					oclLight->triangle.alpha1 = 1.f;
					oclLight->triangle.alpha2 = 1.f;
				}

				const float triangleArea = tl->GetTriangleArea();
				oclLight->triangle.invTriangleArea = (triangleArea == 0.f) ? 0.f : (1.f / triangleArea);
				const float meshArea = tl->GetMeshArea();
				oclLight->triangle.invMeshArea = (meshArea == 0.f) ? 0.f : (1.f / meshArea);

				oclLight->triangle.materialIndex = scene->matDefs.GetMaterialIndex(tl->lightMaterial);
				oclLight->triangle.objectID = tl->objectID;

				const SampleableSphericalFunction *emissionFunc = tl->lightMaterial->GetEmissionFunc();
				if (emissionFunc) {
					oclLight->triangle.avarage = emissionFunc->Average();
					oclLight->triangle.imageMapIndex = scene->imgMapCache.GetImageMapIndex(
							// I use only ImageMapSphericalFunction
							((const ImageMapSphericalFunction *)(emissionFunc->GetFunc()))->GetImageMap());
				} else {
					oclLight->triangle.avarage = 0.f;
					oclLight->triangle.imageMapIndex = NULL_INDEX;
				}
				break;
			}
			case TYPE_IL: {
				const InfiniteLight *il = (const InfiniteLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &il->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &il->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, il->gain);

				// InfiniteLight data
				oclLight->notIntersectable.infinite.imageMapIndex = scene->imgMapCache.GetImageMapIndex(il->imageMap);

				// Compile the image map Distribution2D
				const Distribution2D *dist;
				const EnvLightVisibilityCache *visibilityMapCache;
				il->GetPreprocessedData(&dist, &visibilityMapCache);

				if (il->useVisibilityMapCache && visibilityMapCache) {
					if (elvcAllEntries.size() > 0) {
						SLG_LOG("WARNING: OpenCL rendering supports only one EnvLightVisibilityCache");
					} else {
						CompileELVC(visibilityMapCache);

						oclLight->notIntersectable.infinite.useVisibilityMapCache = true;
					}
				}

				u_int distributionSize;
				const float *infiniteLightDistribution = CompileDistribution2D(dist,
						&distributionSize);
				// distributionSize is expressed in bytes while I'm working with float
				const u_int distributionSize4 = distributionSize / sizeof(float);

				// Copy the Distribution2D data in the right place
				const u_int size = envLightDistributions.size();
				envLightDistributions.resize(size + distributionSize4);
				copy(infiniteLightDistribution, infiniteLightDistribution + distributionSize4,
						&envLightDistributions[size]);
				delete[] infiniteLightDistribution;
				oclLight->notIntersectable.infinite.distributionOffset = size;
				break;
			}
			case TYPE_IL_SKY2: {
				const SkyLight2 *sl = (const SkyLight2 *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL_SKY2;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, sl->gain);

				// SkyLight2 data
				const Distribution2D *dist;
				const EnvLightVisibilityCache *visibilityMapCache;
				sl->GetPreprocessedData(
						&oclLight->notIntersectable.sky2.absoluteSunDir.x,
						&oclLight->notIntersectable.sky2.absoluteUpDir.x,
						oclLight->notIntersectable.sky2.scaledGroundColor.c,
						&oclLight->notIntersectable.sky2.isGroundBlack,
						oclLight->notIntersectable.sky2.aTerm.c,
						oclLight->notIntersectable.sky2.bTerm.c,
						oclLight->notIntersectable.sky2.cTerm.c,
						oclLight->notIntersectable.sky2.dTerm.c,
						oclLight->notIntersectable.sky2.eTerm.c,
						oclLight->notIntersectable.sky2.fTerm.c,
						oclLight->notIntersectable.sky2.gTerm.c,
						oclLight->notIntersectable.sky2.hTerm.c,
						oclLight->notIntersectable.sky2.iTerm.c,
						oclLight->notIntersectable.sky2.radianceTerm.c,
						&dist,
						&visibilityMapCache);

				if (sl->useVisibilityMapCache && visibilityMapCache) {
					if (elvcAllEntries.size() > 0) {
						SLG_LOG("WARNING: OpenCL rendering supports only one EnvLightVisibilityCache");
					} else {
						CompileELVC(visibilityMapCache);

						oclLight->notIntersectable.sky2.useVisibilityMapCache = true;
					}
				}

				oclLight->notIntersectable.sky2.hasGround = sl->hasGround;
				
				u_int distributionSize;
				const float *skyDistribution = CompileDistribution2D(dist,
						&distributionSize);
				// distributionSize is expressed in bytes while I'm working with float
				const u_int distributionSize4 = distributionSize / sizeof(float);

				// Copy the Distribution2D data in the right place
				const u_int size = envLightDistributions.size();
				envLightDistributions.resize(size + distributionSize4);
				copy(skyDistribution, skyDistribution + distributionSize4,
						&envLightDistributions[size]);
				delete[] skyDistribution;
				oclLight->notIntersectable.sky2.distributionOffset = size;
				break;
			}
			case TYPE_SUN: {
				const SunLight *sl = (const SunLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SUN;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, sl->gain);

				// SunLight data
				sl->GetPreprocessedData(
						&oclLight->notIntersectable.sun.absoluteDir.x,
						&oclLight->notIntersectable.sun.x.x,
						&oclLight->notIntersectable.sun.y.x,
						NULL, NULL, NULL,
						&oclLight->notIntersectable.sun.cosThetaMax, &oclLight->notIntersectable.sun.sin2ThetaMax);
				ASSIGN_SPECTRUM(oclLight->notIntersectable.sun.color, sl->color);
				oclLight->notIntersectable.sun.turbidity = sl->turbidity;
				oclLight->notIntersectable.sun.relSize= sl->relSize;
				break;
			}
			case TYPE_POINT: {
				const PointLight *pl = (const PointLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_POINT;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &pl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &pl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, pl->gain);

				// PointLight data
				pl->GetPreprocessedData(
						NULL,
						&oclLight->notIntersectable.point.absolutePos.x,
						oclLight->notIntersectable.point.emittedFactor.c);
				break;
			}
			case TYPE_MAPPOINT: {
				const MapPointLight *mpl = (const MapPointLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_MAPPOINT;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &mpl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &mpl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, mpl->gain);

				// MapPointLight data
				const SampleableSphericalFunction *funcData;
				mpl->GetPreprocessedData(
					&oclLight->notIntersectable.mapPoint.localPos.x,
					&oclLight->notIntersectable.mapPoint.absolutePos.x,
					oclLight->notIntersectable.mapPoint.emittedFactor.c,
					&funcData);
				oclLight->notIntersectable.mapPoint.avarage = funcData->Average();
				oclLight->notIntersectable.mapPoint.imageMapIndex = scene->imgMapCache.GetImageMapIndex(mpl->imageMap);
				break;
			}
			case TYPE_SPOT: {
				const SpotLight *sl = (const SpotLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SPOT;

				// NotIntersectableLightSource data
				//memcpy(&oclLight->notIntersectable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				//memcpy(&oclLight->notIntersectable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, sl->gain);

				// SpotLight data
				const Transform *alignedWorld2Light;
				sl->GetPreprocessedData(
					oclLight->notIntersectable.spot.emittedFactor.c,
					&oclLight->notIntersectable.spot.absolutePos.x,
					&oclLight->notIntersectable.spot.cosTotalWidth,
					&oclLight->notIntersectable.spot.cosFalloffStart,
					&alignedWorld2Light);

				memcpy(&oclLight->notIntersectable.light2World.m, &alignedWorld2Light->m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &alignedWorld2Light->mInv, sizeof(float[4][4]));
				break;
			}
			case TYPE_PROJECTION: {
				const ProjectionLight *pl = (const ProjectionLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_PROJECTION;

				// NotIntersectableLightSource data
				//memcpy(&oclLight->notIntersectable.light2World.m, &pl->lightToWorld.m, sizeof(float[4][4]));
				//memcpy(&oclLight->notIntersectable.light2World.mInv, &pl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, pl->gain);

				// ProjectionLight data
				oclLight->notIntersectable.projection.imageMapIndex = (pl->imageMap) ?
					scene->imgMapCache.GetImageMapIndex(pl->imageMap) :
					NULL_INDEX;

				const Transform *alignedWorld2Light, *lightProjection;
				pl->GetPreprocessedData(
					oclLight->notIntersectable.projection.emittedFactor.c,
					&(oclLight->notIntersectable.projection.absolutePos.x),
					&(oclLight->notIntersectable.projection.lightNormal.x),
					&oclLight->notIntersectable.projection.screenX0,
					&oclLight->notIntersectable.projection.screenX1,
					&oclLight->notIntersectable.projection.screenY0,
					&oclLight->notIntersectable.projection.screenY1,
					&alignedWorld2Light, &lightProjection);

				memcpy(&oclLight->notIntersectable.light2World.m, &alignedWorld2Light->m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &alignedWorld2Light->mInv, sizeof(float[4][4]));

				memcpy(&oclLight->notIntersectable.projection.lightProjection.m, &lightProjection->m, sizeof(float[4][4]));
				break;
			}
			case TYPE_IL_CONSTANT: {
				const ConstantInfiniteLight *cil = (const ConstantInfiniteLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL_CONSTANT;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &cil->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &cil->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, cil->gain);

				// ConstantInfiniteLight data
				ASSIGN_SPECTRUM(oclLight->notIntersectable.constantInfinite.color, cil->color);

				// Compile the visibility map Distribution2D
				const EnvLightVisibilityCache *visibilityMapCache;
				cil->GetPreprocessedData(&visibilityMapCache);

				if (cil->useVisibilityMapCache && visibilityMapCache) {
					if (elvcAllEntries.size() > 0) {
						SLG_LOG("WARNING: OpenCL rendering supports only one EnvLightVisibilityCache");
					} else {
						CompileELVC(visibilityMapCache);

						oclLight->notIntersectable.constantInfinite.useVisibilityMapCache = true;
					}
				}
				break;
			}
		case TYPE_SHARPDISTANT: {
				const SharpDistantLight *sdl = (const SharpDistantLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SHARPDISTANT;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &sdl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &sdl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, sdl->gain);

				// SharpDistantLight data
				ASSIGN_SPECTRUM(oclLight->notIntersectable.sharpDistant.color, sdl->color);
				sdl->GetPreprocessedData(&(oclLight->notIntersectable.sharpDistant.absoluteLightDir.x), NULL, NULL);
				break;
			}
			case TYPE_DISTANT: {
				const DistantLight *dl = (const DistantLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_DISTANT;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &dl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &dl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, dl->gain);

				// DistantLight data
				ASSIGN_SPECTRUM(oclLight->notIntersectable.sharpDistant.color, dl->color);
				dl->GetPreprocessedData(
					&(oclLight->notIntersectable.distant.absoluteLightDir.x),
					&(oclLight->notIntersectable.distant.x.x),
					&(oclLight->notIntersectable.distant.y.x),
					NULL,
					&(oclLight->notIntersectable.distant.cosThetaMax));
				break;
			}
			case TYPE_LASER: {
				const LaserLight *ll = (const LaserLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_LASER;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &ll->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &ll->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, ll->gain);

				// LaserLight data
				ll->GetPreprocessedData(
					oclLight->notIntersectable.laser.emittedFactor.c,
					&oclLight->notIntersectable.laser.absoluteLightPos.x,
					&oclLight->notIntersectable.laser.absoluteLightDir.x,
					NULL, NULL);
				oclLight->notIntersectable.laser.radius = ll->radius;
				break;
			}
			case TYPE_SPHERE: {
				const SphereLight *sl = (const SphereLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SPHERE;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, sl->gain);

				// SphereLight data
				sl->GetPreprocessedData(
						NULL,
						&oclLight->notIntersectable.sphere.absolutePos.x,
						oclLight->notIntersectable.sphere.emittedFactor.c);
				
				oclLight->notIntersectable.sphere.radius = sl->radius;
				break;
			}
			case TYPE_MAPSPHERE: {
				const MapSphereLight *msl = (const MapSphereLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_MAPSPHERE;

				// NotIntersectableLightSource data
				memcpy(&oclLight->notIntersectable.light2World.m, &msl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersectable.light2World.mInv, &msl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersectable.gain, msl->gain);

				// MapSphereLight data
				const SampleableSphericalFunction *funcData;
				msl->GetPreprocessedData(
					NULL,
					&oclLight->notIntersectable.mapSphere.sphere.absolutePos.x,
					oclLight->notIntersectable.mapSphere.sphere.emittedFactor.c,
					&funcData);
				oclLight->notIntersectable.mapSphere.sphere.radius = msl->radius;

				oclLight->notIntersectable.mapSphere.avarage = funcData->Average();
				oclLight->notIntersectable.mapSphere.imageMapIndex = scene->imgMapCache.GetImageMapIndex(msl->imageMap);
				break;
			}
			default:
				throw runtime_error("Unknown Light source type in CompiledScene::CompileLights()");
		}

		if (l->IsEnvironmental())
			envLightIndices.push_back(i);
	}

	lightIndexOffsetByMeshIndex = scene->lightDefs.GetLightIndexOffsetByMeshIndex();
	lightIndexByTriIndex = scene->lightDefs.GetLightIndexByTriIndex();

	CompileLightStrategy();

	const double tEnd = WallClockTime();
	SLG_LOG("Lights compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
