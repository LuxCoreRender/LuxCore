#line 2 "bsdf_funcs.cl"

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

// Used when hitting a surface
OPENCL_FORCE_INLINE void BSDF_Init(
		__global BSDF *bsdf,
		const bool throughShadowTransparency,
		__global Ray *ray,
		__global const RayHit *rayHit,
		const float passThroughEvent,
		__global PathVolumeInfo *volInfo
		MATERIALS_PARAM_DECL
		) {
	const uint meshIndex = rayHit->meshIndex;
	const uint triangleIndex = rayHit->triangleIndex;

	// Initialized local to world object space transformation
	ExtMesh_GetLocal2World(ray->time, meshIndex, triangleIndex, &bsdf->hitPoint.localToWorld EXTMESH_PARAM);

	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
	const float3 hitPointP = rayOrig + rayHit->t * rayDir;

	HitPoint_Init(&bsdf->hitPoint, throughShadowTransparency,
		meshIndex, triangleIndex,
		hitPointP, -rayDir,
		rayHit->b1, rayHit->b2,
		passThroughEvent
		MATERIALS_PARAM);

	// Save the scene object index
	bsdf->sceneObjectIndex = meshIndex;

	// Get the material
	const uint matIndex = sceneObjs[meshIndex].materialIndex;
	bsdf->materialIndex = matIndex;

	//--------------------------------------------------------------------------
	// Set interior and exterior volumes
	//--------------------------------------------------------------------------

	PathVolumeInfo_SetHitPointVolumes(
			volInfo,
			&bsdf->hitPoint,
			Material_GetInteriorVolume(matIndex, &bsdf->hitPoint,
				passThroughEvent
				MATERIALS_PARAM),
			Material_GetExteriorVolume(matIndex, &bsdf->hitPoint,
				passThroughEvent
				MATERIALS_PARAM)
			MATERIALS_PARAM);

	//--------------------------------------------------------------------------

	// Check if it is a light source
	const uint offset = lightIndexOffsetByMeshIndex[meshIndex];
	bsdf->triangleLightSourceIndex = (offset == NULL_INDEX) ? NULL_INDEX : lightIndexByTriIndex[offset + triangleIndex];

	//--------------------------------------------------------------------------
	// Apply bump or normal mapping
	//--------------------------------------------------------------------------

	Material_Bump(matIndex, &bsdf->hitPoint
			MATERIALS_PARAM);
	// Re-read the shadeN modified by Material_Bump()
	const float3 shadeN = VLOAD3F(&bsdf->hitPoint.shadeN.x);
	const float3 dpdu = VLOAD3F(&bsdf->hitPoint.dpdu.x);
	const float3 dpdv = VLOAD3F(&bsdf->hitPoint.dpdv.x);

	//--------------------------------------------------------------------------
	// Build the local reference system
	//--------------------------------------------------------------------------
	Frame_Set(&bsdf->frame, dpdu, dpdv, shadeN);

	bsdf->isVolume = false;
}

// Used when hitting a volume scatter point
OPENCL_FORCE_INLINE void BSDF_InitVolume(
		__global BSDF *bsdf,
		const bool throughShadowTransparency,
		__global const Material* restrict mats,
		__global Ray *ray,
		const uint volumeIndex, const float t, const float passThroughEvent) {
	bsdf->hitPoint.throughShadowTransparency = throughShadowTransparency;
	bsdf->hitPoint.passThroughEvent = passThroughEvent;

	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);

	const float3 hitPointP = rayOrig + t * rayDir;
	VSTORE3F(hitPointP, &bsdf->hitPoint.p.x);
	const float3 geometryN = -rayDir;
	VSTORE3F(geometryN, &bsdf->hitPoint.fixedDir.x);

	bsdf->sceneObjectIndex = NULL_INDEX;
	Matrix4x4_IdentityGlobal(&bsdf->hitPoint.localToWorld.m);
	bsdf->materialIndex = volumeIndex;

	VSTORE3F(geometryN, &bsdf->hitPoint.geometryN.x);
	VSTORE3F(geometryN, &bsdf->hitPoint.interpolatedN.x);
	VSTORE3F(geometryN, &bsdf->hitPoint.shadeN.x);

	bsdf->hitPoint.intoObject = true;
	bsdf->hitPoint.interiorVolumeIndex = volumeIndex;
	bsdf->hitPoint.exteriorVolumeIndex = volumeIndex;
	const uint iorTexIndex = (volumeIndex != NULL_INDEX) ? mats[volumeIndex].volume.iorTexIndex : NULL_INDEX;
	bsdf->hitPoint.interiorIorTexIndex = iorTexIndex;
	bsdf->hitPoint.exteriorIorTexIndex = iorTexIndex;

	bsdf->triangleLightSourceIndex = NULL_INDEX;

	VSTORE2F(MAKE_FLOAT2(0.f, 0.f), &bsdf->hitPoint.defaultUV.u);

	float3 dpdu, dpdv;
	CoordinateSystem(geometryN, &dpdu, &dpdv);
	VSTORE3F(dpdu, &bsdf->hitPoint.dpdu.x);
	VSTORE3F(dpdv, &bsdf->hitPoint.dpdv.x);
	VSTORE3F(MAKE_FLOAT3(0.f, 0.f, 0.f), &bsdf->hitPoint.dndu.x);
	VSTORE3F(MAKE_FLOAT3(0.f, 0.f, 0.f), &bsdf->hitPoint.dndv.x);

	bsdf->hitPoint.meshIndex = NULL_INDEX;
	bsdf->hitPoint.triangleIndex = NULL_INDEX;
	bsdf->hitPoint.triangleBariCoord1 = 0.f;
	bsdf->hitPoint.triangleBariCoord2 = 0.f;

	bsdf->hitPoint.objectID = NULL_INDEX;

	// Build the local reference system
	Frame_SetFromZ(&bsdf->frame, geometryN);

	bsdf->isVolume = true;
}

OPENCL_FORCE_INLINE float3 BSDF_Albedo(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_Albedo(bsdf->materialIndex, &bsdf->hitPoint
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// "Taming the Shadow Terminator"
// by Matt Jen-Yuan Chiang, Yining Karl Li and Brent Burley
// https://www.yiningkarlli.com/projects/shadowterminator.html
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float BSDF_ShadowTerminatorAvoidanceFactor(const float3 Ni, const float3 Ns,
		const float3 lightDir) {
	const float dotNsLightDir = dot(Ns, lightDir);
	if (dotNsLightDir <= 0.f)
		return 0.f;

	const float dotNiNs = dot(Ni, Ns);
	if (dotNiNs <= 0.f)
		return 0.f;

	const float G = fmin(1.f, dot(Ni, lightDir) / (dotNsLightDir * dotNiNs));
	if (G <= 0.f)
		return 0.f;
	
	const float G2 = G * G;
	const float G3 = G2 * G;

	return -G3 + G2 + G;
}

OPENCL_FORCE_INLINE float3 BSDF_Evaluate(__global const BSDF *bsdf,
		const float3 generatedDir, BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	//const Vector &eyeDir = fromLight ? generatedDir : hitPoint.fixedDir;
	//const Vector &lightDir = fromLight ? hitPoint.fixedDir : generatedDir;
	const float3 eyeDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 lightDir = generatedDir;
	const float3 geometryN = VLOAD3F(&bsdf->hitPoint.geometryN.x);
	const float3 interpolatedN =  VLOAD3F(&bsdf->hitPoint.interpolatedN.x);
	const float3 shadeN =  VLOAD3F(&bsdf->hitPoint.shadeN.x);

	const float dotLightDirNG = dot(lightDir, geometryN);
	const float absDotLightDirNG = fabs(dotLightDirNG);
	const float dotEyeDirNG = dot(eyeDir, geometryN);
	const float absDotEyeDirNG = fabs(dotEyeDirNG);

	if (!bsdf->isVolume) {
		// These kind of tests make sense only for materials

		// Avoid glancing angles
		if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
				(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
			return BLACK;

		// Check geometry normal and light direction side
		const float sideTest = dotEyeDirNG * dotLightDirNG;
		const BSDFEvent matEvents = Material_GetEventTypes(bsdf->materialIndex
				MATERIALS_PARAM);
		if (((sideTest > 0.f) && !(matEvents & REFLECT)) ||
				((sideTest < 0.f) && !(matEvents & TRANSMIT)))
			return BLACK;

		// Check shading normal and light direction side
		const float sideTestIS = dot(eyeDir, interpolatedN) * dot(lightDir, interpolatedN);
		if (((sideTestIS > 0.f) && !(matEvents & REFLECT)) ||
				((sideTestIS < 0.f) && !(matEvents & TRANSMIT)))
			return BLACK;
	}

	__global const Frame *frame = &bsdf->frame;
	const float3 localLightDir = Frame_ToLocal(frame, lightDir);
	const float3 localEyeDir = Frame_ToLocal(frame, eyeDir);
	float3 result = Material_Evaluate(bsdf->materialIndex, &bsdf->hitPoint,
			localLightDir, localEyeDir,	event, directPdfW
			MATERIALS_PARAM);
	if (Spectrum_IsBlack(result))
		return BLACK;

	if (!bsdf->isVolume) {
		// Shadow terminator artefact avoidance
		if ((*event & REFLECT) &&
				(*event & (DIFFUSE | GLOSSY)) &&
				((shadeN.x != interpolatedN.x) || (shadeN.y != interpolatedN.y) || (shadeN.z != interpolatedN.z)))
			result *= BSDF_ShadowTerminatorAvoidanceFactor(BSDF_GetLandingInterpolatedN(bsdf),
					BSDF_GetLandingShadeN(bsdf), lightDir);
	}

	return result;
}

OPENCL_FORCE_INLINE float3 BSDF_Sample(__global const BSDF *bsdf, const float u0, const float u1,
		float3 *sampledDir, float *pdfW, float *absCosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	const float3 fixedDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, fixedDir);
	float3 localSampledDir;

	float3 result = Material_Sample(bsdf->materialIndex, &bsdf->hitPoint,
			localFixedDir, &localSampledDir, u0, u1,
			bsdf->hitPoint.passThroughEvent,
			pdfW, event
			MATERIALS_PARAM);
	if (Spectrum_IsBlack(result))
		return BLACK;

	*absCosSampledDir = fabs(CosTheta(localSampledDir));
	*sampledDir = Frame_ToWorld(&bsdf->frame, localSampledDir);

	if (!bsdf->isVolume) {
		// Shadow terminator artefact avoidance
		const float3 shadeN = VLOAD3F(&bsdf->hitPoint.shadeN.x);
		const float3 interpolatedN = VLOAD3F(&bsdf->hitPoint.interpolatedN.x);
		if ((*event & REFLECT) &&
				(*event & (DIFFUSE | GLOSSY)) &&
				((shadeN.x != interpolatedN.x) || (shadeN.y != interpolatedN.y) || (shadeN.z != interpolatedN.z)))
			result *= BSDF_ShadowTerminatorAvoidanceFactor(BSDF_GetLandingInterpolatedN(bsdf),
					BSDF_GetLandingShadeN(bsdf), *sampledDir);
	}

	return result;
}

OPENCL_FORCE_INLINE bool BSDF_IsLightSource(__global const BSDF *bsdf) {
	return (bsdf->triangleLightSourceIndex != NULL_INDEX);
}

OPENCL_FORCE_INLINE float3 BSDF_GetEmittedRadiance(__global const BSDF *bsdf, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const uint triangleLightSourceIndex = bsdf->triangleLightSourceIndex;
	if (triangleLightSourceIndex == NULL_INDEX)
		return BLACK;
	else
		return IntersectableLight_GetRadiance(&lights[triangleLightSourceIndex],
				&bsdf->hitPoint, directPdfA
				LIGHTS_PARAM);
}

OPENCL_FORCE_INLINE float3 BSDF_GetPassThroughTransparency(__global const BSDF *bsdf, const bool backTracing
		MATERIALS_PARAM_DECL) {
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x));

	return Material_GetPassThroughTransparency(bsdf->materialIndex,
			&bsdf->hitPoint, localFixedDir, bsdf->hitPoint.passThroughEvent,backTracing
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE float BSDF_IsPhotonGIEnabled(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsPhotonGIEnabled(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE bool BSDF_IsHoldout(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsHoldout(bsdf->materialIndex
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Shadow catcher related functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool BSDF_IsShadowCatcher(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	return (matIndex == NULL_INDEX) ? false : mats[matIndex].isShadowCatcher;
}

OPENCL_FORCE_INLINE bool BSDF_IsShadowCatcherOnlyInfiniteLights(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	return (matIndex == NULL_INDEX) ? false : (mats[matIndex].isShadowCatcher && mats[matIndex].isShadowCatcherOnlyInfiniteLights);
}

OPENCL_FORCE_INLINE float3 BSDF_ShadowCatcherSample(__global const BSDF *bsdf,
		float3 *sampledDir, float *pdfW, float *absCosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	// Just continue to trace the ray
	*sampledDir = -VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	*absCosSampledDir = fabs(dot(*sampledDir, VLOAD3F(&bsdf->hitPoint.geometryN.x)));

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	const float3 result = WHITE;

	// Adjoint BSDF
//	if (hitPoint.fromLight) {
//		const float absDotFixedDirNG = AbsDot(hitPoint.fixedDir, hitPoint.geometryN);
//		const float absDotSampledDirNG = AbsDot(*sampledDir, hitPoint.geometryN);
//		return result * (absDotSampledDirNG / absDotFixedDirNG);
//	} else
		return result;
}

//------------------------------------------------------------------------------
// Bake related functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool BSDF_HasBakeMap(__global const BSDF *bsdf, const BakeMapType type
		MATERIALS_PARAM_DECL) {
	return (bsdf->sceneObjectIndex != NULL_INDEX) &&
			(sceneObjs[bsdf->sceneObjectIndex].bakeMapIndex != NULL_INDEX) &&
			(sceneObjs[bsdf->sceneObjectIndex].bakeMapType == type);
}

OPENCL_FORCE_INLINE float3 BSDF_GetBakeMapValue(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const uint mapIndex = sceneObjs[bsdf->sceneObjectIndex].bakeMapIndex;
	__global const ImageMap *imageMap = &imageMapDescs[mapIndex];

	const uint uvIndex = sceneObjs[bsdf->sceneObjectIndex].bakeMapUVIndex;
	const float2 uv = HitPoint_GetUV(&bsdf->hitPoint, uvIndex EXTMESH_PARAM);

	return ImageMap_GetSpectrum(
			imageMap,
			uv.x, uv.y
			IMAGEMAPS_PARAM);
}
