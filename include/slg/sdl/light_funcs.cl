#line 2 "light_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_INFINITELIGHT)

float3 InfiniteLight_GetRadiance(__global LightSource *infiniteLight,
		__global float *infiniteLightDistirbution,
		const float3 dir, float *directPdfA
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersecable.infinite.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float3 localDir = normalize(Transform_InvApplyVector(&infiniteLight->notIntersecable.light2World, -dir));
	const float2 uv = (float2)(
		SphericalPhi(localDir) * (1.f / (2.f * M_PI_F)),
		SphericalTheta(localDir) * M_1_PI_F);

	// TextureMapping2D_Map() is expended here
	const float2 scale = VLOAD2F(&infiniteLight->notIntersecable.infinite.mapping.uvMapping2D.uScale);
	const float2 delta = VLOAD2F(&infiniteLight->notIntersecable.infinite.mapping.uvMapping2D.uDelta);
	const float2 mapUV = uv * scale + delta;

	const float distPdf = Distribution2D_Pdf(infiniteLightDistirbution, mapUV.s0, mapUV.s1);
	*directPdfA = distPdf / (4.f * M_PI_F);

	return VLOAD3F(&infiniteLight->notIntersecable.gain.r) * ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

float3 InfiniteLight_Illuminate(__global LightSource *infiniteLight,
		__global float *infiniteLightDistirbution,
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
	*dir = normalize(Transform_ApplyVector(&infiniteLight->notIntersecable.light2World,
			SphericalDirection(sin(theta), cos(theta), phi)));

	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float worldRadius = PARAM_LIGHT_WORLD_RADIUS_SCALE * sceneRadius * 1.01f;

	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));

	const float3 emisPoint = p + (*distance) * (*dir);
	const float3 emisNormal = normalize(worldCenter - emisPoint);

	const float cosAtLight = dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*directPdfW = distPdf / (4.f * M_PI_F);

	// InfiniteLight_GetRadiance is expended here
	__global ImageMap *imageMap = &imageMapDescs[infiniteLight->notIntersecable.infinite.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = (float2)(sampleUV.s0, sampleUV.s1);

	// TextureMapping2D_Map() is expended here
	const float2 scale = VLOAD2F(&infiniteLight->notIntersecable.infinite.mapping.uvMapping2D.uScale);
	const float2 delta = VLOAD2F(&infiniteLight->notIntersecable.infinite.mapping.uvMapping2D.uDelta);
	const float2 mapUV = uv * scale + delta;
	
	return VLOAD3F(&infiniteLight->notIntersecable.gain.r) * ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

#endif

//------------------------------------------------------------------------------
// SktLight
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

float3 SkyLight_GetSkySpectralRadiance(__global LightSource *skyLight,
		const float theta, const float phi) {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = fmin(theta, (M_PI_F * .5f) - .001f);
	const float gamma = SkyLight_RiAngleBetween(theta, phi, 
			skyLight->notIntersecable.sky.thetaS, skyLight->notIntersecable.sky.phiS);

	// Compute xyY values
	const float x = skyLight->notIntersecable.sky.zenith_x * SkyLight_PerezBase(
			skyLight->notIntersecable.sky.perez_x, theta_fin, gamma);
	const float y = skyLight->notIntersecable.sky.zenith_y * SkyLight_PerezBase(
			skyLight->notIntersecable.sky.perez_y, theta_fin, gamma);
	const float Y = skyLight->notIntersecable.sky.zenith_Y * SkyLight_PerezBase(
			skyLight->notIntersecable.sky.perez_Y, theta_fin, gamma);

	return SkyLight_ChromaticityToSpectrum(Y, x, y);
}

float3 SkyLight_GetRadiance(__global LightSource *skyLight, const float3 dir,
		float *directPdfA) {
	*directPdfA = 1.f / (4.f * M_PI_F);

	const float3 localDir = normalize(Transform_InvApplyVector(&skyLight->notIntersecable.light2World, -dir));
	const float theta = SphericalTheta(localDir);
	const float phi = SphericalPhi(localDir);
	const float3 s = SkyLight_GetSkySpectralRadiance(skyLight, theta, phi);

	return VLOAD3F(&skyLight->notIntersecable.gain.r) * s;
}

float3 SkyLight_Illuminate(__global LightSource *skyLight,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float sceneRadius,
		const float u0, const float u1, const float3 p,
		float3 *dir, float *distance, float *directPdfW) {
	const float3 worldCenter = (float3)(worldCenterX, worldCenterY, worldCenterZ);
	const float worldRadius = PARAM_LIGHT_WORLD_RADIUS_SCALE * sceneRadius * 1.01f;

	const float3 localDir = normalize(Transform_ApplyVector(&skyLight->notIntersecable.light2World, -(*dir)));
	*dir = normalize(Transform_ApplyVector(&skyLight->notIntersecable.light2World,  UniformSampleSphere(u0, u1)));

	const float3 toCenter = worldCenter - p;
	const float centerDistance = dot(toCenter, toCenter);
	const float approach = dot(toCenter, *dir);
	*distance = approach + sqrt(max(0.f, worldRadius * worldRadius -
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
// SunLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT)

float3 SunLight_Illuminate(__global LightSource *sunLight,
		const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float cosThetaMax = sunLight->notIntersecable.sun.cosThetaMax;
	const float3 sunDir = VLOAD3F(&sunLight->notIntersecable.sun.sunDir.x);
	*dir = UniformSampleCone(u0, u1, cosThetaMax, VLOAD3F(&sunLight->notIntersecable.sun.x.x), VLOAD3F(&sunLight->notIntersecable.sun.y.x), sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = dot(sunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return BLACK;

	*distance = INFINITY;
	*directPdfW = UniformConePdf(cosThetaMax);

	return VLOAD3F(&sunLight->notIntersecable.sun.sunColor.r);
}

float3 SunLight_GetRadiance(__global LightSource *sunLight, const float3 dir, float *directPdfA) {
	const float cosThetaMax = sunLight->notIntersecable.sun.cosThetaMax;
	const float3 sunDir = VLOAD3F(&sunLight->notIntersecable.sun.sunDir.x);

	if ((cosThetaMax < 1.f) && (dot(-dir, sunDir) > cosThetaMax)) {
		if (directPdfA)
			*directPdfA = UniformConePdf(cosThetaMax);

		return VLOAD3F(&sunLight->notIntersecable.sun.sunColor.r);
	} else
		return BLACK;
}

#endif

//------------------------------------------------------------------------------
// TriangleLight
//------------------------------------------------------------------------------

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)

float3 TriangleLight_Illuminate(__global LightSource *triLight,
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

	const float3 sampleN = Triangle_GetGeometryNormal(p0, p1, p2); // Light sources are supposed to be flat

	*dir = samplePoint - p;
	const float distanceSquared = dot(*dir, *dir);;
	*distance = sqrt(distanceSquared);
	*dir /= (*distance);

	const float cosAtLight = dot(sampleN, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	float3 emissionColor = WHITE;
#if defined(PARAM_HAS_IMAGEMAPS)
	if (triLight->triangle.imageMapIndex != NULL_INDEX) {
		// Build the local frame
		float3 X, Y;
		CoordinateSystem(sampleN, &X, &Y);

		const float3 localFromLight = ToLocal(X, Y, sampleN, -(*dir));

		// Retrieve the image map information
		__global ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		__global float *pixels = ImageMap_GetPixelsAddress(
				imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);
		const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
		emissionColor = ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			uv.s0, uv.s1) / triLight->triangle.avarage;

		*directPdfW = triLight->triangle.invArea * distanceSquared ;
	} else
#endif
		*directPdfW = triLight->triangle.invArea * distanceSquared / cosAtLight;

	const float2 uv0 = VLOAD2F(&triLight->triangle.uv0.u);
	const float2 uv1 = VLOAD2F(&triLight->triangle.uv1.u);
	const float2 uv2 = VLOAD2F(&triLight->triangle.uv2.u);
	const float2 triUV = Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);

	VSTORE3F(-sampleN, &tmpHitPoint->fixedDir.x);
	VSTORE3F(samplePoint, &tmpHitPoint->p.x);
	VSTORE2F(triUV, &tmpHitPoint->uv.u);
	VSTORE3F(sampleN, &tmpHitPoint->geometryN.x);
	VSTORE3F(sampleN, &tmpHitPoint->shadeN.x);
#if defined(PARAM_HAS_PASSTHROUGH)
	tmpHitPoint->passThroughEvent = passThroughEvent;
#endif

	return Material_GetEmittedRadiance(&mats[triLight->triangle.materialIndex],
			tmpHitPoint, triLight->triangle.invArea
			MATERIALS_PARAM) * emissionColor;
}

float3 TriangleLight_GetRadiance(__global LightSource *triLight,
		 __global HitPoint *hitPoint, float *directPdfA
		MATERIALS_PARAM_DECL) {
	const float3 dir = VLOAD3F(&hitPoint->fixedDir.x);
	const float3 hitPointNormal = VLOAD3F(&hitPoint->geometryN.x);
	const float cosOutLight = dot(hitPointNormal, dir);
	if (cosOutLight <= 0.f)
		return BLACK;

	if (directPdfA)
		*directPdfA = triLight->triangle.invArea;

	float3 emissionColor = WHITE;
#if defined(PARAM_HAS_IMAGEMAPS)
	if (triLight->triangle.imageMapIndex != NULL_INDEX) {
		// Build the local frame
		float3 X, Y;
		CoordinateSystem(hitPointNormal, &X, &Y);

		const float3 localFromLight = ToLocal(X, Y, hitPointNormal, dir);

		// Retrieve the image map information
		__global ImageMap *imageMap = &imageMapDescs[triLight->triangle.imageMapIndex];
		__global float *pixels = ImageMap_GetPixelsAddress(
				imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);
		const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
		emissionColor = ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			uv.s0, uv.s1) / triLight->triangle.avarage;
	}
#endif

	return Material_GetEmittedRadiance(&mats[triLight->triangle.materialIndex],
			hitPoint, triLight->triangle.invArea
			MATERIALS_PARAM) * emissionColor;
}

#endif

//------------------------------------------------------------------------------
// PointLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_POINTLIGHT)

float3 PointLight_Illuminate(__global LightSource *pointLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW) {
	const float3 toLight = VLOAD3F(&pointLight->notIntersecable.point.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	*directPdfW = distanceSquared;

	return VLOAD3F(&pointLight->notIntersecable.point.emittedFactor.r);
}

#endif

//------------------------------------------------------------------------------
// MapPointLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_MAPPOINTLIGHT)

float3 MapPointLight_Illuminate(__global LightSource *mapPointLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 toLight = VLOAD3F(&mapPointLight->notIntersecable.mapPoint.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	*directPdfW = distanceSquared;

	// Retrieve the image map information
	__global ImageMap *imageMap = &imageMapDescs[mapPointLight->notIntersecable.mapPoint.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float3 localFromLight = normalize(Transform_InvApplyVector(&mapPointLight->notIntersecable.light2World, p) - 
		VLOAD3F(&mapPointLight->notIntersecable.mapPoint.localPos.x));
	const float2 uv = (float2)(SphericalPhi(localFromLight) * (1.f / (2.f * M_PI_F)), SphericalTheta(localFromLight) * M_1_PI_F);
	const float3 emissionColor = ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			uv.s0, uv.s1) / mapPointLight->notIntersecable.mapPoint.avarage;

	return VLOAD3F(&mapPointLight->notIntersecable.mapPoint.emittedFactor.r) * emissionColor;
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

float3 SpotLight_Illuminate(__global LightSource *spotLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW) {
	const float3 toLight = VLOAD3F(&spotLight->notIntersecable.spot.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	const float3 localFromLight = normalize(Matrix4x4_ApplyVector(
			&spotLight->notIntersecable.spot.alignedWorld2Light, -(*dir)));
	const float falloff = SpotLight_LocalFalloff(localFromLight,
			spotLight->notIntersecable.spot.cosTotalWidth,
			spotLight->notIntersecable.spot.cosFalloffStart);
	if (falloff == 0.f)
		return BLACK;

	*directPdfW = distanceSquared;

	return VLOAD3F(&spotLight->notIntersecable.spot.emittedFactor.r) *
			(falloff / fabs(CosTheta(localFromLight)));
}

#endif

//------------------------------------------------------------------------------
// ProjectionLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_PROJECTIONLIGHT)

float3 ProjectionLight_Illuminate(__global LightSource *projectionLight,
		const float3 p,	float3 *dir, float *distance, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	const float3 toLight = VLOAD3F(&projectionLight->notIntersecable.projection.absolutePos.x) - p;
	const float distanceSquared = dot(toLight, toLight);
	*distance = sqrt(distanceSquared);
	*dir = toLight / *distance;

	// Check the side
	if (dot(-(*dir), VLOAD3F(&projectionLight->notIntersecable.projection.lightNormal.x)) < 0.f)
		return BLACK;

	// Check if the point is inside the image plane
	const float3 localFromLight = normalize(Matrix4x4_ApplyVector(
			&projectionLight->notIntersecable.light2World.mInv, -(*dir)));
	const float3 p0 = Matrix4x4_ApplyPoint(
			&projectionLight->notIntersecable.projection.lightProjection, localFromLight);

	const float screenX0 = projectionLight->notIntersecable.projection.screenX0;
	const float screenX1 = projectionLight->notIntersecable.projection.screenX1;
	const float screenY0 = projectionLight->notIntersecable.projection.screenY0;
	const float screenY1 = projectionLight->notIntersecable.projection.screenY1;
	if ((p0.x < screenX0) || (p0.x >= screenX1) || (p0.y < screenY0) || (p0.y >= screenY1))
		return BLACK;

	*directPdfW = distanceSquared;

	float3 c = VLOAD3F(&projectionLight->notIntersecable.gain.r) * VLOAD3F(&projectionLight->notIntersecable.projection.color.r);
	const uint imageMapIndex = projectionLight->notIntersecable.projection.imageMapIndex;
	if (imageMapIndex != NULL_INDEX) {
		const float u = (p0.x - screenX0) / (screenX1 - screenX0);
		const float v = (p0.y - screenY0) / (screenY1 - screenY0);
		
		// Retrieve the image map information
		__global ImageMap *imageMap = &imageMapDescs[imageMapIndex];
		__global float *pixels = ImageMap_GetPixelsAddress(
				imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

		c *= ImageMap_GetSpectrum(
				pixels,
				imageMap->width, imageMap->height, imageMap->channelCount,
				u, v);
	}

	return c;
}

#endif

//------------------------------------------------------------------------------
// Generic light functions
//------------------------------------------------------------------------------

float3 EnvLight_GetRadiance(__global LightSource *light, const float3 dir, float *directPdfA
				LIGHTS_PARAM_DECL) {
	switch (light->type) {
#if defined(PARAM_HAS_INFINITELIGHT)
		case TYPE_IL:
			return InfiniteLight_GetRadiance(light,
					&infiniteLightDistribution[light->notIntersecable.infinite.distributionOffset],
					dir, directPdfA
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		case TYPE_IL_SKY:
			return SkyLight_GetRadiance(light,
					dir, directPdfA);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
			return SunLight_GetRadiance(light,
					dir, directPdfA);
#endif
		default:
			return BLACK;
	}
}

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
float3 IntersecableLight_GetRadiance(__global LightSource *light,
		 __global HitPoint *hitPoint, float *directPdfA
		LIGHTS_PARAM_DECL) {
	return TriangleLight_GetRadiance(light, hitPoint, directPdfA
			MATERIALS_PARAM);
}
#endif

float3 Light_Illuminate(
		__global LightSource *light,
		const float3 point,
		const float u0, const float u1, const float u2,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u3,
#endif
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		float3 *lightRayDir, float *distance, float *directPdfW
		LIGHTS_PARAM_DECL) {
	switch (light->type) {
#if defined(PARAM_HAS_INFINITELIGHT)
		case TYPE_IL:
			return InfiniteLight_Illuminate(
				light,
				&infiniteLightDistribution[light->notIntersecable.infinite.distributionOffset],
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
				u0, u1,
				point,
				lightRayDir, distance, directPdfW
				IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		case TYPE_IL_SKY:
			return SkyLight_Illuminate(
				light,
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
				u0, u1,
				point,
				lightRayDir, distance, directPdfW);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		case TYPE_SUN:
			return SunLight_Illuminate(
				light,
				u0, u1,
				lightRayDir, distance, directPdfW);
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		case TYPE_TRIANGLE:
			return TriangleLight_Illuminate(
					light,
					tmpHitPoint,
					point,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					u3,
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
		default:
			return BLACK;
	}
}
