#line 2 "light_funcs.cl"

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

OPENCL_FORCE_INLINE float EnvLightSource_GetEnvRadius(const float sceneRadius) {
	// This is used to scale the world radius in sun/sky/infinite lights in order to
	// avoid problems with objects that are near the borderline of the world bounding sphere
	return LIGHT_WORLD_RADIUS_SCALE * sceneRadius;
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

OPENCL_FORCE_INLINE float3 ConstantInfiniteLight_GetRadiance(__global const LightSource *constantInfiniteLight,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const bool useVisibilityMapCache = constantInfiniteLight->notIntersectable.constantInfinite.useVisibilityMapCache;

	if (useVisibilityMapCache && (!bsdf || EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM))) {
		const float3 localDir = normalize(Transform_InvApplyVector(&constantInfiniteLight->notIntersectable.light2World, -dir));

		float u, v, latLongMappingPdf;
		EnvLightSource_ToLatLongMapping(localDir, &u, &v, &latLongMappingPdf);
		if (latLongMappingPdf == 0.f)
			return BLACK;

		if (bsdf)
			*directPdfA = EnvLightVisibilityCache_Pdf(bsdf, u, v LIGHTS_PARAM) * latLongMappingPdf;
		else
			*directPdfA = 0.f;
	} else
		*directPdfA = bsdf ? UniformSpherePdf() : 0.f;

	return VLOAD3F(constantInfiniteLight->notIntersectable.temperatureScale.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.gain.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.constantInfinite.color.c);
}

OPENCL_FORCE_INLINE float3 ConstantInfiniteLight_Illuminate(__global const LightSource *constantInfiniteLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW
		LIGHTS_PARAM_DECL) {
	const bool useVisibilityMapCache = constantInfiniteLight->notIntersectable.constantInfinite.useVisibilityMapCache;

	float3 shadowRayDir;
	if (useVisibilityMapCache && EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		float2 sampleUV;
		float distPdf;

		EnvLightVisibilityCache_Sample(bsdf, u0, u1, &sampleUV, &distPdf LIGHTS_PARAM);
		if (distPdf == 0.f)
			return BLACK;

		float latLongMappingPdf;
		EnvLightSource_FromLatLongMapping(sampleUV.x, sampleUV.y, &shadowRayDir, &latLongMappingPdf);
		if (latLongMappingPdf == 0.f)
			return BLACK;

		*directPdfW = distPdf * latLongMappingPdf;
	} else {
		shadowRayDir = UniformSampleSphere(u0, u1);

		*directPdfW = UniformSpherePdf();
	}

	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	const float3 pSurface = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	const float3 toCenter = worldCenter - pSurface;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = pSurface + shadowRayDistance * shadowRayDir;
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -shadowRayDir);
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(constantInfiniteLight->notIntersectable.temperatureScale.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.gain.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.constantInfinite.color.c);
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 InfiniteLight_GetRadiance(__global const LightSource *infiniteLight,
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
		*directPdfA = EnvLightVisibilityCache_Pdf(bsdf, u, v LIGHTS_PARAM) * latLongMappingPdf;
	} else {
		__global const float *infiniteLightDist = &envLightDistribution[infiniteLight->notIntersectable.infinite.illuminateDistributionOffset];

		const float distPdf = Distribution2D_Pdf(infiniteLightDist, u, v);
		*directPdfA = distPdf * latLongMappingPdf;
	}

	__global const ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersectable.infinite.imageMapIndex];
	return VLOAD3F(infiniteLight->notIntersectable.temperatureScale.c) *
			VLOAD3F(infiniteLight->notIntersectable.gain.c) *
			ImageMap_GetSpectrum(
				imageMap,
				u, v
				IMAGEMAPS_PARAM);
}

OPENCL_FORCE_INLINE float3 InfiniteLight_Illuminate(__global const LightSource *infiniteLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW
		LIGHTS_PARAM_DECL) {
	float2 sampleUV;
	float distPdf;
	if (infiniteLight->notIntersectable.infinite.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		EnvLightVisibilityCache_Sample(bsdf, u0, u1, &sampleUV, &distPdf LIGHTS_PARAM);
	} else {
		__global const float *infiniteLightDistribution = &envLightDistribution[infiniteLight->notIntersectable.infinite.illuminateDistributionOffset];

		Distribution2D_SampleContinuous(infiniteLightDistribution, u0, u1, &sampleUV, &distPdf);
	}

	if (distPdf == 0.f)
		return BLACK;

	float3 localDir;
	float latLongMappingPdf;
	EnvLightSource_FromLatLongMapping(sampleUV.x, sampleUV.y, &localDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	const float3 shadowRayDir = normalize(Transform_ApplyVector(&infiniteLight->notIntersectable.light2World, localDir));

	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	const float3 pSurface = BSDF_GetRayOrigin(bsdf, worldCenter - VLOAD3F(&bsdf->hitPoint.p.x));
	const float3 toCenter = worldCenter - pSurface;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = pSurface + shadowRayDistance * shadowRayDir;
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -shadowRayDir);
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*directPdfW = distPdf * latLongMappingPdf;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	// InfiniteLight_GetRadiance is expended here
	__global const ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersectable.infinite.imageMapIndex];

	const float2 uv = MAKE_FLOAT2(sampleUV.x, sampleUV.y);

	return VLOAD3F(infiniteLight->notIntersectable.temperatureScale.c) *
			VLOAD3F(infiniteLight->notIntersectable.gain.c) *
			ImageMap_GetSpectrum(
				imageMap,
				uv.x, uv.y
				IMAGEMAPS_PARAM);
}

//------------------------------------------------------------------------------
// Sky2Light
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float RiCosBetween(const float3 w1, const float3 w2) {
	return clamp(dot(w1, w2), -1.f, 1.f);
}

OPENCL_FORCE_INLINE float3 Sky2Light_ComputeSkyRadiance(__global const LightSource *sky2Light, const float3 w) {
	const float3 absoluteSunDir = VLOAD3F(&sky2Light->notIntersectable.sky2.absoluteSunDir.x);
	const float cosG = RiCosBetween(w, absoluteSunDir);
	const float cosG2 = cosG * cosG;
	const float gamma = acos(cosG);
	const float cosT = fmax(0.f, CosTheta(w));

	const float3 aTerm = VLOAD3F(sky2Light->notIntersectable.sky2.aTerm.c);
	const float3 bTerm = VLOAD3F(sky2Light->notIntersectable.sky2.bTerm.c);
	const float3 cTerm = VLOAD3F(sky2Light->notIntersectable.sky2.cTerm.c);
	const float3 dTerm = VLOAD3F(sky2Light->notIntersectable.sky2.dTerm.c);
	const float3 eTerm = VLOAD3F(sky2Light->notIntersectable.sky2.eTerm.c);
	const float3 fTerm = VLOAD3F(sky2Light->notIntersectable.sky2.fTerm.c);
	const float3 gTerm = VLOAD3F(sky2Light->notIntersectable.sky2.gTerm.c);
	const float3 hTerm = VLOAD3F(sky2Light->notIntersectable.sky2.hTerm.c);
	const float3 iTerm = VLOAD3F(sky2Light->notIntersectable.sky2.iTerm.c);
	const float3 radianceTerm = VLOAD3F(sky2Light->notIntersectable.sky2.radianceTerm.c);
	
	const float3 expTerm = dTerm * Spectrum_Exp(eTerm * gamma);
	const float3 rayleighTerm = fTerm * cosG2;
	const float3 mieTerm = gTerm * (1.f + cosG2) /
		Spectrum_Pow(1.f + iTerm * (iTerm - (2.f * cosG)), 1.5f);
	const float3 zenithTerm = hTerm * sqrt(cosT);

	// 683 is a scaling factor to convert W to lm
	return 683.f * (1.f + aTerm * Spectrum_Exp(bTerm / (cosT + .01f))) *
		(cTerm + expTerm + rayleighTerm + mieTerm + zenithTerm) * radianceTerm;
}

OPENCL_FORCE_INLINE float3 Sky2Light_ComputeRadiance(__global const LightSource *sky2Light, const float3 w) {
	if (sky2Light->notIntersectable.sky2.hasGround &&
			(dot(w, VLOAD3F(&sky2Light->notIntersectable.sky2.absoluteUpDir.x)) < 0.f)) {
		// Lower hemisphere
		return VLOAD3F(sky2Light->notIntersectable.sky2.scaledGroundColor.c);
	} else
		return VLOAD3F(sky2Light->notIntersectable.temperatureScale.c) *
				VLOAD3F(sky2Light->notIntersectable.gain.c) *
				Sky2Light_ComputeSkyRadiance(sky2Light, w);
}

OPENCL_FORCE_INLINE float3 Sky2Light_GetRadiance(__global const LightSource *sky2Light,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const float3 w = -dir;
	float u, v, latLongMappingPdf;
	EnvLightSource_ToLatLongMapping(w, &u, &v, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	if (!bsdf)
		*directPdfA = 0.f;
	else if (sky2Light->notIntersectable.sky2.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		*directPdfA = EnvLightVisibilityCache_Pdf(bsdf, u, v LIGHTS_PARAM) * latLongMappingPdf;
	} else {
		__global const float *skyLightDist = &envLightDistribution[sky2Light->notIntersectable.sky2.distributionOffset];

		const float distPdf = Distribution2D_Pdf(skyLightDist, u, v);
		*directPdfA = distPdf * latLongMappingPdf;
	}

	return Sky2Light_ComputeRadiance(sky2Light, w);
}

OPENCL_FORCE_INLINE float3 Sky2Light_Illuminate(__global const LightSource *sky2Light,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW
		LIGHTS_PARAM_DECL) {
	float2 sampleUV;
	float distPdf;
	if (sky2Light->notIntersectable.sky2.useVisibilityMapCache &&
			EnvLightVisibilityCache_IsCacheEnabled(bsdf MATERIALS_PARAM)) {
		EnvLightVisibilityCache_Sample(bsdf, u0, u1, &sampleUV, &distPdf LIGHTS_PARAM);
	} else {
		__global const float *skyLightDistribution = &envLightDistribution[sky2Light->notIntersectable.sky2.distributionOffset];

		Distribution2D_SampleContinuous(skyLightDistribution, u0, u1, &sampleUV, &distPdf);
	}

	if (distPdf == 0.f)
			return BLACK;

	float3 shadowRayDir;
	float latLongMappingPdf;
	EnvLightSource_FromLatLongMapping(sampleUV.x, sampleUV.y, &shadowRayDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return BLACK;

	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	const float3 pSurface = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	const float3 toCenter = worldCenter - pSurface;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = pSurface + shadowRayDistance * shadowRayDir;
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -shadowRayDir);
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*directPdfW = distPdf * latLongMappingPdf;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return Sky2Light_ComputeRadiance(sky2Light, shadowRayDir);
}

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 SunLight_GetRadiance(__global const LightSource *sunLight,
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

OPENCL_FORCE_INLINE float3 SunLight_Illuminate(__global const LightSource *sunLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW) {

	const float cosThetaMax = sunLight->notIntersectable.sun.cosThetaMax;
	const float3 sunDir = VLOAD3F(&sunLight->notIntersectable.sun.absoluteDir.x);
	const float3 shadowRayDir = UniformSampleCone(u0, u1, cosThetaMax, VLOAD3F(&sunLight->notIntersectable.sun.x.x), VLOAD3F(&sunLight->notIntersectable.sun.y.x), sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = dot(sunDir, shadowRayDir);
	if (cosAtLight <= cosThetaMax)
		return BLACK;

	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);
	
	const float3 pSurface = BSDF_GetRayOrigin(bsdf, worldCenter - VLOAD3F(&bsdf->hitPoint.p.x));
	const float3 toCenter = worldCenter - pSurface;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	*directPdfW = UniformConePdf(cosThetaMax);

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(sunLight->notIntersectable.sun.color.c);
}

//------------------------------------------------------------------------------
// TriangleLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 TriangleLight_GetRadiance(__global const LightSource *triLight,
		 __global const HitPoint *hitPoint, float *directPdfA
		MATERIALS_PARAM_DECL) {
	const uint materialIndex = sceneObjs[triLight->triangle.meshIndex].materialIndex;

	const float3 dir = VLOAD3F(&hitPoint->fixedDir.x);
	const float3 hitPointNormal = VLOAD3F(&hitPoint->shadeN.x);
	const float cosOutLight = dot(hitPointNormal, dir);
	const float cosThetaMax = Material_GetEmittedCosThetaMax(materialIndex
			MATERIALS_PARAM);
	// emissionFunc can emit light even backward, this is for compatibility with classic Lux
	if (((triLight->triangle.imageMapIndex == NULL_INDEX) && (cosOutLight < cosThetaMax + DEFAULT_COS_EPSILON_STATIC)) ||
			// A safety check to avoid NaN/Inf
			(triLight->triangle.invTriangleArea == 0.f) || (triLight->triangle.invMeshArea == 0.f))
		return BLACK;

	if (directPdfA)
		*directPdfA = triLight->triangle.invTriangleArea;

	float3 emissionColor = WHITE;

	if (triLight->triangle.imageMapIndex != NULL_INDEX) {
		// Build the local frame
		Frame frame;
		HitPoint_GetFrame(hitPoint, &frame);

		const float3 localFromLight = normalize(Frame_ToLocal_Private(&frame, dir));

		// Retrieve the image map information
		__global const ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		const float2 uv = MAKE_FLOAT2(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
		emissionColor = ImageMap_GetSpectrum(
				imageMap,
				uv.x, uv.y
				IMAGEMAPS_PARAM) / triLight->triangle.average;
	}

	return Material_GetEmittedRadiance(materialIndex,
			hitPoint, triLight->triangle.invMeshArea
			MATERIALS_PARAM) * emissionColor;
}

OPENCL_FORCE_INLINE float3 TriangleLight_Illuminate(__global const LightSource *triLight,
		__global HitPoint *tmpHitPoint, __global const BSDF *bsdf,
		const float time, const float u0, const float u1,
		const float passThroughEvent,
		__global Ray *shadowRay, float *directPdfW
		MATERIALS_PARAM_DECL) {
	// A safety check to avoid NaN/Inf
	if ((triLight->triangle.invTriangleArea == 0.f) || (triLight->triangle.invMeshArea == 0.f))
		return BLACK;

	//--------------------------------------------------------------------------
	// Compute the sample point and direction
	//--------------------------------------------------------------------------

	const uint meshIndex = triLight->triangle.meshIndex;
	const uint triangleIndex = triLight->triangle.triangleIndex;
	const uint materialIndex = sceneObjs[meshIndex].materialIndex;

	// Initialized local to world object space transformation
	Transform localToWorld;
	ExtMesh_GetLocal2World(time, meshIndex, triangleIndex, &localToWorld EXTMESH_PARAM);

	float b0, b1, b2;
	float3 samplePoint;
	ExtMesh_Sample(&localToWorld,
			meshIndex, triangleIndex,
			u0, u1,
			&samplePoint, &b0, &b1, &b2
			EXTMESH_PARAM);

	float3 sampleDir = samplePoint - VLOAD3F(&bsdf->hitPoint.p.x);
	const float distanceSquared = dot(sampleDir, sampleDir);
	const float distance = sqrt(distanceSquared);
	sampleDir /= distance;
	
	// Build a temporary HitPoint
	HitPoint_Init(tmpHitPoint, false,
		meshIndex, triangleIndex,
		samplePoint, -sampleDir,
		b1, b2,
		passThroughEvent,
		&localToWorld, time
		MATERIALS_PARAM);
	// Add bump?
		// lightMaterial->Bump(&hitPoint, 1.f);

	const float3 geometryN = VLOAD3F(&tmpHitPoint->geometryN.x);
	const float cosAtLight = dot(geometryN, -sampleDir);
	const float cosThetaMax = Material_GetEmittedCosThetaMax(materialIndex
		MATERIALS_PARAM);
	// emissionFunc can emit light even backward, this is for compatibility with classic Lux
	if ((triLight->triangle.imageMapIndex == NULL_INDEX) && (cosAtLight < cosThetaMax + DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	//--------------------------------------------------------------------------
	// Initialize the shadow ray
	//--------------------------------------------------------------------------
	
	// Move shadow ray origin along the geometry normal by an epsilon to avoid self-shadow problems
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, sampleDir);

	// Compute shadow ray direction with displaced start and end point to avoid self-shadow problems
	float3 shadowRayDir = samplePoint + geometryN * MachineEpsilon_E_Float3(samplePoint) *
			(tmpHitPoint->intoObject ? 1.f : -1.f) - shadowRayOrig;
	const float shadowRayDistanceSquared = dot(shadowRayDir, shadowRayDir);
	const float shadowRayDistance = sqrt(shadowRayDistanceSquared);
	shadowRayDir /= shadowRayDistance;

	//--------------------------------------------------------------------------
	// Compute the PDF and emission color
	//--------------------------------------------------------------------------
	
	float3 emissionColor = WHITE;

	if (triLight->triangle.imageMapIndex != NULL_INDEX) {
		// Build the local frame
		Frame frame;
		HitPoint_GetFrame(tmpHitPoint, &frame);

		const float3 localFromLight = normalize(Frame_ToLocal_Private(&frame, -shadowRayDir));

		// Retrieve the image map information
		__global const ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		const float2 uv = MAKE_FLOAT2(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
		emissionColor = ImageMap_GetSpectrum(
				imageMap,
				uv.x, uv.y
				IMAGEMAPS_PARAM) / triLight->triangle.average;

		*directPdfW = triLight->triangle.invTriangleArea * shadowRayDistanceSquared ;
	} else
		*directPdfW = triLight->triangle.invTriangleArea * shadowRayDistanceSquared / fabs(cosAtLight);

	// Setup the shadow ray
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return Material_GetEmittedRadiance(materialIndex,
			tmpHitPoint, triLight->triangle.invMeshArea
			MATERIALS_PARAM) * emissionColor;
}

//------------------------------------------------------------------------------
// PointLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 PointLight_Illuminate(__global const LightSource *pointLight,
		__global const BSDF *bsdf, const float time,
		__global Ray *shadowRay, float *directPdfW) {
	const float3 pLight = VLOAD3F(&pointLight->notIntersectable.point.absolutePos.x);	
	const float3 pSurface = BSDF_GetRayOrigin(bsdf, pLight - VLOAD3F(&bsdf->hitPoint.p.x));

	const float3 toLight = pLight - pSurface;
	const float shadowRayDistanceSquared = dot(toLight, toLight);
	const float shadowRayDistance = sqrt(shadowRayDistanceSquared);
	const float3 shadowRayDir = toLight / shadowRayDistance;

	*directPdfW = shadowRayDistanceSquared;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(pointLight->notIntersectable.point.emittedFactor.c) * (1.f / (4.f * M_PI_F));
}

//------------------------------------------------------------------------------
// SphereLight
//------------------------------------------------------------------------------

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

OPENCL_FORCE_INLINE float3 SphereLight_Illuminate(__global const LightSource *sphereLight,
		__global const BSDF *bsdf, const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW) {
	const float3 absolutePos = VLOAD3F(&sphereLight->notIntersectable.sphere.absolutePos.x);
	const float3 toLight = absolutePos - VLOAD3F(&bsdf->hitPoint.p.x);
	const float centerDistanceSquared = dot(toLight, toLight);
	const float centerDistance = sqrt(centerDistanceSquared);

	const float radius = sphereLight->notIntersectable.sphere.radius;
	const float radiusSquared = radius * radius;

	// Check if the point is inside the sphere
	if (centerDistanceSquared - radiusSquared < DEFAULT_EPSILON_STATIC) {
		// The point is inside the sphere, return black
		return BLACK;
	}

	// The point isn't inside the sphere

	const float cosThetaMax = sqrt(max(0.f, 1.f - radiusSquared / centerDistanceSquared));

	// Build a local coordinate system
	const float3 localZ = toLight * (1.f / centerDistance);
	Frame localFrame;
	Frame_SetFromZ_Private(&localFrame, localZ);

	// Sample sphere uniformly inside subtended cone
	const float3 localShadowRayDir = UniformSampleConeLocal(u0, u1, cosThetaMax);
	if (CosTheta(localShadowRayDir) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 shadowRayDir = Frame_ToWorld_Private(&localFrame, localShadowRayDir);

	// Check the intersection with the sphere
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	float shadowRayDistance;
	if (!SphereLight_SphereIntersect(absolutePos, radiusSquared, shadowRayOrig, shadowRayDir, &shadowRayDistance))
		shadowRayDistance = dot(toLight, shadowRayDir);

	if (cosThetaMax > 1.f - DEFAULT_EPSILON_STATIC) {
		// If the subtended angle is too small, I replace the computation for
		// directPdfW  and return factor with that of a point light source in
		// order to avoiding banding due to (lack of) numerical precision.
		*directPdfW = shadowRayDistance * shadowRayDistance;

		// Setup the shadow ray
		Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

		return VLOAD3F(sphereLight->notIntersectable.sphere.emittedFactor.c) * (1.f / (4.f * M_PI_F));
	} else {
		*directPdfW = UniformConePdf(cosThetaMax);

		// Setup the shadow ray
		Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

		const float invArea = 1.f / (4.f * M_PI_F * radiusSquared);

		return VLOAD3F(sphereLight->notIntersectable.sphere.emittedFactor.c) * invArea * M_1_PI_F;
	}
}

//------------------------------------------------------------------------------
// MapPointLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 MapPointLight_Illuminate(__global const LightSource *mapPointLight,
		__global const BSDF *bsdf, const float time,
		__global Ray *shadowRay, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 pLight = VLOAD3F(&mapPointLight->notIntersectable.mapPoint.absolutePos.x);
	const float3 toLight = pLight - VLOAD3F(&bsdf->hitPoint.p.x);
	const float shadowRayDistanceSquared = dot(toLight, toLight);
	const float shadowRayDistance = sqrt(shadowRayDistanceSquared);
	const float3 shadowRayDir = toLight / shadowRayDistance;

	*directPdfW = shadowRayDistanceSquared;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	// Retrieve the image map information
	__global const ImageMap *imageMap = &imageMapDescs[mapPointLight->notIntersectable.mapPoint.imageMapIndex];

	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&mapPointLight->notIntersectable.light2World, -shadowRayDir));
	const float2 uv = MAKE_FLOAT2(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
	const float3 emissionColor = ImageMap_GetSpectrum(
			imageMap,
			uv.x, uv.y
			IMAGEMAPS_PARAM) / (4.f * M_PI_F * mapPointLight->notIntersectable.mapPoint.average);

	return VLOAD3F(mapPointLight->notIntersectable.mapPoint.emittedFactor.c) * emissionColor;
}

//------------------------------------------------------------------------------
// MapSphereLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 MapSphereLight_Illuminate(__global const LightSource *mapSphereLight,
		__global const BSDF *bsdf,	const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 result = SphereLight_Illuminate(mapSphereLight, bsdf, time, u0, u1,
			shadowRay, directPdfW);

	// Retrieve the image map information
	__global const ImageMap *imageMap = &imageMapDescs[mapSphereLight->notIntersectable.mapSphere.imageMapIndex];

	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&mapSphereLight->notIntersectable.light2World, -VLOAD3F(&shadowRay->d.x)));
	const float2 uv = MAKE_FLOAT2(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
	const float3 emissionColor = ImageMap_GetSpectrum(
			imageMap,
			uv.x, uv.y
			IMAGEMAPS_PARAM) * (1.f / mapSphereLight->notIntersectable.mapSphere.average);

	return result * emissionColor;
}

//------------------------------------------------------------------------------
// SpotLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float SpotLight_LocalFalloff(const float3 w, const float cosTotalWidth, const float cosFalloffStart) {
	if (CosTheta(w) < cosTotalWidth)
		return 0.f;
 	if (CosTheta(w) > cosFalloffStart)
		return 1.f;

	// Compute falloff inside spotlight cone
	const float delta = (CosTheta(w) - cosTotalWidth) / (cosFalloffStart - cosTotalWidth);
	return pow(delta, 4.f);
}

OPENCL_FORCE_INLINE float3 SpotLight_Illuminate(__global const LightSource *spotLight,
		__global const BSDF *bsdf, const float time,
		__global Ray *shadowRay, float *directPdfW) {
	const float3 pLight = VLOAD3F(&spotLight->notIntersectable.spot.absolutePos.x);
	const float3 pSurface = BSDF_GetRayOrigin(bsdf, pLight - VLOAD3F(&bsdf->hitPoint.p.x));

	const float3 toLight = pLight - pSurface;
	const float shadowRayDistanceSquared = dot(toLight, toLight);
	const float shadowRayDistance = sqrt(shadowRayDistanceSquared);
	const float3 shadowRayDir = toLight / shadowRayDistance;

	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&spotLight->notIntersectable.light2World, -shadowRayDir));
	const float falloff = SpotLight_LocalFalloff(localFromLight,
			spotLight->notIntersectable.spot.cosTotalWidth,
			spotLight->notIntersectable.spot.cosFalloffStart);
	if (falloff == 0.f)
		return BLACK;

	*directPdfW = shadowRayDistanceSquared;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(spotLight->notIntersectable.spot.emittedFactor.c) *
			(falloff / fabs(CosTheta(localFromLight)));
}

//------------------------------------------------------------------------------
// ProjectionLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ProjectionLight_Illuminate(__global const LightSource *projectionLight,
		__global const BSDF *bsdf, const float time,
		__global Ray *shadowRay, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 pLight = VLOAD3F(&projectionLight->notIntersectable.projection.absolutePos.x);
	const float3 pSurface = BSDF_GetRayOrigin(bsdf, pLight - VLOAD3F(&bsdf->hitPoint.p.x));

	const float3 toLight = pLight - pSurface;
	const float shadowRayDistanceSquared = dot(toLight, toLight);
	const float shadowRayDistance = sqrt(shadowRayDistanceSquared);
	const float3 shadowRayDir = toLight / shadowRayDistance;

	// Check the side
	if (dot(-shadowRayDir, VLOAD3F(&projectionLight->notIntersectable.projection.lightNormal.x)) < 0.f)
		return BLACK;

	// Check if the point is inside the image plane
	const float3 localFromLight = normalize(Transform_InvApplyVector(
			&projectionLight->notIntersectable.light2World, -shadowRayDir));
	const float3 p0 = Matrix4x4_ApplyPoint(
			&projectionLight->notIntersectable.projection.lightProjection, localFromLight);

	const float screenX0 = projectionLight->notIntersectable.projection.screenX0;
	const float screenX1 = projectionLight->notIntersectable.projection.screenX1;
	const float screenY0 = projectionLight->notIntersectable.projection.screenY0;
	const float screenY1 = projectionLight->notIntersectable.projection.screenY1;
	if ((p0.x < screenX0) || (p0.x >= screenX1) || (p0.y < screenY0) || (p0.y >= screenY1))
		return BLACK;

	*directPdfW = shadowRayDistanceSquared;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	float3 c = VLOAD3F(projectionLight->notIntersectable.projection.emittedFactor.c);

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

	return c;
}

//------------------------------------------------------------------------------
// SharpDistantLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 SharpDistantLight_Illuminate(__global const LightSource *sharpDistantLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float time,
		__global Ray *shadowRay, float *directPdfW) {
	const float3 shadowRayDir = -VLOAD3F(&sharpDistantLight->notIntersectable.sharpDistant.absoluteLightDir.x);

	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);
	
	const float3 pSurface = BSDF_GetRayOrigin(bsdf, worldCenter - VLOAD3F(&bsdf->hitPoint.p.x));
	const float3 toCenter = worldCenter - pSurface;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	*directPdfW = 1.f;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(sharpDistantLight->notIntersectable.temperatureScale.c) *
			VLOAD3F(sharpDistantLight->notIntersectable.gain.c) *
			VLOAD3F(sharpDistantLight->notIntersectable.sharpDistant.color.c);
}

//------------------------------------------------------------------------------
// DistantLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DistantLight_Illuminate(__global const LightSource *distantLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global const BSDF *bsdf, const float time, const float u0, const float u1,
		__global Ray *shadowRay, float *directPdfW) {
	const float3 absoluteLightDir = VLOAD3F(&distantLight->notIntersectable.distant.absoluteLightDir.x);
	const float3 x = VLOAD3F(&distantLight->notIntersectable.distant.x.x);
	const float3 y = VLOAD3F(&distantLight->notIntersectable.distant.y.x);
	const float cosThetaMax = distantLight->notIntersectable.distant.cosThetaMax;
	const float3 shadowRayDir = -UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);

	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);
	
	const float3 pSurface = BSDF_GetRayOrigin(bsdf, worldCenter - VLOAD3F(&bsdf->hitPoint.p.x));
	const float3 toCenter = worldCenter - pSurface;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	*directPdfW = uniformConePdf;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(distantLight->notIntersectable.temperatureScale.c) *
			VLOAD3F(distantLight->notIntersectable.gain.c) *
			VLOAD3F(distantLight->notIntersectable.sharpDistant.color.c);
}

//------------------------------------------------------------------------------
// LaserLight
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 LaserLight_Illuminate(__global const LightSource *laserLight,
		__global const BSDF *bsdf, const float time,
		__global Ray *shadowRay, float *directPdfW) {
	const float3 absoluteLightPos = VLOAD3F(&laserLight->notIntersectable.laser.absoluteLightPos.x);
	const float3 absoluteLightDir = VLOAD3F(&laserLight->notIntersectable.laser.absoluteLightDir.x);

	const float3 shadowRayDir = -absoluteLightDir;
	const float3 rayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	const float3 planeCenter = absoluteLightPos;
	const float3 planeNormal = absoluteLightDir;

	// Intersect the shadow ray with light plane
	const float denom = dot(planeNormal, shadowRayDir);
	const float3 pr = planeCenter - rayOrig;
	float shadowRayDistance = dot(pr, planeNormal);

	if (fabs(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		shadowRayDistance /= denom; 

		if ((shadowRayDistance <= 0.f) || (denom >= 0.f))
			return BLACK;
	} else
		return BLACK;

	const float3 lightPoint = rayOrig + shadowRayDistance * shadowRayDir;

	// Check if the point is inside the emitting circle
	const float radius = laserLight->notIntersectable.laser.radius;
	const float3 dist = lightPoint - absoluteLightPos;
	if (dot(dist, dist) > radius * radius)
		return BLACK;
	
	// Ok, the light is visible
	
	*directPdfW = 1.f;

	// Setup the shadow ray
	const float3 shadowRayOrig = BSDF_GetRayOrigin(bsdf, shadowRayDir);
	Ray_Init4(shadowRay, shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return VLOAD3F(laserLight->notIntersectable.laser.emittedFactor.c);
}

//------------------------------------------------------------------------------
// Generic light functions
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 EnvLight_GetRadiance(__global const LightSource *light,
		__global const BSDF *bsdf, const float3 dir, float *directPdfA
		LIGHTS_PARAM_DECL) {
	switch (light->type) {
		case TYPE_IL_CONSTANT:
			return ConstantInfiniteLight_GetRadiance(light,
					bsdf,
					dir, directPdfA
					LIGHTS_PARAM);
		case TYPE_IL:
			return InfiniteLight_GetRadiance(light,
					bsdf,
					dir, directPdfA
					LIGHTS_PARAM);
		case TYPE_IL_SKY2:
			return Sky2Light_GetRadiance(light,
					bsdf,
					dir, directPdfA
					LIGHTS_PARAM);
		case TYPE_SUN:
			return SunLight_GetRadiance(light,
					bsdf,
					dir, directPdfA);
		case TYPE_SHARPDISTANT:
			// Just return Black
		case TYPE_DISTANT:
			// Just return Black
		default:
			return BLACK;
	}
}

OPENCL_FORCE_INLINE float3 IntersectableLight_GetRadiance(__global const LightSource *light,
		 __global const HitPoint *hitPoint, float *directPdfA
		LIGHTS_PARAM_DECL) {
	return TriangleLight_GetRadiance(light, hitPoint, directPdfA
			MATERIALS_PARAM);
}

// Cuda reports large argument size, so overrides noinline attribute anyway
OPENCL_FORCE_INLINE float3 Light_Illuminate(
		__global const LightSource *light,
		__global const BSDF *bsdf,
		const float time, const float u0, const float u1,
		const float passThroughEvent,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float envRadius,
		__global HitPoint *tmpHitPoint,
		__global Ray *shadowRay, float *directPdfW
		LIGHTS_PARAM_DECL) {
	switch (light->type) {
		case TYPE_IL_CONSTANT:
			return ConstantInfiniteLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, time, u0, u1,
					shadowRay, directPdfW
					LIGHTS_PARAM);
		case TYPE_IL:
			return InfiniteLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, time, u0, u1,
					shadowRay, directPdfW
					LIGHTS_PARAM);
		case TYPE_IL_SKY2:
			return Sky2Light_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, time, u0, u1,
					shadowRay, directPdfW
					LIGHTS_PARAM);
		case TYPE_SUN:
			return SunLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, time, u0, u1,
					shadowRay, directPdfW);
		case TYPE_TRIANGLE:
			return TriangleLight_Illuminate(
					light,
					tmpHitPoint,
					bsdf, time ,u0, u1,
					passThroughEvent,
					shadowRay, directPdfW
					MATERIALS_PARAM);
		case TYPE_POINT:
			return PointLight_Illuminate(
					light,
					bsdf, time,
					shadowRay, directPdfW);
		case TYPE_MAPPOINT:
			return MapPointLight_Illuminate(
					light,
					bsdf, time,
					shadowRay, directPdfW
					IMAGEMAPS_PARAM);
		case TYPE_SPOT:
			return SpotLight_Illuminate(
					light,
					bsdf, time,
					shadowRay, directPdfW);
		case TYPE_PROJECTION:
			return ProjectionLight_Illuminate(
					light,
					bsdf, time,
					shadowRay, directPdfW
					IMAGEMAPS_PARAM);
		case TYPE_SHARPDISTANT:
			return SharpDistantLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, time,
					shadowRay, directPdfW);
		case TYPE_DISTANT:
			return DistantLight_Illuminate(
					light,
					worldCenterX, worldCenterY, worldCenterZ, envRadius,
					bsdf, time, u0, u1,
					shadowRay, directPdfW);
		case TYPE_LASER:
			return LaserLight_Illuminate(
					light,
					bsdf, time,
					shadowRay, directPdfW);
		case TYPE_SPHERE:
			return SphereLight_Illuminate(
					light,
					bsdf, time, u0, u1,
					shadowRay, directPdfW);
		case TYPE_MAPSPHERE:
			return MapSphereLight_Illuminate(
					light,
					bsdf, time, u0, u1,
					shadowRay, directPdfW
					IMAGEMAPS_PARAM);
		default:
			return BLACK;
	}
}

OPENCL_FORCE_INLINE bool Light_IsEnvOrIntersectable(__global const LightSource *light) {
	switch (light->type) {
		case TYPE_IL_CONSTANT:
		case TYPE_IL:
		case TYPE_IL_SKY2:
		case TYPE_SUN:
		case TYPE_TRIANGLE:
			return true;
		case TYPE_SPHERE:
		case TYPE_MAPSPHERE:
		case TYPE_POINT:
		case TYPE_MAPPOINT:
		case TYPE_SPOT:
		case TYPE_PROJECTION:
		case TYPE_SHARPDISTANT:
		case TYPE_DISTANT:
		case TYPE_LASER:
		default:
			return false;
	}
}

OPENCL_FORCE_INLINE float Light_GetAvgPassThroughTransparency(
		__global const LightSource *light
		LIGHTS_PARAM_DECL) {
	if (light->type == TYPE_TRIANGLE) {
		const uint meshIndex = light->triangle.meshIndex;
		const uint materialIndex = sceneObjs[meshIndex].materialIndex;
		
		return mats[materialIndex].avgPassThroughTransparency;
	} else
		return 1.f;
}

#if defined(PARAM_LIGHT_TRACING)

//------------------------------------------------------------------------------
// Light emission for the light tracing (as LightSource::Emit())
//
// The functions return the emitted radiance and set the ray and its pdf
// (emissionPdfW). Emission maps (map point, map sphere and triangle lights
// with an IES/image map) are sampled uniformly: the CPU code samples their
// distribution, not available on the GPU (same expected value).
//
// Guided emission: the light tracing splats only caustic connections, so a
// light path is useful only if its first vertex is on a caster, an object
// with a nearly specular material (see
// CompiledScene::CompileLightTracingCasters()). With probability guide->prob,
// the second half of the emission is sampled toward the bounding sphere of a
// caster instead of with the light distribution: the direction for the lights
// with a position (uniform in the cone of directions toward the sphere), the
// ray origin on the emission disk for the environment lights (uniform in the
// shadow of the sphere). The pdf is the mixture of both, so the estimate stays
// unbiased whatever the casters are.
//------------------------------------------------------------------------------

typedef struct {
	// The bounding spheres of the casters: center x, y, z and radius
	__global const float *casters;
	uint casterCount;
	// The probability of the guided sampling, 0 without casters
	float prob;
	// The random number choosing between the light and the guided sampling,
	// rescaled to pick the caster when the guided sampling is chosen
	float u;
} LightEmitGuide;

OPENCL_FORCE_INLINE bool LightEmitGuide_IsGuided(LightEmitGuide *guide) {
	if (guide->u < guide->prob) {
		guide->u /= guide->prob;
		return true;
	} else
		return false;
}

// The cone of the directions from p toward the bounding sphere of a caster.
// Returns its solid angle and sets its axis and 1 - cos of its half angle
// (2 if p is inside the sphere: all directions).
OPENCL_FORCE_INLINE float LightEmitGuide_CasterCone(__global const float *caster, const float3 p,
		float3 *axis, float *oneMinusCosMax) {
	const float3 toCenter = VLOAD3F(caster) - p;
	const float dist2 = dot(toCenter, toCenter);
	const float radius2 = caster[3] * caster[3];

	if (dist2 <= radius2) {
		*axis = MAKE_FLOAT3(0.f, 0.f, 1.f);
		*oneMinusCosMax = 2.f;
	} else {
		*axis = toCenter / sqrt(dist2);
		const float sin2ThetaMax = radius2 / dist2;
		// Without cancellation, for the small cones
		*oneMinusCosMax = sin2ThetaMax / (1.f + sqrt(1.f - sin2ThetaMax));
	}

	return 2.f * M_PI_F * (*oneMinusCosMax);
}

OPENCL_FORCE_INLINE bool LightEmitGuide_IsInCone(const float3 dir, const float3 axis,
		const float oneMinusCosMax) {
	const float cosTheta = dot(dir, axis);
	if (oneMinusCosMax >= 1.f)
		return (cosTheta >= 1.f - oneMinusCosMax);

	// The sine is accurate for the small cones, the cosine is not
	const float3 c = cross(dir, axis);
	return (cosTheta > 0.f) && (dot(c, c) <= oneMinusCosMax * (2.f - oneMinusCosMax));
}

OPENCL_FORCE_INLINE float LightEmitGuide_TotalSolidAngle(const LightEmitGuide *guide, const float3 p) {
	float total = 0.f;
	for (uint i = 0; i < guide->casterCount; ++i) {
		float3 axis;
		float oneMinusCosMax;
		total += LightEmitGuide_CasterCone(&guide->casters[4 * i], p, &axis, &oneMinusCosMax);
	}

	return total;
}

// Samples a direction from p toward a caster, picked with a probability
// proportional to the solid angle of its cone. Returns the caster index.
OPENCL_FORCE_INLINE uint LightEmitGuide_SampleDir(const LightEmitGuide *guide, const float3 p,
		const float totalSolidAngle, const float u0, const float u1, float3 *dir) {
	float target = guide->u * totalSolidAngle;
	float3 axis;
	float oneMinusCosMax;
	uint i = 0;
	for (;;) {
		const float solidAngle = LightEmitGuide_CasterCone(&guide->casters[4 * i], p, &axis, &oneMinusCosMax);
		if ((target < solidAngle) || (i == guide->casterCount - 1))
			break;
		target -= solidAngle;
		++i;
	}

	// Uniform in the cone, with 1 - cos to keep the small cones accurate
	const float oneMinusCos = u1 * oneMinusCosMax;
	const float sinTheta = sqrt(fmax(0.f, oneMinusCos * (2.f - oneMinusCos)));
	const float phi = 2.f * M_PI_F * u0;
	float3 x, y;
	CoordinateSystem(axis, &x, &y);
	*dir = normalize(sinTheta * (cos(phi) * x + sin(phi) * y) + (1.f - oneMinusCos) * axis);

	return i;
}

// The pdf (solid angle) of the emission direction dir from p: the mixture of
// the light pdf lightPdfW and of the guided sampling. A direction sampled
// toward sampledCaster (NULL_INDEX if not guided) is counted inside its cone
// even if the rounding put it outside.
OPENCL_FORCE_INLINE float LightEmitGuide_DirPdf(const LightEmitGuide *guide, const float3 p,
		const float totalSolidAngle, const float3 dir, const uint sampledCaster,
		const float lightPdfW) {
	if (guide->prob == 0.f)
		return lightPdfW;

	// Each cone holding dir adds 1 / totalSolidAngle: picked with probability
	// solidAngle / totalSolidAngle, sampled with pdf 1 / solidAngle
	uint coneCount = 0;
	for (uint i = 0; i < guide->casterCount; ++i) {
		float3 axis;
		float oneMinusCosMax;
		LightEmitGuide_CasterCone(&guide->casters[4 * i], p, &axis, &oneMinusCosMax);
		if ((i == sampledCaster) || LightEmitGuide_IsInCone(dir, axis, oneMinusCosMax))
			++coneCount;
	}

	return (1.f - guide->prob) * lightPdfW + guide->prob * coneCount / totalSolidAngle;
}

// The center, in the disk coordinates, of the shadow of a caster bounding
// sphere along rayDir on the emission disk plane
OPENCL_FORCE_INLINE float2 LightEmitGuide_ShadowCenter(__global const float *caster,
		const float3 diskCenter, const float3 x, const float3 y, const float3 diskN,
		const float3 rayDir, const float cosDir) {
	const float3 center = VLOAD3F(caster);
	const float3 q = center + (dot(diskCenter - center, diskN) / cosDir) * rayDir - diskCenter;

	return MAKE_FLOAT2(dot(q, x), dot(q, y));
}

// Samples the ray origin of an environment light on its emission disk
// (center diskCenter, orthonormal axes x and y, radius diskRadius) for the
// ray direction rayDir: uniform on the disk or, with the guided sampling,
// uniform in the shadow of a caster bounding sphere. The shadow is an
// ellipse; the sampled disk holds it (radius / |cos|, about 1 here: rayDir is
// nearly normal to the disk). Returns the pdf (area).
OPENCL_FORCE_INLINE float LightEmitGuide_SampleEnvDisk(LightEmitGuide *guide,
		const float3 diskCenter, const float3 x, const float3 y, const float diskRadius,
		const float3 rayDir, const float u0, const float u1, float3 *p) {
	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	const float diskPdf = 1.f / (M_PI_F * diskRadius * diskRadius);

	const float3 diskN = cross(x, y);
	const float cosDir = dot(rayDir, diskN);
	if ((guide->prob == 0.f) || (fabs(cosDir) < 1e-3f)) {
		*p = diskCenter + diskRadius * (d1 * x + d2 * y);
		return diskPdf;
	}
	const float invCos2 = 1.f / (cosDir * cosDir);

	float totalRadius2 = 0.f;
	for (uint i = 0; i < guide->casterCount; ++i)
		totalRadius2 += guide->casters[4 * i + 3] * guide->casters[4 * i + 3];

	// Pick a caster with a probability proportional to its shadow area
	uint sampledCaster = NULL_INDEX;
	float2 diskPos;
	if (LightEmitGuide_IsGuided(guide)) {
		float target = guide->u * totalRadius2;
		uint i = 0;
		for (;;) {
			const float radius2 = guide->casters[4 * i + 3] * guide->casters[4 * i + 3];
			if ((target < radius2) || (i == guide->casterCount - 1))
				break;
			target -= radius2;
			++i;
		}

		sampledCaster = i;
		const float shadowRadius = guide->casters[4 * i + 3] / fabs(cosDir);
		diskPos = LightEmitGuide_ShadowCenter(&guide->casters[4 * i], diskCenter, x, y, diskN, rayDir, cosDir) +
				shadowRadius * MAKE_FLOAT2(d1, d2);
	} else
		diskPos = diskRadius * MAKE_FLOAT2(d1, d2);
	*p = diskCenter + diskPos.x * x + diskPos.y * y;

	// Each shadow holding the point adds 1 / (pi * invCos2 * totalRadius2)
	uint shadowCount = 0;
	for (uint i = 0; i < guide->casterCount; ++i) {
		if (i == sampledCaster)
			++shadowCount;
		else {
			const float2 d = diskPos - LightEmitGuide_ShadowCenter(&guide->casters[4 * i],
					diskCenter, x, y, diskN, rayDir, cosDir);
			const float radius2 = guide->casters[4 * i + 3] * guide->casters[4 * i + 3];
			if (dot(d, d) <= radius2 * invCos2)
				++shadowCount;
		}
	}

	const bool isOnDisk = (sampledCaster == NULL_INDEX) ||
			(dot(diskPos, diskPos) <= diskRadius * diskRadius);
	return (1.f - guide->prob) * (isOnDisk ? diskPdf : 0.f) +
			guide->prob * shadowCount / (M_PI_F * invCos2 * totalRadius2);
}

OPENCL_FORCE_INLINE float3 Light_EmitLocal(__global const LightSource *light,
		const float3 localDir, float3 *rayDir) {
	*rayDir = normalize(Transform_ApplyVector(&light->notIntersectable.light2World, localDir));
	return localDir;
}

OPENCL_FORCE_INLINE float3 Light_WorldToLocal(__global const LightSource *light, const float3 dir) {
	return normalize(Transform_InvApplyVector(&light->notIntersectable.light2World, dir));
}

OPENCL_FORCE_INLINE float2 Light_EmissionMapUV(const float3 localDir) {
	return MAKE_FLOAT2(SphericalPhi(localDir) * (1.f / (2.f * M_PI_F)), SphericalTheta(localDir) * M_1_PI_F);
}

OPENCL_FORCE_INLINE float3 SphereLight_Emit(__global const LightSource *sphereLight,
		LightEmitGuide *guide,
		const float u0, const float u1, const float u2, const float u3,
		float3 *rayOrig, float3 *rayDir, float *emissionPdfW) {
	const float3 absolutePos = VLOAD3F(&sphereLight->notIntersectable.sphere.absolutePos.x);
	const float radius = sphereLight->notIntersectable.sphere.radius;
	const float invArea = 1.f / (4.f * M_PI_F * radius * radius);

	const float3 normal = UniformSampleSphere(u0, u1);
	*rayOrig = absolutePos + radius * normal;

	const float totalSolidAngle = LightEmitGuide_TotalSolidAngle(guide, *rayOrig);
	uint sampledCaster = NULL_INDEX;
	float cosDir, lightPdfW;
	if (LightEmitGuide_IsGuided(guide)) {
		sampledCaster = LightEmitGuide_SampleDir(guide, *rayOrig, totalSolidAngle, u2, u3, rayDir);
		cosDir = dot(*rayDir, normal);
		if (cosDir <= 0.f)
			return BLACK;
		lightPdfW = cosDir * M_1_PI_F;
	} else {
		float3 localDirOut = CosineSampleHemisphereWithPdf(u2, u3, &lightPdfW);
		// Cannot really not emit the particle, so just bias it to the correct angle
		localDirOut.z = fmax(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);
		cosDir = localDirOut.z;

		float3 x, y;
		CoordinateSystem(normal, &x, &y);
		*rayDir = localDirOut.x * x + localDirOut.y * y + localDirOut.z * normal;
	}

	*emissionPdfW = invArea * LightEmitGuide_DirPdf(guide, *rayOrig, totalSolidAngle, *rayDir,
			sampledCaster, lightPdfW);

	return VLOAD3F(sphereLight->notIntersectable.sphere.emittedFactor.c) *
			(invArea * cosDir * M_1_PI_F);
}

OPENCL_FORCE_INLINE float3 TriangleLight_Emit(__global const LightSource *triLight,
		LightEmitGuide *guide,
		__global HitPoint *tmpHitPoint,
		const float time, const float u0, const float u1, const float u2, const float u3,
		const float passThroughEvent,
		float3 *rayOrig, float3 *rayDir, float *emissionPdfW
		MATERIALS_PARAM_DECL) {
	// A safety check to avoid NaN/Inf
	if ((triLight->triangle.invTriangleArea == 0.f) || (triLight->triangle.invMeshArea == 0.f))
		return BLACK;

	const uint meshIndex = triLight->triangle.meshIndex;
	const uint triangleIndex = triLight->triangle.triangleIndex;
	const uint materialIndex = sceneObjs[meshIndex].materialIndex;

	// Origin
	Transform localToWorld;
	ExtMesh_GetLocal2World(time, meshIndex, triangleIndex, &localToWorld EXTMESH_PARAM);

	float b0, b1, b2;
	float3 samplePoint;
	ExtMesh_Sample(&localToWorld,
			meshIndex, triangleIndex,
			u0, u1,
			&samplePoint, &b0, &b1, &b2
			EXTMESH_PARAM);

	// Build a temporary HitPoint (as TriangleLight::Emit(), the fixed
	// direction is the geometry normal)
	const float3 geometryN = ExtMesh_GetGeometryNormal(&localToWorld, meshIndex, triangleIndex EXTMESH_PARAM);
	HitPoint_Init(tmpHitPoint, false,
		meshIndex, triangleIndex,
		samplePoint, geometryN,
		b1, b2,
		passThroughEvent,
		&localToWorld, time
		MATERIALS_PARAM);
	tmpHitPoint->flags |= HITPOINT_FROM_LIGHT;

	Frame frame;
	HitPoint_GetFrame(tmpHitPoint, &frame);

	// Direction
	const bool hasEmissionMap = (triLight->triangle.imageMapIndex != NULL_INDEX);
	const float cosThetaMax = hasEmissionMap ? 0.f : Material_GetEmittedCosThetaMax(materialIndex
			MATERIALS_PARAM);
	// The emission theta 0 is a single direction, it can't be guided
	const bool isGuided = (cosThetaMax < 1.f) && LightEmitGuide_IsGuided(guide);
	const float totalSolidAngle = isGuided ? LightEmitGuide_TotalSolidAngle(guide, samplePoint) : 0.f;

	float3 localDirOut;
	float lightPdfW;
	uint sampledCaster = NULL_INDEX;
	if (isGuided) {
		sampledCaster = LightEmitGuide_SampleDir(guide, samplePoint, totalSolidAngle, u2, u3, rayDir);
		localDirOut = Frame_ToLocal_Private(&frame, *rayDir);

		if (hasEmissionMap)
			lightPdfW = UniformSpherePdf();
		else if (localDirOut.z <= 0.f)
			return BLACK;
		else if (cosThetaMax > 0.f) {
			if (localDirOut.z < cosThetaMax)
				return BLACK;
			lightPdfW = UniformConePdf(cosThetaMax);
		} else
			lightPdfW = localDirOut.z * M_1_PI_F;
	} else {
		if (hasEmissionMap) {
			// The emission map can emit even backward
			localDirOut = UniformSampleSphere(u2, u3);
			lightPdfW = UniformSpherePdf();
		} else {
			if (cosThetaMax >= 1.f) {
				// Emission theta is 0
				localDirOut = MAKE_FLOAT3(0.f, 0.f, 1.f);
				lightPdfW = 1.f;
			} else if (cosThetaMax > 0.f) {
				// Emission theta is less than 90 degrees
				localDirOut = UniformSampleCone(u2, u3, cosThetaMax,
						MAKE_FLOAT3(1.f, 0.f, 0.f), MAKE_FLOAT3(0.f, 1.f, 0.f), MAKE_FLOAT3(0.f, 0.f, 1.f));
				lightPdfW = UniformConePdf(cosThetaMax);
			} else
				localDirOut = CosineSampleHemisphereWithPdf(u2, u3, &lightPdfW);

			// Cannot really not emit the particle, so just bias it to the correct angle
			localDirOut.z = fmax(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);
		}

		*rayDir = Frame_ToWorld_Private(&frame, localDirOut);
	}

	if (lightPdfW == 0.f)
		return BLACK;
	// The emission theta 0 keeps its pdf 1: the guided sampling is off for it
	*emissionPdfW = triLight->triangle.invTriangleArea * ((cosThetaMax < 1.f) ?
			LightEmitGuide_DirPdf(guide, samplePoint,
				isGuided ? totalSolidAngle : LightEmitGuide_TotalSolidAngle(guide, samplePoint),
				*rayDir, sampledCaster, lightPdfW) :
			lightPdfW);

	float3 emissionColor = WHITE;
	if (hasEmissionMap) {
		__global const ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		const float2 uv = Light_EmissionMapUV(localDirOut);
		emissionColor = ImageMap_GetSpectrum(
				imageMap,
				uv.x, uv.y
				IMAGEMAPS_PARAM) / triLight->triangle.average;
	}

	// With an IES/map the light can be emitted from the back face too so I
	// have to use rayDir here instead of intoObject
	*rayOrig = samplePoint + geometryN * MachineEpsilon_E_Float3(samplePoint) *
			((dot(*rayDir, geometryN) > 0.f) ? 1.f : -1.f);

	return Material_GetEmittedRadiance(materialIndex,
			tmpHitPoint, triLight->triangle.invMeshArea
			MATERIALS_PARAM) * emissionColor * fabs(localDirOut.z);
}

OPENCL_FORCE_INLINE float3 Light_Emit(__global const LightSource *light,
		LightEmitGuide *guide,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		__global HitPoint *tmpHitPoint,
		const float time, const float u0, const float u1, const float u2, const float u3,
		const float passThroughEvent,
		float3 *rayOrig, float3 *rayDir, float *emissionPdfW
		LIGHTS_PARAM_DECL) {
	const float3 worldCenter = MAKE_FLOAT3(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = EnvLightSource_GetEnvRadius(sceneRadius);

	switch (light->type) {
		case TYPE_IL_CONSTANT: {
			// A uniform direction and a uniform origin on the disk of the
			// scene bounding sphere: the same distribution of rays as 2
			// uniform points on the sphere (LightSource::Emit())
			*rayDir = UniformSampleSphere(u2, u3);

			float3 x, y;
			CoordinateSystem(-(*rayDir), &x, &y);
			float3 pDisk;
			const float diskPdf = LightEmitGuide_SampleEnvDisk(guide, worldCenter, x, y, envRadius,
					*rayDir, u0, u1, &pDisk);
			*rayOrig = pDisk - envRadius * (*rayDir);
			*emissionPdfW = UniformSpherePdf() * diskPdf;

			return VLOAD3F(light->notIntersectable.temperatureScale.c) *
					VLOAD3F(light->notIntersectable.gain.c) *
					VLOAD3F(light->notIntersectable.constantInfinite.color.c);
		}
		case TYPE_IL:
		case TYPE_IL_SKY2: {
			const bool isSky2 = (light->type == TYPE_IL_SKY2);

			float2 sampleUV;
			float distPdf;
			Distribution2D_SampleContinuous(&envLightDistribution[isSky2 ?
					light->notIntersectable.sky2.distributionOffset :
					light->notIntersectable.infinite.distributionOffset],
					u0, u1, &sampleUV, &distPdf);
			if (distPdf == 0.f)
				return BLACK;

			float3 dir;
			float latLongMappingPdf;
			EnvLightSource_FromLatLongMapping(sampleUV.x, sampleUV.y, &dir, &latLongMappingPdf);
			if (latLongMappingPdf == 0.f)
				return BLACK;

			// dir is the direction toward the light
			if (!isSky2)
				dir = normalize(Transform_ApplyVector(&light->notIntersectable.light2World, dir));
			*rayDir = -dir;

			// Ray origin on the disk of the scene bounding sphere seen from rayDir
			float3 x, y;
			CoordinateSystem(dir, &x, &y);
			float3 pDisk;
			const float diskPdf = LightEmitGuide_SampleEnvDisk(guide, worldCenter, x, y, envRadius,
					*rayDir, u2, u3, &pDisk);
			*rayOrig = pDisk - envRadius * (*rayDir);
			*emissionPdfW = distPdf * latLongMappingPdf * diskPdf;

			if (isSky2)
				return Sky2Light_ComputeRadiance(light, dir);
			else {
				__global const ImageMap *imageMap = &imageMapDescs[light->notIntersectable.infinite.imageMapIndex];
				return VLOAD3F(light->notIntersectable.temperatureScale.c) *
						VLOAD3F(light->notIntersectable.gain.c) *
						ImageMap_GetSpectrum(
							imageMap,
							sampleUV.x, sampleUV.y
							IMAGEMAPS_PARAM);
			}
		}
		case TYPE_SUN: {
			const float cosThetaMax = light->notIntersectable.sun.cosThetaMax;
			const float3 sunDir = VLOAD3F(&light->notIntersectable.sun.absoluteDir.x);
			const float3 x = VLOAD3F(&light->notIntersectable.sun.x.x);
			const float3 y = VLOAD3F(&light->notIntersectable.sun.y.x);

			*rayDir = -UniformSampleCone(u2, u3, cosThetaMax, x, y, sunDir);
			const float diskPdf = LightEmitGuide_SampleEnvDisk(guide, worldCenter + envRadius * sunDir,
					x, y, envRadius, *rayDir, u0, u1, rayOrig);
			*emissionPdfW = UniformConePdf(cosThetaMax) * diskPdf;

			return VLOAD3F(light->notIntersectable.sun.color.c);
		}
		case TYPE_SHARPDISTANT:
		case TYPE_DISTANT: {
			const float3 lightDir = VLOAD3F(&light->notIntersectable.sharpDistant.absoluteLightDir.x);

			float3 x, y;
			float dirPdf, uDisk0, uDisk1;
			if (light->type == TYPE_DISTANT) {
				const float cosThetaMax = light->notIntersectable.distant.cosThetaMax;
				x = VLOAD3F(&light->notIntersectable.distant.x.x);
				y = VLOAD3F(&light->notIntersectable.distant.y.x);
				*rayDir = UniformSampleCone(u0, u1, cosThetaMax, x, y, lightDir);
				dirPdf = UniformConePdf(cosThetaMax);
				uDisk0 = u2;
				uDisk1 = u3;
			} else {
				CoordinateSystem(lightDir, &x, &y);
				*rayDir = lightDir;
				dirPdf = 1.f;
				uDisk0 = u0;
				uDisk1 = u1;
			}

			const float diskPdf = LightEmitGuide_SampleEnvDisk(guide, worldCenter - envRadius * lightDir,
					x, y, envRadius, *rayDir, uDisk0, uDisk1, rayOrig);
			*emissionPdfW = dirPdf * diskPdf;

			return VLOAD3F(light->notIntersectable.temperatureScale.c) *
					VLOAD3F(light->notIntersectable.gain.c) *
					VLOAD3F(light->notIntersectable.sharpDistant.color.c);
		}
		case TYPE_TRIANGLE:
			return TriangleLight_Emit(light, guide, tmpHitPoint, time, u0, u1, u2, u3, passThroughEvent,
					rayOrig, rayDir, emissionPdfW
					MATERIALS_PARAM);
		case TYPE_POINT: {
			*rayOrig = VLOAD3F(&light->notIntersectable.point.absolutePos.x);

			const float totalSolidAngle = LightEmitGuide_TotalSolidAngle(guide, *rayOrig);
			uint sampledCaster = NULL_INDEX;
			if (LightEmitGuide_IsGuided(guide))
				sampledCaster = LightEmitGuide_SampleDir(guide, *rayOrig, totalSolidAngle, u0, u1, rayDir);
			else
				*rayDir = UniformSampleSphere(u0, u1);
			*emissionPdfW = LightEmitGuide_DirPdf(guide, *rayOrig, totalSolidAngle, *rayDir,
					sampledCaster, 1.f / (4.f * M_PI_F));

			return VLOAD3F(light->notIntersectable.point.emittedFactor.c) * (1.f / (4.f * M_PI_F));
		}
		case TYPE_MAPPOINT: {
			*rayOrig = VLOAD3F(&light->notIntersectable.mapPoint.absolutePos.x);

			const float totalSolidAngle = LightEmitGuide_TotalSolidAngle(guide, *rayOrig);
			uint sampledCaster = NULL_INDEX;
			float3 localFromLight;
			if (LightEmitGuide_IsGuided(guide)) {
				sampledCaster = LightEmitGuide_SampleDir(guide, *rayOrig, totalSolidAngle, u0, u1, rayDir);
				localFromLight = Light_WorldToLocal(light, *rayDir);
			} else
				localFromLight = Light_EmitLocal(light, UniformSampleSphere(u0, u1), rayDir);
			*emissionPdfW = LightEmitGuide_DirPdf(guide, *rayOrig, totalSolidAngle, *rayDir,
					sampledCaster, 1.f / (4.f * M_PI_F));

			__global const ImageMap *imageMap = &imageMapDescs[light->notIntersectable.mapPoint.imageMapIndex];
			const float2 uv = Light_EmissionMapUV(localFromLight);
			return VLOAD3F(light->notIntersectable.mapPoint.emittedFactor.c) *
					ImageMap_GetSpectrum(
						imageMap,
						uv.x, uv.y
						IMAGEMAPS_PARAM) / (4.f * M_PI_F * light->notIntersectable.mapPoint.average);
		}
		case TYPE_SPOT: {
			const float cosTotalWidth = light->notIntersectable.spot.cosTotalWidth;
			const float uniformConePdf = UniformConePdf(cosTotalWidth);

			*rayOrig = VLOAD3F(&light->notIntersectable.spot.absolutePos.x);

			const float totalSolidAngle = LightEmitGuide_TotalSolidAngle(guide, *rayOrig);
			uint sampledCaster = NULL_INDEX;
			float3 localFromLight;
			float lightPdfW;
			if (LightEmitGuide_IsGuided(guide)) {
				sampledCaster = LightEmitGuide_SampleDir(guide, *rayOrig, totalSolidAngle, u0, u1, rayDir);
				localFromLight = Light_WorldToLocal(light, *rayDir);
				lightPdfW = (CosTheta(localFromLight) >= cosTotalWidth) ? uniformConePdf : 0.f;
			} else {
				localFromLight = Light_EmitLocal(light,
						UniformSampleCone(u0, u1, cosTotalWidth,
							MAKE_FLOAT3(1.f, 0.f, 0.f), MAKE_FLOAT3(0.f, 1.f, 0.f), MAKE_FLOAT3(0.f, 0.f, 1.f)),
						rayDir);
				lightPdfW = uniformConePdf;
			}

			const float falloff = SpotLight_LocalFalloff(localFromLight, cosTotalWidth,
					light->notIntersectable.spot.cosFalloffStart);
			if (falloff == 0.f)
				return BLACK;
			*emissionPdfW = LightEmitGuide_DirPdf(guide, *rayOrig, totalSolidAngle, *rayDir,
					sampledCaster, lightPdfW);

			return VLOAD3F(light->notIntersectable.spot.emittedFactor.c) *
					(falloff / fabs(CosTheta(localFromLight)));
		}
		case TYPE_PROJECTION: {
			// Not guided
			__global const ProjectionLightParam *projection = &light->notIntersectable.projection;

			*rayOrig = VLOAD3F(&projection->absolutePos.x);
			const float3 ps = Matrix4x4_ApplyPoint(&projection->lightProjectionInv,
					MAKE_FLOAT3(u0 * (projection->screenX1 - projection->screenX0) + projection->screenX0,
						u1 * (projection->screenY1 - projection->screenY0) + projection->screenY0, 0.f));
			Light_EmitLocal(light, ps, rayDir);

			const float cosAtLight = dot(*rayDir, VLOAD3F(&projection->lightNormal.x));
			*emissionPdfW = 1.f / (projection->area * cosAtLight * cosAtLight * cosAtLight);

			float3 result = VLOAD3F(projection->emittedFactor.c);
			if (projection->imageMapIndex != NULL_INDEX) {
				__global const ImageMap *imageMap = &imageMapDescs[projection->imageMapIndex];
				result *= ImageMap_GetSpectrum(
						imageMap,
						u0, u1
						IMAGEMAPS_PARAM);
			}

			return result;
		}
		case TYPE_LASER: {
			// Not guided
			const float3 lightDir = VLOAD3F(&light->notIntersectable.laser.absoluteLightDir.x);
			const float radius = light->notIntersectable.laser.radius;

			float3 x, y;
			CoordinateSystem(lightDir, &x, &y);
			float d1, d2;
			ConcentricSampleDisk(u0, u1, &d1, &d2);

			*rayOrig = VLOAD3F(&light->notIntersectable.laser.absoluteLightPos.x) - radius * (d1 * x + d2 * y);
			*rayDir = lightDir;
			*emissionPdfW = 1.f / (M_PI_F * radius * radius);

			return VLOAD3F(light->notIntersectable.laser.emittedFactor.c);
		}
		case TYPE_SPHERE:
			return SphereLight_Emit(light, guide, u0, u1, u2, u3, rayOrig, rayDir, emissionPdfW);
		case TYPE_MAPSPHERE: {
			const float3 result = SphereLight_Emit(light, guide, u0, u1, u2, u3, rayOrig, rayDir, emissionPdfW);

			const float3 localFromLight = Light_WorldToLocal(light, *rayDir);
			__global const ImageMap *imageMap = &imageMapDescs[light->notIntersectable.mapSphere.imageMapIndex];
			const float2 uv = Light_EmissionMapUV(localFromLight);

			return result * ImageMap_GetSpectrum(
					imageMap,
					uv.x, uv.y
					IMAGEMAPS_PARAM) * (1.f / light->notIntersectable.mapSphere.average);
		}
		default:
			return BLACK;
	}
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
