#line 2 "light_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

float InfiniteLightSource_GetEnvRadius(const float sceneRadius) {
	// This is used to scale the world radius in sun/sky/infinite lights in order to
	// avoid problems with objects that are near the borderline of the world bounding sphere
	return PARAM_LIGHT_WORLD_RADIUS_SCALE * sceneRadius;
}

//------------------------------------------------------------------------------
// ConstantInfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)

float3 ConstantInfiniteLight_GetRadiance(__global const LightSource *constantInfiniteLight,
		const float3 dir, float *directPdfA) {
	*directPdfA = 1.f / (4.f * M_PI_F);

	return VLOAD3F(constantInfiniteLight->notIntersectable.gain.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.constantInfinite.color.c);
}

float3 ConstantInfiniteLight_Illuminate(__global const LightSource *constantInfiniteLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float u0, const float u1, const float3 p,
		float3 *dir, float *distance, float *directPdfW) {
	const float phi = u0 * 2.f * M_PI_F;
	const float theta = u1 * M_PI_F;
	*dir = SphericalDirection(sin(theta), cos(theta), phi);

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);

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

	*directPdfW = 1.f / (4.f * M_PI_F);

	return VLOAD3F(constantInfiniteLight->notIntersectable.gain.c) *
			VLOAD3F(constantInfiniteLight->notIntersectable.constantInfinite.color.c);
}

#endif

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_INFINITELIGHT)

float3 InfiniteLight_GetRadiance(__global const LightSource *infiniteLight,
		__global const float *infiniteLightDistirbution,
		const float3 dir, float *directPdfA
		IMAGEMAPS_PARAM_DECL) {
	__global const ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersectable.infinite.imageMapIndex];

	const float3 localDir = normalize(Transform_InvApplyVector(&infiniteLight->notIntersectable.light2World, -dir));
	const float2 uv = (float2)(
		SphericalPhi(localDir) * (1.f / (2.f * M_PI_F)),
		SphericalTheta(localDir) * M_1_PI_F);

	// TextureMapping2D_Map() is expended here
	const float2 scale = VLOAD2F(&infiniteLight->notIntersectable.infinite.mapping.uvMapping2D.uScale);
	const float2 delta = VLOAD2F(&infiniteLight->notIntersectable.infinite.mapping.uvMapping2D.uDelta);
	const float2 mapUV = uv * scale + delta;

	const float distPdf = Distribution2D_Pdf(infiniteLightDistirbution, mapUV.s0, mapUV.s1);
	*directPdfA = distPdf / (4.f * M_PI_F);

	return VLOAD3F(infiniteLight->notIntersectable.gain.c) * ImageMap_GetSpectrum(
			imageMap,
			mapUV.s0, mapUV.s1
			IMAGEMAPS_PARAM);
}

float3 InfiniteLight_Illuminate(__global const LightSource *infiniteLight,
		__global const float *infiniteLightDistirbution,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float u0, const float u1, const float3 p,
		float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	float2 sampleUV;
	float distPdf;
	Distribution2D_SampleContinuous(infiniteLightDistirbution, u0, u1, &sampleUV, &distPdf);

	const float phi = sampleUV.s0 * 2.f * M_PI_F;
	const float theta = sampleUV.s1 * M_PI_F;
	*dir = normalize(Transform_ApplyVector(&infiniteLight->notIntersectable.light2World,
			SphericalDirection(sin(theta), cos(theta), phi)));

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);

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

	*directPdfW = distPdf / (4.f * M_PI_F);

	// InfiniteLight_GetRadiance is expended here
	__global const ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersectable.infinite.imageMapIndex];

	const float2 uv = (float2)(sampleUV.s0, sampleUV.s1);

	// TextureMapping2D_Map() is expended here
	const float2 scale = VLOAD2F(&infiniteLight->notIntersectable.infinite.mapping.uvMapping2D.uScale);
	const float2 delta = VLOAD2F(&infiniteLight->notIntersectable.infinite.mapping.uvMapping2D.uDelta);
	const float2 mapUV = uv * scale + delta;
	
	return VLOAD3F(infiniteLight->notIntersectable.gain.c) * ImageMap_GetSpectrum(
			imageMap,
			mapUV.s0, mapUV.s1
			IMAGEMAPS_PARAM);
}

#endif

//------------------------------------------------------------------------------
// SkyLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT)

float SkyLight_PerezBase(__global float *lam, const float theta, const float gamma) {
	return (1.f + lam[1] * exp(lam[2] / cos(theta))) *
		(1.f + lam[3] * exp(lam[4] * gamma)  + lam[5] * cos(gamma) * cos(gamma));
}

float SkyLight_RiAngleBetween(const float thetav, const float phiv, const float theta, const float phi) {
	const float cospsi = sin(thetav) * sin(theta) * cos(phi - phiv) + cos(thetav) * cos(theta);
	if (cospsi >= 1.f)
		return 0.f;
	if (cospsi <= -1.f)
		return M_PI_F;
	return acos(cospsi);
}

float3 SkyLight_ChromaticityToSpectrum(float Y, float x, float y) {
	float X, Z;
	
	if (y != 0.f)
		X = (x / y) * Y;
	else
		X = 0.f;
	
	if (y != 0.f && Y != 0.f)
		Z = (1.f - x - y) / y * Y;
	else
		Z = 0.f;

	// Assuming sRGB (D65 illuminant)
	return (float3)(3.2410f * X - 1.5374f * Y - 0.4986f * Z,
			-0.9692f * X + 1.8760f * Y + 0.0416f * Z,
			0.0556f * X - 0.2040f * Y + 1.0570f * Z);
}

float3 SkyLight_GetSkySpectralRadiance(__global const LightSource *skyLight,
		const float theta, const float phi) {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = fmin(theta, (M_PI_F * .5f) - .001f);
	const float gamma = SkyLight_RiAngleBetween(theta, phi, 
			skyLight->notIntersectable.sky.absoluteTheta, skyLight->notIntersectable.sky.absolutePhi);

	// Compute xyY values
	const float x = skyLight->notIntersectable.sky.zenith_x * SkyLight_PerezBase(
			skyLight->notIntersectable.sky.perez_x, theta_fin, gamma);
	const float y = skyLight->notIntersectable.sky.zenith_y * SkyLight_PerezBase(
			skyLight->notIntersectable.sky.perez_y, theta_fin, gamma);
	const float Y = skyLight->notIntersectable.sky.zenith_Y * SkyLight_PerezBase(
			skyLight->notIntersectable.sky.perez_Y, theta_fin, gamma);

	return SkyLight_ChromaticityToSpectrum(Y, x, y);
}

float3 SkyLight_GetRadiance(__global const LightSource *skyLight, const float3 dir,
		float *directPdfA) {
	*directPdfA = 1.f / (4.f * M_PI_F);

	const float theta = SphericalTheta(-dir);
	const float phi = SphericalPhi(-dir);
	const float3 s = SkyLight_GetSkySpectralRadiance(skyLight, theta, phi);

	return VLOAD3F(skyLight->notIntersectable.gain.c) * s;
}

float3 SkyLight_Illuminate(__global const LightSource *skyLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float u0, const float u1, const float3 p,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);

	const float3 localDir = normalize(Transform_ApplyVector(&skyLight->notIntersectable.light2World, -(*dir)));
	*dir = normalize(Transform_ApplyVector(&skyLight->notIntersectable.light2World,  UniformSampleSphere(u0, u1)));

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

	return SkyLight_GetRadiance(skyLight, -(*dir), directPdfW);
}

#endif

//------------------------------------------------------------------------------
// Sky2Light
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT2)

float RiCosBetween(const float3 w1, const float3 w2) {
	return clamp(dot(w1, w2), -1.f, 1.f);
}

float3 SkyLight2_ComputeRadiance(__global const LightSource *skyLight2, const float3 w) {
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

float3 SkyLight2_GetRadiance(__global const LightSource *skyLight2, const float3 dir,
		float *directPdfA) {
	*directPdfA = 1.f / (4.f * M_PI_F);
	
	const float3 w = -dir;
	if (skyLight2->notIntersectable.sky2.hasGround &&
			(dot(w, VLOAD3F(&skyLight2->notIntersectable.sky2.absoluteUpDir.x)) < 0.f)) {
		// Higher hemisphere
		return VLOAD3F(skyLight2->notIntersectable.sky2.scaledGroundColor.c);
	} else {
		// Lower hemisphere
		const float3 s = SkyLight2_ComputeRadiance(skyLight2, w);

		return VLOAD3F(skyLight2->notIntersectable.gain.c) * s;
	}
}

float3 SkyLight2_Illuminate(__global const LightSource *skyLight2,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float u0, const float u1, const float3 p,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);

	const float3 localDir = normalize(Transform_ApplyVector(&skyLight2->notIntersectable.light2World, -(*dir)));
	*dir = normalize(Transform_ApplyVector(&skyLight2->notIntersectable.light2World,  UniformSampleSphere(u0, u1)));

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

	return SkyLight2_GetRadiance(skyLight2, -(*dir), directPdfW);
}

#endif

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT)

float3 SunLight_Illuminate(__global const LightSource *sunLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float3 p, const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float cosThetaMax = sunLight->notIntersectable.sun.cosThetaMax;
	const float3 sunDir = VLOAD3F(&sunLight->notIntersectable.sun.absoluteDir.x);
	*dir = UniformSampleCone(u0, u1, cosThetaMax, VLOAD3F(&sunLight->notIntersectable.sun.x.x), VLOAD3F(&sunLight->notIntersectable.sun.y.x), sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = dot(sunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return BLACK;

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);
	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	*directPdfW = UniformConePdf(cosThetaMax);

	return VLOAD3F(sunLight->notIntersectable.sun.color.c);
}

float3 SunLight_GetRadiance(__global const LightSource *sunLight, const float3 dir, float *directPdfA) {
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

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)

float3 TriangleLight_Illuminate(__global const LightSource *triLight,
		__global HitPoint *tmpHitPoint,
		const float3 p, const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float3 *dir, float *distance, float *directPdfW
		MATERIALS_PARAM_DECL) {
	const float3 p0 = VLOAD3F(&triLight->triangle.v0.x);
	const float3 p1 = VLOAD3F(&triLight->triangle.v1.x);
	const float3 p2 = VLOAD3F(&triLight->triangle.v2.x);
	float b0, b1, b2;
	const float3 samplePoint = Triangle_Sample(
			p0, p1, p2,
			u0, u1,
			&b0, &b1, &b2);

	*dir = samplePoint - p;
	const float distanceSquared = dot(*dir, *dir);;
	*distance = sqrt(distanceSquared);
	*dir /= (*distance);
	
	const float3 n0 = VLOAD3F(&triLight->triangle.n0.x);
	const float3 n1 = VLOAD3F(&triLight->triangle.n1.x);
	const float3 n2 = VLOAD3F(&triLight->triangle.n2.x);
	const float3 shadeN = Triangle_InterpolateNormal(n0, n1, n2, b0, b1, b2);

	const float cosAtLight = dot(shadeN, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
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
		*directPdfW = triLight->triangle.invTriangleArea * distanceSquared / cosAtLight;

	return Material_GetEmittedRadiance(triLight->triangle.materialIndex,
			tmpHitPoint, triLight->triangle.invMeshArea
			MATERIALS_PARAM) * emissionColor;
}

float3 TriangleLight_GetRadiance(__global const LightSource *triLight,
		 __global HitPoint *hitPoint, float *directPdfA
		MATERIALS_PARAM_DECL) {
	const float3 dir = VLOAD3F(&hitPoint->fixedDir.x);
	const float3 hitPointNormal = VLOAD3F(&hitPoint->geometryN.x);
	const float cosOutLight = dot(hitPointNormal, dir);
	if (cosOutLight <= 0.f)
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

float3 PointLight_Illuminate(__global const LightSource *pointLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW) {
	const float3 toLight = VLOAD3F(&pointLight->notIntersectable.point.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	*directPdfW = distanceSquared;

	return VLOAD3F(pointLight->notIntersectable.point.emittedFactor.c);
}

#endif

//------------------------------------------------------------------------------
// MapPointLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_MAPPOINTLIGHT)

float3 MapPointLight_Illuminate(__global const LightSource *mapPointLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 toLight = VLOAD3F(&mapPointLight->notIntersectable.mapPoint.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	*directPdfW = distanceSquared;

	// Retrieve the image map information
	__global const ImageMap *imageMap = &imageMapDescs[mapPointLight->notIntersectable.mapPoint.imageMapIndex];

	const float3 localFromLight = normalize(Transform_InvApplyVector(&mapPointLight->notIntersectable.light2World, p) - 
		VLOAD3F(&mapPointLight->notIntersectable.mapPoint.localPos.x));
	const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
	const float3 emissionColor = ImageMap_GetSpectrum(
			imageMap,
			uv.s0, uv.s1
			IMAGEMAPS_PARAM) / mapPointLight->notIntersectable.mapPoint.avarage;

	return VLOAD3F(mapPointLight->notIntersectable.mapPoint.emittedFactor.c) * emissionColor;
}

#endif

//------------------------------------------------------------------------------
// SpotLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SPOTLIGHT)

float SpotLight_LocalFalloff(const float3 w, const float cosTotalWidth, const float cosFalloffStart) {
	if (CosTheta(w) < cosTotalWidth)
		return 0.f;
 	if (CosTheta(w) > cosFalloffStart)
		return 1.f;

	// Compute falloff inside spotlight cone
	const float delta = (CosTheta(w) - cosTotalWidth) / (cosFalloffStart - cosTotalWidth);
	return pow(delta, 4);
}

float3 SpotLight_Illuminate(__global const LightSource *spotLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW) {
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

#if defined(PARAM_HAS_PROJECTIONLIGHT)

float3 ProjectionLight_Illuminate(__global const LightSource *projectionLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
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

float3 SharpDistantLight_Illuminate(__global const LightSource *sharpDistantLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float3 p,	float3 *dir, float *distance, float *directPdfW) {
	*dir = -VLOAD3F(&sharpDistantLight->notIntersectable.sharpDistant.absoluteLightDir.x);

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);
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

float3 DistantLight_Illuminate(__global const LightSource *distantLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float3 p,	const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 absoluteLightDir = VLOAD3F(&distantLight->notIntersectable.distant.absoluteLightDir.x);
	const float3 x = VLOAD3F(&distantLight->notIntersectable.distant.x.x);
	const float3 y = VLOAD3F(&distantLight->notIntersectable.distant.y.x);
	const float cosThetaMax = distantLight->notIntersectable.distant.cosThetaMax;
	*dir = -UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float envRadius = InfiniteLightSource_GetEnvRadius(sceneRadius);
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

float3 LaserLight_Illuminate(__global const LightSource *laserLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW) {
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

float3 EnvLight_GetRadiance(__global const LightSource *light, const float3 dir, float *directPdfA
				LIGHTS_PARAM_DECL) {
	switch (light->type) {
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)
		case TYPE_IL_CONSTANT:
			return ConstantInfiniteLight_GetRadiance(light,
					dir, directPdfA);
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		case TYPE_IL:
			return InfiniteLight_GetRadiance(light,
					&infiniteLightDistribution[light->notIntersectable.infinite.distributionOffset],
					dir, directPdfA
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		case TYPE_IL_SKY:
			return SkyLight_GetRadiance(light,
					dir, directPdfA);
#endif
#if defined(PARAM_HAS_SKYLIGHT2)
		case TYPE_IL_SKY2:
			return SkyLight2_GetRadiance(light,
					dir, directPdfA);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
			return SunLight_GetRadiance(light,
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

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
float3 IntersectableLight_GetRadiance(__global const LightSource *light,
		 __global HitPoint *hitPoint, float *directPdfA
		LIGHTS_PARAM_DECL) {
	return TriangleLight_GetRadiance(light, hitPoint, directPdfA
			MATERIALS_PARAM);
}
#endif

float3 Light_Illuminate(
		__global const LightSource *light,
		const float3 point,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float envRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		float3 *lightRayDir, float *distance, float *directPdfW
		LIGHTS_PARAM_DECL) {
	switch (light->type) {
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)
		case TYPE_IL_CONSTANT:
			return ConstantInfiniteLight_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				u0, u1,
				point,
				lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		case TYPE_IL:
			return InfiniteLight_Illuminate(
				light,
				&infiniteLightDistribution[light->notIntersectable.infinite.distributionOffset],
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				u0, u1,
				point,
				lightRayDir, distance, directPdfW
				IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		case TYPE_IL_SKY:
			return SkyLight_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				u0, u1,
				point,
				lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_SKYLIGHT2)
		case TYPE_IL_SKY2:
			return SkyLight2_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				u0, u1,
				point,
				lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
			return SunLight_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				point, u0, u1, lightRayDir, distance, directPdfW);
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		case TYPE_TRIANGLE:
			return TriangleLight_Illuminate(
					light,
					tmpHitPoint,
					point,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					lightRayDir, distance, directPdfW
					MATERIALS_PARAM);
#endif
#if defined(PARAM_HAS_POINTLIGHT)
		case TYPE_POINT:
			return PointLight_Illuminate(
					light, point,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_MAPPOINTLIGHT)
		case TYPE_MAPPOINT:
			return MapPointLight_Illuminate(
					light, point,
					lightRayDir, distance, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SPOTLIGHT)
		case TYPE_SPOT:
			return SpotLight_Illuminate(
					light, point,
					lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_PROJECTIONLIGHT)
		case TYPE_PROJECTION:
			return ProjectionLight_Illuminate(
					light, point,
					lightRayDir, distance, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SHARPDISTANTLIGHT)
		case TYPE_SHARPDISTANT:
			return SharpDistantLight_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				point, lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_DISTANTLIGHT)
		case TYPE_DISTANT:
			return DistantLight_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, envRadius,
				point, u0, u1, lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_LASERLIGHT)
		case TYPE_LASER:
			return LaserLight_Illuminate(
					light, point,
					lightRayDir, distance, directPdfW);
#endif
		default:
			return BLACK;
	}
}

bool Light_IsEnvOrIntersectable(__global const LightSource *light) {
	switch (light->type) {
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT)
		case TYPE_IL_CONSTANT:
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		case TYPE_IL:
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		case TYPE_IL_SKY:
#endif
#if defined(PARAM_HAS_SKYLIGHT2)
		case TYPE_IL_SKY2:
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		case TYPE_TRIANGLE:
#endif
#if defined(PARAM_HAS_CONSTANTINFINITELIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_SKYLIGHT2) || defined(PARAM_HAS_SUNLIGHT) || (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			return true;
#endif

#if defined(PARAM_HAS_POINTLIGHT)
		case TYPE_POINT:
#endif
#if defined(PARAM_HAS_MAPPOINTLIGHT)
		case TYPE_MAPPOINT:
#endif
#if defined(PARAM_HAS_SPOTLIGHT)
		case TYPE_SPOT:
#endif
#if defined(PARAM_HAS_PROJECTIONLIGHT)
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
#if defined(PARAM_HAS_POINTLIGHT) || defined(PARAM_HAS_MAPPOINTLIGHT) || defined(PARAM_HAS_SPOTLIGHT) || defined(PARAM_HAS_PROJECTIONLIGHT) || defined(PARAM_HAS_SHARPDISTANTLIGHT) || defined(PARAM_HAS_DISTANTLIGHT) || defined(PARAM_HAS_LASERLIGHT)
			return false;
#endif

		default:
			return false;
	}
}
