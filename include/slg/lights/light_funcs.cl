#line 2 "light_funcs.cl"

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

OPENCL_FORCE_INLINE float EnvLightSource_GetEnvRadius(const float sceneRadius) {
	// This is used to scale the world radius in sun/sky/infinite lights in order to
	// avoid problems with objects that are near the borderline of the world bounding sphere
	return PARAM_LIGHT_WORLD_RADIUS_SCALE * sceneRadius;
}

OPENCL_FORCE_INLINE void EnvLightSource_ToLatLongMapping(const float3 w,
		float *s, float *t, float *pdf) {
	const float theta = SphericalTheta(w);

	*s = SphericalPhi(w) * (1.f / (2.f * M_PI_F));
	*t = theta * M_1_PI_F;

	if (pdf) {
		const float sinTheta = sin(theta);
		*pdf = (sinTheta > 0.f) ? ((1.f / (2.f * M_PI_F)) * M_1_PI_F / sinTheta) : 0.f;
	}
}

OPENCL_FORCE_INLINE void EnvLightSource_FromLatLongMapping(const float s, const float t,
		float3 *w, float *pdf) {
	const float phi = s * 2.f * M_PI_F;
	const float theta = t * M_PI_F;
	const float sinTheta = sin(theta);

	*w = SphericalDirection(sinTheta, cos(theta), phi);

	if (pdf)
		*pdf = (sinTheta > 0.f) ? ((1.f / (2.f * M_PI_F)) * M_1_PI_F / sinTheta) : 0.f;
}

//------------------------------------------------------------------------------
// ConstantInfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)

OPENCL_FORCE_NOT_INLINE float3 ConstantInfiniteLight_GetRadiance(__global const LightSource *constantInfiniteLight,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const uint offset = constantInfiniteLight->notIntersectable.constantInfinite.distributionOffset;
	__global const float *visibilityLightDist = (offset != NULL_INDEX) ? &envLightDistribution[offset] : NULL;
	const bool useVisibilityMapCache = constantInfiniteLight->notIntersectable.constantInfinite.useVisibilityMapCache;

	if (visibilityLightDist || useVisibilityMapCache) {
		const float3 w = -dir;
		float u, v, latLongMappingPdf;
		EnvLightSource_ToLatLongMapping(w, &u, &v, &latLongMappingPdf);
		if (latLongMappingPdf == 0.f)
			return BLACK;

		if (!bsdf)
			*directPdfA = 0.f;
		else if (useVisibilityMapCache && EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
			__global const float *cacheDist = EnvLightVisibilityCache_GetVisibilityMap(bsdf LIGHTS_PARAM);
			if (cacheDist) {
				const float cacheDistPdf = Distribution2D_Pdf(cacheDist, u, v);

				*directPdfA = cacheDistPdf * latLongMappingPdf;
			} else
				*directPdfA = 0.f;
		} else if (visibilityLightDist) {
			const float distPdf = Distribution2D_Pdf(visibilityLightDist, u, v);
			*directPdfA = distPdf * latLongMappingPdf;
		} else
			*directPdfA = 0.f;
	} else
		*directPdfA = bsdf ? UniformSpherePdf() : 0.f;

	return VLOAD3F(constantInfiniteLight->notIntersectable.gain.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.constantInfinite.color.c);
}

OPENCL_FORCE_NOT_INLINE float3 ConstantInfiniteLight_Illuminate(__global const LightSource *constantInfiniteLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW
		LIGHTS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const uint offset = constantInfiniteLight->notIntersectable.constantInfinite.distributionOffset;
	__global const float *visibilityLightDistribution = (offset != NULL_INDEX) ? &envLightDistribution[offset] : NULL;
	const bool useVisibilityMapCache = constantInfiniteLight->notIntersectable.constantInfinite.useVisibilityMapCache;

	if (visibilityLightDistribution || useVisibilityMapCache) {
		float2 sampleUV;
		float distPdf;

		if (useVisibilityMapCache && EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
			__global const float *cacheDist = EnvLightVisibilityCache_GetVisibilityMap(bsdf LIGHTS_PARAM);
			if (cacheDist)
				Distribution2D_SampleContinuous(cacheDist, u0, u1, &sampleUV, &distPdf);
			else
				return BLACK;
		} else if (visibilityLightDistribution)
			Distribution2D_SampleContinuous(visibilityLightDistribution, u0, u1, &sampleUV, &distPdf);
		else
			return BLACK;

		if (distPdf == 0.f)
			return BLACK;

		float latLongMappingPdf;
		EnvLightSource_FromLatLongMapping(sampleUV.s0, sampleUV.s1, dir, &latLongMappingPdf);
		if (latLongMappingPdf == 0.f)
			return BLACK;

		*directPdfW = distPdf * latLongMappingPdf;
	} else {
		*dir = UniformSampleSphere(u0, u1);

		*directPdfW = UniformSpherePdf();
	}

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = p + (*distance) * (*dir);
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	return VLOAD3F(constantInfiniteLight->notIntersectable.gain.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.constantInfinite.color.c);
}

#endif

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_INFINITELIGHT) && defined(PARAM_HAS_IMAGEMAPS)

OPENCL_FORCE_NOT_INLINE float3 InfiniteLight_GetRadiance(__global const LightSource *infiniteLight,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const float3 localDir = normalize(Transform_InvApplyVector(&infiniteLight->notIntersectable.light2World, -dir));

 	float u, v, latLongMappingPdf;
	EnvLightSource_ToLatLongMapping(localDir, &u, &v, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	if (!bsdf)
		*directPdfA = 0.f;
	else if (infiniteLight->notIntersectable.infinite.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		__global const float *cacheDist = EnvLightVisibilityCache_GetVisibilityMap(bsdf LIGHTS_PARAM);

		if (cacheDist) {
			const float cacheDistPdf = Distribution2D_Pdf(cacheDist, u, v);

			*directPdfA = cacheDistPdf * latLongMappingPdf;
		} else
			*directPdfA = 0.f;
	} else {
		__global const float *infiniteLightDist = &envLightDistribution[infiniteLight->notIntersectable.infinite.distributionOffset];

		const float distPdf = Distribution2D_Pdf(infiniteLightDist, u, v);
		*directPdfA = distPdf * latLongMappingPdf;
	}

	__global const ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersectable.infinite.imageMapIndex];
	return VLOAD3F(infiniteLight->notIntersectable.gain.c) * ImageMap_GetSpectrum(
			imageMap,
			u, v
			IMAGEMAPS_PARAM);
}

OPENCL_FORCE_NOT_INLINE float3 InfiniteLight_Illuminate(__global const LightSource *infiniteLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW
		LIGHTS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	__global const float *infiniteLightDistribution;
	if (infiniteLight->notIntersectable.infinite.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		infiniteLightDistribution = EnvLightVisibilityCache_GetVisibilityMap(bsdf LIGHTS_PARAM);
		if (!infiniteLightDistribution)
			return BLACK;
	} else
		infiniteLightDistribution = &envLightDistribution[infiniteLight->notIntersectable.infinite.distributionOffset];

	float2 sampleUV;
	float distPdf;
	Distribution2D_SampleContinuous(infiniteLightDistribution, u0, u1, &sampleUV, &distPdf);
	if (distPdf == 0.f)
			return BLACK;

	float3 localDir;
	float latLongMappingPdf;
	EnvLightSource_FromLatLongMapping(sampleUV.s0, sampleUV.s1, &localDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	*dir = normalize(Transform_ApplyVector(&infiniteLight->notIntersectable.light2World, localDir));

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = p + (*distance) * (*dir);
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*directPdfW = distPdf * latLongMappingPdf;

	// InfiniteLight_GetRadiance is expended here
	__global const ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersectable.infinite.imageMapIndex];

	const float2 uv = (float2)(sampleUV.s0, sampleUV.s1);

	return VLOAD3F(infiniteLight->notIntersectable.gain.c) * ImageMap_GetSpectrum(
			imageMap,
			uv.s0, uv.s1
			IMAGEMAPS_PARAM);
}

#endif

//------------------------------------------------------------------------------
// Sky2Light
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT2)

OPENCL_FORCE_INLINE float RiCosBetween(const float3 w1, const float3 w2) {
	return clamp(dot(w1, w2), -1.f, 1.f);
}

OPENCL_FORCE_INLINE float3 SkyLight2_ComputeSkyRadiance(__global const LightSource *skyLight2, const float3 w) {
	const float3 absoluteSunDir = VLOAD3F(&skyLight2->notIntersectable.sky2.absoluteSunDir.x);
	const float cosG = RiCosBetween(w, absoluteSunDir);
	const float cosG2 = cosG * cosG;
	const float gamma = acos(cosG);
	const float cosT = fmax(0.f, CosTheta(w));

	const float3 aTerm = VLOAD3F(skyLight2->notIntersectable.sky2.aTerm.c);
	const float3 bTerm = VLOAD3F(skyLight2->notIntersectable.sky2.bTerm.c);
	const float3 cTerm = VLOAD3F(skyLight2->notIntersectable.sky2.cTerm.c);
	const float3 dTerm = VLOAD3F(skyLight2->notIntersectable.sky2.dTerm.c);
	const float3 eTerm = VLOAD3F(skyLight2->notIntersectable.sky2.eTerm.c);
	const float3 fTerm = VLOAD3F(skyLight2->notIntersectable.sky2.fTerm.c);
	const float3 gTerm = VLOAD3F(skyLight2->notIntersectable.sky2.gTerm.c);
	const float3 hTerm = VLOAD3F(skyLight2->notIntersectable.sky2.hTerm.c);
	const float3 iTerm = VLOAD3F(skyLight2->notIntersectable.sky2.iTerm.c);
	const float3 radianceTerm = VLOAD3F(skyLight2->notIntersectable.sky2.radianceTerm.c);
	
	const float3 expTerm = dTerm * Spectrum_Exp(eTerm * gamma);
	const float3 rayleighTerm = fTerm * cosG2;
	const float3 mieTerm = gTerm * (1.f + cosG2) /
		Spectrum_Pow(1.f + iTerm * (iTerm - (2.f * cosG)), 1.5f);
	const float3 zenithTerm = hTerm * sqrt(cosT);

	// 683 is a scaling factor to convert W to lm
	return 683.f * (1.f + aTerm * Spectrum_Exp(bTerm / (cosT + .01f))) *
		(cTerm + expTerm + rayleighTerm + mieTerm + zenithTerm) * radianceTerm;
}

OPENCL_FORCE_INLINE float3 SkyLight2_ComputeRadiance(__global const LightSource *skyLight2, const float3 w) {
	if (skyLight2->notIntersectable.sky2.hasGround &&
			(dot(w, VLOAD3F(&skyLight2->notIntersectable.sky2.absoluteUpDir.x)) < 0.f)) {
		// Lower hemisphere
		return VLOAD3F(skyLight2->notIntersectable.sky2.scaledGroundColor.c);
	} else
		return VLOAD3F(skyLight2->notIntersectable.gain.c) * SkyLight2_ComputeSkyRadiance(skyLight2, w);
}

OPENCL_FORCE_NOT_INLINE float3 SkyLight2_GetRadiance(__global const LightSource *skyLight2,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const float3 w = -dir;
	float u, v, latLongMappingPdf;
	EnvLightSource_ToLatLongMapping(w, &u, &v, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	if (!bsdf)
		*directPdfA = 0.f;
	else /*if (skyLight2->notIntersectable.sky2.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		__global const float *cacheDist = EnvLightVisibilityCache_GetVisibilityMap(bsdf LIGHTS_PARAM);
		if (cacheDist) {
			const float cacheDistPdf = Distribution2D_Pdf(cacheDist, u, v);

			*directPdfA = cacheDistPdf * latLongMappingPdf;
		} else
			*directPdfA = 0.f;
	} else */{
		__global const float *skyLightDist = &envLightDistribution[skyLight2->notIntersectable.sky2.distributionOffset];

		const float distPdf = Distribution2D_Pdf(skyLightDist, u, v);
		*directPdfA = distPdf * latLongMappingPdf;
	}

	return SkyLight2_ComputeRadiance(skyLight2, w);
}

OPENCL_FORCE_NOT_INLINE float3 SkyLight2_Illuminate(__global const LightSource *skyLight2,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW
		LIGHTS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	__global const float *skyLightDistribution;
	if (skyLight2->notIntersectable.sky2.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		skyLightDistribution = EnvLightVisibilityCache_GetVisibilityMap(bsdf LIGHTS_PARAM);
		if (!skyLightDistribution)
			return BLACK;
	} else
		skyLightDistribution = &envLightDistribution[skyLight2->notIntersectable.sky2.distributionOffset];

	float2 sampleUV;
	float distPdf;
	Distribution2D_SampleContinuous(skyLightDistribution, u0, u1, &sampleUV, &distPdf);
	if (distPdf == 0.f)
			return BLACK;

	float latLongMappingPdf;
	EnvLightSource_FromLatLongMapping(sampleUV.s0, sampleUV.s1, dir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = p + (*distance) * (*dir);
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*directPdfW = distPdf * latLongMappingPdf;

	return SkyLight2_ComputeRadiance(skyLight2, *dir);
}

#endif

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT)

OPENCL_FORCE_NOT_INLINE float3 SunLight_Illuminate(__global const LightSource *sunLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float cosThetaMax = sunLight->notIntersectable.sun.cosThetaMax;
	const float3 sunDir = VLOAD3F(&sunLight->notIntersectable.sun.absoluteDir.x);
	*dir = UniformSampleCone(u0, u1, cosThetaMax, VLOAD3F(&sunLight->notIntersectable.sun.x.x), VLOAD3F(&sunLight->notIntersectable.sun.y.x), sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = dot(sunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return BLACK;

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);
	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	*directPdfW = UniformConePdf(cosThetaMax);

	return VLOAD3F(sunLight->notIntersectable.sun.color.c);
}

OPENCL_FORCE_NOT_INLINE float3 SunLight_GetRadiance(__global const LightSource *sunLight,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA) {
	const float cosThetaMax = sunLight->notIntersectable.sun.cosThetaMax;
	const float sin2ThetaMax = sunLight->notIntersectable.sun.sin2ThetaMax;
	const float3 x = VLOAD3F(&sunLight->notIntersectable.sun.x.x);
	const float3 y = VLOAD3F(&sunLight->notIntersectable.sun.y.x);
	const float3 absoluteSunDir = VLOAD3F(&sunLight->notIntersectable.sun.absoluteDir.x);

	const float xD = dot(-dir, x);
	const float yD = dot(-dir, y);
	const float zD = dot(-dir, absoluteSunDir);
	if ((cosThetaMax == 1.f) || (zD < 0.f) || ((xD * xD + yD * yD) > sin2ThetaMax))
		return BLACK;

	if (directPdfA)
		*directPdfA = UniformConePdf(cosThetaMax);

	return VLOAD3F(sunLight->notIntersectable.sun.color.c);
}

#endif

//------------------------------------------------------------------------------
// TriangleLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_TRIANGLELIGHT)

OPENCL_FORCE_NOT_INLINE float3 TriangleLight_Illuminate(__global const LightSource *triLight,
		__global HitPoint *tmpHitPoint,
		__global const BSDF *bsdf, const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float3 *dir, float *distance, float *directPdfW
		MATERIALS_PARAM_DECL) {
	// A safety check to avoid NaN/Inf
	if ((triLight->triangle.invTriangleArea == 0.f) || (triLight->triangle.invMeshArea == 0.f))
		return BLACK;

	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 p0 = VLOAD3F(&triLight->triangle.v0.x);
	const float3 p1 = VLOAD3F(&triLight->triangle.v1.x);
	const float3 p2 = VLOAD3F(&triLight->triangle.v2.x);
	float b0, b1, b2;
	float3 samplePoint = Triangle_Sample(
			p0, p1, p2,
			u0, u1,
			&b0, &b1, &b2);

	const float3 n0 = VLOAD3F(&triLight->triangle.n0.x);
	const float3 n1 = VLOAD3F(&triLight->triangle.n1.x);
	const float3 n2 = VLOAD3F(&triLight->triangle.n2.x);
	const float3 shadeN = Triangle_InterpolateNormal(n0, n1, n2, b0, b1, b2);

	// Move p along the geometry normal by an epsilon to avoid self-shadow problems	
	samplePoint += shadeN * MachineEpsilon_E_Float3(shadeN);

	*dir = samplePoint - p;
	const float distanceSquared = dot(*dir, *dir);
	*distance = sqrt(distanceSquared);
	*dir /= (*distance);
	
	const float cosAtLight = dot(shadeN, -(*dir));
	const float cosThetaMax = Material_GetEmittedCosThetaMax(triLight->triangle.materialIndex
			MATERIALS_PARAM);
	// emissionFunc can emit light even backward, this is for compatibility with classic Lux
	if ((triLight->triangle.imageMapIndex == NULL_INDEX) && (cosAtLight < cosThetaMax - DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	// Build a temporary hit point on the emitting point of the light source
	VSTORE3F(samplePoint, &tmpHitPoint->p.x);

#if defined(PARAM_HAS_PASSTHROUGH)
	tmpHitPoint->passThroughEvent = passThroughEvent;
#endif

	const float3 geometryN = VLOAD3F(&triLight->triangle.geometryN.x);
	VSTORE3F(geometryN, &tmpHitPoint->geometryN.x);
    VSTORE3F(shadeN, &tmpHitPoint->shadeN.x);
	VSTORE3F(-shadeN, &tmpHitPoint->fixedDir.x);

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY) || defined(PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR)
	const float3 rgb0 = VLOAD3F(triLight->triangle.rgb0.c);
	const float3 rgb1 = VLOAD3F(triLight->triangle.rgb1.c);
	const float3 rgb2 = VLOAD3F(triLight->triangle.rgb2.c);
	const float3 triColor = Triangle_InterpolateColor(rgb0, rgb1, rgb2, b0, b1, b2);

	VSTORE3F(triColor, tmpHitPoint->color.c);
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	tmpHitPoint->alpha = Triangle_InterpolateAlpha(triLight->triangle.alpha0,
			triLight->triangle.alpha1, triLight->triangle.alpha2, b0, b1, b2);
#endif
	Matrix4x4_IdentityGlobal(&tmpHitPoint->worldToLocal);
#if defined(PARAM_HAS_VOLUMES)
	tmpHitPoint->interiorVolumeIndex = NULL_INDEX;
	tmpHitPoint->exteriorVolumeIndex = NULL_INDEX;
	tmpHitPoint->intoObject = true;
#endif

	const float2 uv0 = VLOAD2F(&triLight->triangle.uv0.u);
	const float2 uv1 = VLOAD2F(&triLight->triangle.uv1.u);
	const float2 uv2 = VLOAD2F(&triLight->triangle.uv2.u);
	const float2 triUV = Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);
	VSTORE2F(triUV, &tmpHitPoint->uv.u);

	// Apply Bump mapping and get proper differentials?
#if defined(PARAM_HAS_BUMPMAPS)
	float3 dpdu, dpdv;
	CoordinateSystem(shadeN, &dpdu, &dpdv);
	VSTORE3F(dpdu, &tmpHitPoint->dpdu.x);
	VSTORE3F(dpdv, &tmpHitPoint->dpdv.x);
#endif

#if defined(PARAM_ENABLE_TEX_OBJECTID) || defined(PARAM_ENABLE_TEX_OBJECTID_COLOR) || defined(PARAM_ENABLE_TEX_OBJECTID_NORMALIZED)
	tmpHitPoint->objectID = triLight->triangle.objectID;
#endif

	float3 emissionColor = WHITE;
#if defined(PARAM_HAS_IMAGEMAPS)
	if (triLight->triangle.imageMapIndex != NULL_INDEX) {
		// Build the local frame
		float3 X, Y;
		CoordinateSystem(shadeN, &X, &Y);

		const float3 localFromLight = ToLocal(X, Y, shadeN, -(*dir));

		// Retrieve the image map information
		__global const ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
		emissionColor = ImageMap_GetSpectrum(
				imageMap,
				uv.s0, uv.s1
				IMAGEMAPS_PARAM) / triLight->triangle.avarage;

		*directPdfW = triLight->triangle.invTriangleArea * distanceSquared ;
	} else
#endif
		*directPdfW = triLight->triangle.invTriangleArea * distanceSquared / fabs(cosAtLight);

	return Material_GetEmittedRadiance(triLight->triangle.materialIndex,
			tmpHitPoint, triLight->triangle.invMeshArea
			MATERIALS_PARAM) * emissionColor;
}

OPENCL_FORCE_NOT_INLINE float3 TriangleLight_GetRadiance(__global const LightSource *triLight,
		 __global const HitPoint *hitPoint, float *directPdfA
		MATERIALS_PARAM_DECL) {
	const float3 dir = VLOAD3F(&hitPoint->fixedDir.x);
	const float3 hitPointNormal = VLOAD3F(&hitPoint->shadeN.x);
	const float cosOutLight = dot(hitPointNormal, dir);
	const float cosThetaMax = Material_GetEmittedCosThetaMax(triLight->triangle.materialIndex
			MATERIALS_PARAM);
	// emissionFunc can emit light even backward, this is for compatibility with classic Lux
	if (((triLight->triangle.imageMapIndex == NULL_INDEX) && (cosOutLight < cosThetaMax - DEFAULT_COS_EPSILON_STATIC)) ||
			// A safety check to avoid NaN/Inf
			(triLight->triangle.invTriangleArea == 0.f) || (triLight->triangle.invMeshArea == 0.f))
		return BLACK;

	if (directPdfA)
		*directPdfA = triLight->triangle.invTriangleArea;

	float3 emissionColor = WHITE;
#if defined(PARAM_HAS_IMAGEMAPS)
	if (triLight->triangle.imageMapIndex != NULL_INDEX) {
		// Build the local frame
		float3 X, Y;
		CoordinateSystem(hitPointNormal, &X, &Y);

		const float3 localFromLight = ToLocal(X, Y, hitPointNormal, dir);

		// Retrieve the image map information
		__global const ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
		emissionColor = ImageMap_GetSpectrum(
				imageMap,
				uv.s0, uv.s1
				IMAGEMAPS_PARAM) / triLight->triangle.avarage;
	}
#endif

	return Material_GetEmittedRadiance(triLight->triangle.materialIndex,
			hitPoint, triLight->triangle.invMeshArea
			MATERIALS_PARAM) * emissionColor;
}

#endif

//------------------------------------------------------------------------------
// PointLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_POINTLIGHT)

OPENCL_FORCE_NOT_INLINE float3 PointLight_Illuminate(__global const LightSource *pointLight,
		__global const BSDF *bsdf,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 toLight = VLOAD3F(&pointLight->notIntersectable.point.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	*directPdfW = distanceSquared;

	return VLOAD3F(pointLight->notIntersectable.point.emittedFactor.c) * (1.f / (4.f * M_PI_F));
}

#endif

//------------------------------------------------------------------------------
// SphereLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SPHERELIGHT) || (defined(PARAM_HAS_MAPSPHERELIGHT) && defined(PARAM_HAS_IMAGEMAPS))

OPENCL_FORCE_INLINE bool SphereLight_SphereIntersect(const float3 absolutePos, const float radiusSquared,
		const float3 rayOrig, const float3 rayDir, float *hitT) {
	const float3 op = absolutePos - rayOrig;
	const float b = dot(op, rayDir);

	float det = b * b - dot(op, op) + radiusSquared;
	if (det < 0.f)
		return false;
	else
		det = sqrt(det);

	const float mint = MachineEpsilon_E_Float3(rayOrig);
	const float maxt = INFINITY;

	float t = b - det;
	if ((t > mint) && ((t < maxt)))
		*hitT = t;
	else {
		t = b + det;

		if ((t > mint) && ((t < maxt)))
			*hitT = t;
		else
			return false;
	}

	return true;
}

OPENCL_FORCE_NOT_INLINE float3 SphereLight_Illuminate(__global const LightSource *pointLight,
		__global const BSDF *bsdf, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 absolutePos = VLOAD3F(&pointLight->notIntersectable.sphere.absolutePos.x);
	const float3 toLight = absolutePos - p;
	const float centerDistanceSquared = dot(toLight, toLight);
	const float centerDistance = sqrt(centerDistanceSquared);

	const float radius = pointLight->notIntersectable.sphere.radius;
	const float radiusSquared = radius * radius;

	// Check if the point is inside the sphere
	if (centerDistanceSquared - radiusSquared < DEFAULT_EPSILON_STATIC) {
		// The point is inside the sphere, return black
		return BLACK;
	}

	// The point isn't inside the sphere

	// Build a local coordinate system
	const float3 localZ = toLight * (1.f / centerDistance);
	Frame localFrame;
	Frame_SetFromZ_Private(&localFrame, localZ);

	// Sample sphere uniformly inside subtended cone
	const float cosThetaMax = sqrt(max(0.f, 1.f - radiusSquared / centerDistanceSquared));

	const float3 rayOrig = p;
	const float3 localRayDir = UniformSampleConeLocal(u0, u1, cosThetaMax);

	if (CosTheta(localRayDir) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 rayDir = Frame_ToWorld_Private(&localFrame, localRayDir);

	// Check the intersection with the sphere
	if (!SphereLight_SphereIntersect(absolutePos, radiusSquared, rayOrig, rayDir, distance))
		*distance = dot(toLight, rayDir);
	*dir = rayDir;

	*directPdfW = UniformConePdf(cosThetaMax);

	const float invArea = 1.f / (4.f * M_PI_F * radiusSquared);

	return VLOAD3F(pointLight->notIntersectable.sphere.emittedFactor.c) * invArea * M_1_PI_F;
}

#endif

//------------------------------------------------------------------------------
// MapPointLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_MAPPOINTLIGHT) && defined(PARAM_HAS_IMAGEMAPS)

OPENCL_FORCE_NOT_INLINE float3 MapPointLight_Illuminate(__global const LightSource *mapPointLight,
		__global const BSDF *bsdf,
		float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 toLight = VLOAD3F(&mapPointLight->notIntersectable.mapPoint.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	*directPdfW = distanceSquared;

	// Retrieve the image map information
	__global const ImageMap *imageMap = &imageMapDescs[mapPointLight->notIntersectable.mapPoint.imageMapIndex];

	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&mapPointLight->notIntersectable.light2World, -(*dir)));
	const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
	const float3 emissionColor = ImageMap_GetSpectrum(
			imageMap,
			uv.s0, uv.s1
			IMAGEMAPS_PARAM) / (4.f * M_PI_F * mapPointLight->notIntersectable.mapPoint.avarage);

	return VLOAD3F(mapPointLight->notIntersectable.mapPoint.emittedFactor.c) * emissionColor;
}

#endif

//------------------------------------------------------------------------------
// MapSphereLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_MAPSPHERELIGHT) && defined(PARAM_HAS_IMAGEMAPS)

OPENCL_FORCE_NOT_INLINE float3 MapSphereLight_Illuminate(__global const LightSource *mapSphereLight,
		__global const BSDF *bsdf,	const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 result = SphereLight_Illuminate(mapSphereLight, bsdf, u0, u1,
			dir, distance, directPdfW);

	// Retrieve the image map information
	__global const ImageMap *imageMap = &imageMapDescs[mapSphereLight->notIntersectable.mapSphere.imageMapIndex];

	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&mapSphereLight->notIntersectable.light2World, -(*dir)));
	const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
	const float3 emissionColor = ImageMap_GetSpectrum(
			imageMap,
			uv.s0, uv.s1
			IMAGEMAPS_PARAM) * (1.f / mapSphereLight->notIntersectable.mapSphere.avarage);

	return result * emissionColor;
}

#endif

//------------------------------------------------------------------------------
// SpotLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SPOTLIGHT)

OPENCL_FORCE_INLINE float SpotLight_LocalFalloff(const float3 w, const float cosTotalWidth, const float cosFalloffStart) {
	if (CosTheta(w) < cosTotalWidth)
		return 0.f;
 	if (CosTheta(w) > cosFalloffStart)
		return 1.f;

	// Compute falloff inside spotlight cone
	const float delta = (CosTheta(w) - cosTotalWidth) / (cosFalloffStart - cosTotalWidth);
	return pow(delta, 4);
}

OPENCL_FORCE_NOT_INLINE float3 SpotLight_Illuminate(__global const LightSource *spotLight,
		__global const BSDF *bsdf,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 toLight = VLOAD3F(&spotLight->notIntersectable.spot.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&spotLight->notIntersectable.light2World, -(*dir)));
	const float falloff = SpotLight_LocalFalloff(localFromLight,
			spotLight->notIntersectable.spot.cosTotalWidth,
			spotLight->notIntersectable.spot.cosFalloffStart);
	if (falloff == 0.f)
		return BLACK;

	*directPdfW = distanceSquared;

	return VLOAD3F(spotLight->notIntersectable.spot.emittedFactor.c) *
			(falloff / fabs(CosTheta(localFromLight)));
}

#endif

//------------------------------------------------------------------------------
// ProjectionLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_PROJECTIONLIGHT) && defined(PARAM_HAS_IMAGEMAPS)

OPENCL_FORCE_NOT_INLINE float3 ProjectionLight_Illuminate(__global const LightSource *projectionLight,
		__global const BSDF *bsdf,
		float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 toLight = VLOAD3F(&projectionLight->notIntersectable.projection.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	// Check the side
	if (dot(-(*dir), VLOAD3F(&projectionLight->notIntersectable.projection.lightNormal.x)) < 0.f)
		return BLACK;

	// Check if the point is inside the image plane
	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&projectionLight->notIntersectable.light2World, -(*dir)));
	const float3 p0 = Matrix4x4_ApplyPoint(
			&projectionLight->notIntersectable.projection.lightProjection, localFromLight);

	const float screenX0 = projectionLight->notIntersectable.projection.screenX0;
	const float screenX1 = projectionLight->notIntersectable.projection.screenX1;
	const float screenY0 = projectionLight->notIntersectable.projection.screenY0;
	const float screenY1 = projectionLight->notIntersectable.projection.screenY1;
	if ((p0.x < screenX0) || (p0.x >= screenX1) || (p0.y < screenY0) || (p0.y >= screenY1))
		return BLACK;

	*directPdfW = distanceSquared;

	float3 c = VLOAD3F(projectionLight->notIntersectable.projection.emittedFactor.c);
#if defined(PARAM_HAS_IMAGEMAPS)
	const uint imageMapIndex = projectionLight->notIntersectable.projection.imageMapIndex;
	if (imageMapIndex != NULL_INDEX) {
		const float u = (p0.x - screenX0) / (screenX1 - screenX0);
		const float v = (p0.y - screenY0) / (screenY1 - screenY0);
		
		// Retrieve the image map information
		__global const ImageMap *imageMap = &imageMapDescs[imageMapIndex];

		c *= ImageMap_GetSpectrum(
				imageMap,
				u, v
				IMAGEMAPS_PARAM);
	}
#endif

	return c;
}

#endif

//------------------------------------------------------------------------------
// SharpDistantLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SHARPDISTANTLIGHT)

OPENCL_FORCE_NOT_INLINE float3 SharpDistantLight_Illuminate(__global const LightSource *sharpDistantLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	*dir = -VLOAD3F(&sharpDistantLight->notIntersectable.sharpDistant.absoluteLightDir.x);

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);
	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	*directPdfW = 1.f;

	return VLOAD3F(sharpDistantLight->notIntersectable.gain.c) *
			VLOAD3F(sharpDistantLight->notIntersectable.sharpDistant.color.c);
}

#endif

//------------------------------------------------------------------------------
// DistantLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_DISTANTLIGHT)

OPENCL_FORCE_NOT_INLINE float3 DistantLight_Illuminate(__global const LightSource *distantLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 absoluteLightDir = VLOAD3F(&distantLight->notIntersectable.distant.absoluteLightDir.x);
	const float3 x = VLOAD3F(&distantLight->notIntersectable.distant.x.x);
	const float3 y = VLOAD3F(&distantLight->notIntersectable.distant.y.x);
	const float cosThetaMax = distantLight->notIntersectable.distant.cosThetaMax;
	*dir = -UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);
	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	*directPdfW = uniformConePdf;

	return VLOAD3F(distantLight->notIntersectable.gain.c) *
			VLOAD3F(distantLight->notIntersectable.sharpDistant.color.c);
}

#endif

//------------------------------------------------------------------------------
// LaserLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_LASERLIGHT)

OPENCL_FORCE_NOT_INLINE float3 LaserLight_Illuminate(__global const LightSource *laserLight,
		__global const BSDF *bsdf,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	const float3 absoluteLightPos = VLOAD3F(&laserLight->notIntersectable.laser.absoluteLightPos.x);
	const float3 absoluteLightDir = VLOAD3F(&laserLight->notIntersectable.laser.absoluteLightDir.x);

	*dir = -absoluteLightDir;
	
	const float3 rayOrig = p;
	const float3 rayDir = *dir;
	const float3 planeCenter = absoluteLightPos;
	const float3 planeNormal = absoluteLightDir;

	// Intersect the shadow ray with light plane
	const float denom = dot(planeNormal, rayDir);
	const float3 pr = planeCenter - rayOrig;
	float d = dot(pr, planeNormal);

	if (fabs(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		d /= denom; 

		if ((d <= 0.f) || (denom >= 0.f))
			return BLACK;
	} else
		return BLACK;

	const float3 lightPoint = rayOrig + d * rayDir;

	// Check if the point is inside the emitting circle
	const float radius = laserLight->notIntersectable.laser.radius;
	const float3 dist = lightPoint - absoluteLightPos;
	if (dot(dist, dist) > radius * radius)
		return BLACK;
	
	// Ok, the light is visible
	
	*distance = d;
	*directPdfW = 1.f;

	return VLOAD3F(laserLight->notIntersectable.laser.emittedFactor.c);
}

#endif

//------------------------------------------------------------------------------
// Generic light functions
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 EnvLight_GetRadiance(__global const LightSource *light,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	switch (light->type) {
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)
		case TYPE_IL_CONSTANT:
			return ConstantInfiniteLight_GetRadiance(light,
					bsdf,
					dir, directPdfA
					LIGHTS_PARAM);
#endif
#if defined(PARAM_HAS_INFINITELIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_IL:
			return InfiniteLight_GetRadiance(light,
					bsdf,
					dir, directPdfA
					LIGHTS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT2)
		case TYPE_IL_SKY2:
			return SkyLight2_GetRadiance(light,
					bsdf,
					dir, directPdfA
					LIGHTS_PARAM);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
			return SunLight_GetRadiance(light,
					bsdf,
					dir, directPdfA);
#endif
#if defined(PARAM_HAS_SHARPDISTANTLIGHT)
		case TYPE_SHARPDISTANT:
			// Just return Black
#endif
#if defined(PARAM_HAS_DISTANTLIGHT)
		case TYPE_DISTANT:
			// Just return Black
#endif

		default:
			return BLACK;
	}
}

OPENCL_FORCE_INLINE float3 IntersectableLight_GetRadiance(__global const LightSource *light,
		 __global const HitPoint *hitPoint, float *directPdfA
		LIGHTS_PARAM_DECL) {
#if defined(PARAM_HAS_TRIANGLELIGHT)
	return TriangleLight_GetRadiance(light, hitPoint, directPdfA
			MATERIALS_PARAM);
#else
	return 0.f;
#endif
}

OPENCL_FORCE_NOT_INLINE float3 Light_Illuminate(
		__global const LightSource *light,
		__global const BSDF *bsdf,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float envRadius,
		__global HitPoint *tmpHitPoint,
		float3 *lightRayDir, float *distance, float *directPdfW
		LIGHTS_PARAM_DECL) {
	switch (light->type) {
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)
		case TYPE_IL_CONSTANT:
			return ConstantInfiniteLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, u0, u1,
					lightRayDir, distance, directPdfW
					LIGHTS_PARAM);
#endif
#if defined(PARAM_HAS_INFINITELIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_IL:
			return InfiniteLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, u0, u1,
					lightRayDir, distance, directPdfW
					LIGHTS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT2)
		case TYPE_IL_SKY2:
			return SkyLight2_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, u0, u1,
					lightRayDir, distance, directPdfW
					LIGHTS_PARAM);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
			return SunLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, u0, u1,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_TRIANGLELIGHT)
		case TYPE_TRIANGLE:
			return TriangleLight_Illuminate(
					light,
					tmpHitPoint,
					bsdf, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					lightRayDir, distance, directPdfW
					MATERIALS_PARAM);
#endif
#if defined(PARAM_HAS_POINTLIGHT)
		case TYPE_POINT:
			return PointLight_Illuminate(
					light,
					bsdf,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_MAPPOINTLIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_MAPPOINT:
			return MapPointLight_Illuminate(
					light,
					bsdf,
					lightRayDir, distance, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SPOTLIGHT)
		case TYPE_SPOT:
			return SpotLight_Illuminate(
					light,
					bsdf,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_PROJECTIONLIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_PROJECTION:
			return ProjectionLight_Illuminate(
					light,
					bsdf,
					lightRayDir, distance, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SHARPDISTANTLIGHT)
		case TYPE_SHARPDISTANT:
			return SharpDistantLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_DISTANTLIGHT)
		case TYPE_DISTANT:
			return DistantLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, u0, u1,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_LASERLIGHT)
		case TYPE_LASER:
			return LaserLight_Illuminate(
					light,
					bsdf,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_SPHERELIGHT)
		case TYPE_SPHERE:
			return SphereLight_Illuminate(
					light,
					bsdf,
					u0, u1, lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_MAPSPHERELIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_MAPSPHERE:
			return MapSphereLight_Illuminate(
					light,
					bsdf,
					u0, u1, lightRayDir, distance, directPdfW
					IMAGEMAPS_PARAM);
#endif
		default:
			return BLACK;
	}
}

OPENCL_FORCE_INLINE bool Light_IsEnvOrIntersectable(__global const LightSource *light) {
	switch (light->type) {
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)
		case TYPE_IL_CONSTANT:
#endif
#if defined(PARAM_HAS_INFINITELIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_IL:
#endif
#if defined(PARAM_HAS_SKYLIGHT2)
		case TYPE_IL_SKY2:
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
#endif
#if defined(PARAM_HAS_TRIANGLELIGHT)
		case TYPE_TRIANGLE:
#endif
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT) || (defined(PARAM_HAS_INFINITELIGHT) && defined(PARAM_HAS_IMAGEMAPS)) || defined(PARAM_HAS_SKYLIGHT2) || defined(PARAM_HAS_SUNLIGHT) || defined(PARAM_HAS_TRIANGLELIGHT)
			return true;
#endif

#if defined(PARAM_HAS_SPHERELIGHT)
		case TYPE_SPHERE:
#endif
#if defined(PARAM_HAS_MAPSPHERELIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_MAPSPHERE:
#endif
#if defined(PARAM_HAS_POINTLIGHT)
		case TYPE_POINT:
#endif
#if defined(PARAM_HAS_MAPPOINTLIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_MAPPOINT:
#endif
#if defined(PARAM_HAS_SPOTLIGHT)
		case TYPE_SPOT:
#endif
#if defined(PARAM_HAS_PROJECTIONLIGHT) && defined(PARAM_HAS_IMAGEMAPS)
		case TYPE_PROJECTION:
#endif
#if defined(PARAM_HAS_SHARPDISTANTLIGHT)
		case TYPE_SHARPDISTANT:
#endif
#if defined(PARAM_HAS_DISTANTLIGHT)
		case TYPE_DISTANT:
#endif
#if defined(PARAM_HAS_LASERLIGHT)
		case TYPE_LASER:
#endif
#if defined(PARAM_HAS_POINTLIGHT) || (defined(PARAM_HAS_MAPPOINTLIGHT) && defined(PARAM_HAS_IMAGEMAPS)) || defined(PARAM_HAS_SPOTLIGHT) || (defined(PARAM_HAS_PROJECTIONLIGHT) && defined(PARAM_HAS_IMAGEMAPS)) || defined(PARAM_HAS_SHARPDISTANTLIGHT) || defined(PARAM_HAS_DISTANTLIGHT) || defined(PARAM_HAS_LASERLIGHT) || (defined(PARAM_HAS_MAPSPHERELIGHT) && defined(PARAM_HAS_IMAGEMAPS))
			return false;
#endif

		default:
			return false;
	}
}

OPENCL_FORCE_INLINE float Light_GetAvgPassThroughTransparency(
		__global const LightSource *light
		LIGHTS_PARAM_DECL) {
#if defined(PARAM_HAS_TRIANGLELIGHT)
	return (light->type == TYPE_TRIANGLE) ? mats[light->triangle.materialIndex].avgPassThroughTransparency : 0.f;
#else
	return 1.f;
#endif
}
