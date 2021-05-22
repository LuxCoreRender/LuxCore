#line 2 "pgic_funcs.cl"

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

OPENCL_FORCE_INLINE bool PhotonGICache_IsPhotonGIEnabled(__global const BSDF *bsdf,
		const float glossinessUsageThreshold
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	if (matIndex == NULL_INDEX)
		return false;
	else {
		const BSDFEvent eventTypes = BSDF_GetEventTypes(bsdf
				MATERIALS_PARAM);

		if ((eventTypes & TRANSMIT) || (eventTypes & SPECULAR) ||
				((eventTypes & GLOSSY) && (BSDF_GetGlossiness(bsdf MATERIALS_PARAM) < glossinessUsageThreshold)))
			return false;
		else
			return BSDF_IsPhotonGIEnabled(bsdf MATERIALS_PARAM);
	}
}

OPENCL_FORCE_INLINE bool PhotonGICache_IsDirectLightHitVisible(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global const EyePathInfo *pathInfo,
		const bool photonGICausticCacheUsed) {
	// This is a specific check to cut fireflies created by some glossy or
	// specular bounce
	if (!(pathInfo->lastBSDFEvent & DIFFUSE) && (pathInfo->depth.diffuseDepth > 0))
		return false;
	else if (!taskConfig->pathTracer.pgic.causticEnabled || !photonGICausticCacheUsed)
		return true;
	else if (!pathInfo->isNearlyCaustic && (taskConfig->pathTracer.pgic.debugType == PGIC_DEBUG_NONE))
		return true;
	else
		return false;
}

//------------------------------------------------------------------------------
// Indirect cache related functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float PhotonGICache_GetIndirectUsageThreshold(
		const BSDFEvent lastBSDFEvent, const float lastGlossiness,
		const float u0,
		const float pgicGlossinessUsageThreshold,
		const float pgicIndirectUsageThresholdScale,
		const float pgicIndirectLookUpRadius) {
	// Decide if the glossy surface is "nearly specular"

	if ((lastBSDFEvent & GLOSSY) && (lastGlossiness < pgicGlossinessUsageThreshold)) {
		// Disable the cache, the surface is "nearly specular"
		return INFINITY;
	} else {
		// Use a larger blend zone for glossy surface
		const float scale = (lastBSDFEvent & GLOSSY) ? 2.f : 1.f;

		// Enable the cache for diffuse or glossy "nearly diffuse" but only after
		// the threshold (before I brute force and cache between 0x and 1x the threshold)
		return scale * u0 * pgicIndirectUsageThresholdScale * pgicIndirectLookUpRadius;
	}
}

OPENCL_FORCE_INLINE __global const RadiancePhoton* restrict RadiancePhotonsBVH_GetNearestEntry(
		__global const RadiancePhoton* restrict pgicRadiancePhotons,
		__global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes,
		const float pgicIndirectLookUpRadius2, const float pgicIndirectLookUpNormalCosAngle,
		const float3 p, const float3 n, const bool isVolume
		) {
	__global const RadiancePhoton* restrict nearestEntry = NULL;

	float nearestDistance2 = pgicIndirectLookUpRadius2;

	uint currentNode = 0; // Root Node
	const uint stopNode = IndexBVHNodeData_GetSkipIndex(pgicRadiancePhotonsBVHNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		__global const IndexBVHArrayNode* restrict node = &pgicRadiancePhotonsBVHNodes[currentNode];

		const uint nodeData = node->nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			__global const RadiancePhoton* restrict entry = &pgicRadiancePhotons[node->entryLeaf.entryIndex];

			const float distance2 = DistanceSquared(p, VLOAD3F(&entry->p.x));
			if ((distance2 < nearestDistance2) && 
					(entry->isVolume == isVolume) &&
						(isVolume ||
						(dot(n, VLOAD3F(&entry->n.x)) > pgicIndirectLookUpNormalCosAngle)
						)
				) {
				// I have found a valid nearer entry
				nearestEntry = entry;
				nearestDistance2 = distance2;
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node->bvhNode.bboxMin[0] && p.x <= node->bvhNode.bboxMax[0] &&
					p.y >= node->bvhNode.bboxMin[1] && p.y <= node->bvhNode.bboxMax[1] &&
					p.z >= node->bvhNode.bboxMin[2] && p.z <= node->bvhNode.bboxMax[2])
				++currentNode;
			else {
				// I don't need to use IndexBVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return nearestEntry;
}

OPENCL_FORCE_INLINE __global const Spectrum* restrict PhotonGICache_GetIndirectRadiance(__global const BSDF *bsdf,
		__global const RadiancePhoton* restrict pgicRadiancePhotons,
		const uint pgicLightGroupCounts, __global const Spectrum* restrict pgicRadiancePhotonsValues,
		__global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes,
		const float pgicIndirectLookUpRadius2, const float pgicIndirectLookUpNormalCosAngle) {
	if (!pgicRadiancePhotons)
		return NULL;

	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);
	const float3 n = (bsdf->hitPoint.intoObject ? 1.f: -1.f) * VLOAD3F(&bsdf->hitPoint.geometryN.x);

	__global const RadiancePhoton* restrict radiancePhoton = RadiancePhotonsBVH_GetNearestEntry(
			pgicRadiancePhotons, pgicRadiancePhotonsBVHNodes,
			pgicIndirectLookUpRadius2, pgicIndirectLookUpNormalCosAngle,
			p, n, bsdf->isVolume
			);

	if (radiancePhoton)
		return &pgicRadiancePhotonsValues[radiancePhoton->outgoingRadianceIndex];
	else
		return NULL;
}

//------------------------------------------------------------------------------
// Caustic cache related functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 PGICPhotonBvh_ConnectCacheEntry(__global const Photon* restrict photon,
		__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	BSDFEvent event;
	const float3 photonDir = VLOAD3F(&photon->d.x);
	float directPdfW;
	float3 bsdfEval = BSDF_Evaluate(bsdf, -photonDir, &event, &directPdfW MATERIALS_PARAM);
	// bsdf.Evaluate() multiplies the result by AbsDot(bsdf.hitPoint.shadeN, -photon->d)
	// so I have to cancel that factor. It is already included in photon density
	// estimation.
	if (!bsdf->isVolume)
		bsdfEval /= fabs(dot(VLOAD3F(&bsdf->hitPoint.shadeN.x), -photonDir));
	else
		bsdfEval /= directPdfW;

	return VLOAD3F(photon->alpha.c) * bsdfEval;
}

OPENCL_FORCE_INLINE bool PGICPhotonBvh_ConnectAllNearEntries(__global const BSDF *bsdf,
		__global const Photon* restrict pgicCausticPhotons,
		__global const IndexBVHArrayNode* restrict pgicCausticPhotonsBVHNodes,
		const uint pgicCausticPhotonTracedCount,
		const float pgicCausticLookUpRadius, const float pgicCausticLookUpNormalCosAngle,
		const float3 scale,
		__global Spectrum *radiance
		MATERIALS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);
	// Flip the normal if required
	const float3 n = (bsdf->hitPoint.intoObject ? 1.f: -1.f) * VLOAD3F(&bsdf->hitPoint.geometryN.x);
	const bool isVolume = bsdf->isVolume;

	uint currentNode = 0; // Root Node
	const uint stopNode = IndexBVHNodeData_GetSkipIndex(pgicCausticPhotonsBVHNodes[0].nodeData); // Non-existent

	const float pgicCausticLookUpRadius2 = pgicCausticLookUpRadius * pgicCausticLookUpRadius;
	const float factor = isVolume ?
		1.f / (pgicCausticPhotonTracedCount * (4.f / 3.f * M_PI_F * pgicCausticLookUpRadius2 * pgicCausticLookUpRadius)) :
		1.f / (pgicCausticPhotonTracedCount * (M_PI_F * pgicCausticLookUpRadius2));

	bool isEmpty = true;
	while (currentNode < stopNode) {
		__global const IndexBVHArrayNode* restrict node = &pgicCausticPhotonsBVHNodes[currentNode];

		const uint nodeData = node->nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const uint entryIndex = node->entryLeaf.entryIndex;
			__global const Photon* restrict entry = &pgicCausticPhotons[entryIndex];

			const float distance2 = DistanceSquared(p, VLOAD3F(&entry->p.x));
			if ((distance2 < pgicCausticLookUpRadius2) &&
					(entry->isVolume == isVolume) &&
						(isVolume ||
						((dot(n, -VLOAD3F(&entry->d.x)) > DEFAULT_COS_EPSILON_STATIC) &&
						(dot(n, VLOAD3F(&entry->landingSurfaceNormal.x)) > pgicCausticLookUpNormalCosAngle))
						)
				) {
				const float3 alpha = PGICPhotonBvh_ConnectCacheEntry(entry, bsdf MATERIALS_PARAM) * factor;
				VADD3F(radiance[entry->lightID].c, alpha * scale);
				isEmpty = false;
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node->bvhNode.bboxMin[0] && p.x <= node->bvhNode.bboxMax[0] &&
					p.y >= node->bvhNode.bboxMin[1] && p.y <= node->bvhNode.bboxMax[1] &&
					p.z >= node->bvhNode.bboxMin[2] && p.z <= node->bvhNode.bboxMax[2])
				++currentNode;
			else {
				// I don't need to use IndexBVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return isEmpty;
}

OPENCL_FORCE_INLINE bool PhotonGICache_ConnectWithCausticPaths(__global const BSDF *bsdf,
		__global const Photon* restrict pgicCausticPhotons,
		__global const IndexBVHArrayNode* restrict pgicCausticPhotonsBVHNodes,
		const uint pgicCausticPhotonTracedCount,
		const float pgicCausticLookUpRadius, const float pgicCausticLookUpNormalCosAngle,
		const float3 scale,
		__global Spectrum *radiance
		MATERIALS_PARAM_DECL) {

	return pgicCausticPhotons ?
		PGICPhotonBvh_ConnectAllNearEntries(bsdf, pgicCausticPhotons, pgicCausticPhotonsBVHNodes,
			pgicCausticPhotonTracedCount, pgicCausticLookUpRadius, pgicCausticLookUpNormalCosAngle,
			scale, radiance
			MATERIALS_PARAM) :
		true;
}
