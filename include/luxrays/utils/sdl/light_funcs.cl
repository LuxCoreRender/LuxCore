#line 2 "light_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_INFINITELIGHT)

float3 InfiniteLight_GetRadiance(
	__global InfiniteLight *infiniteLight, const float3 dir
	IMAGEMAPS_PARAM_DECL) {
	const float2 uv = (float2)(
		1.f - SphericalPhi(-dir) * (1.f / (2.f * M_PI_F))+ infiniteLight->shiftU,
		SphericalTheta(-dir) * M_1_PI_F + infiniteLight->shiftV);

	return VLOAD3F(&infiniteLight->gain.r) * ImageMapInstance_GetColor(
			&infiniteLight->imageMapInstance, uv
			IMAGEMAPS_PARAM);
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

float3 SkyLight_GetSkySpectralRadiance(__global SkyLight *skyLight,
		const float theta, const float phi) {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = fmin(theta, (M_PI_F * .5f) - .001f);
	const float gamma = SkyLight_RiAngleBetween(theta, phi, skyLight->thetaS, skyLight->phiS);

	// Compute xyY values
	const float x = skyLight->zenith_x * SkyLight_PerezBase(skyLight->perez_x, theta_fin, gamma);
	const float y = skyLight->zenith_y * SkyLight_PerezBase(skyLight->perez_y, theta_fin, gamma);
	const float Y = skyLight->zenith_Y * SkyLight_PerezBase(skyLight->perez_Y, theta_fin, gamma);

	return SkyLight_ChromaticityToSpectrum(Y, x, y);
}

float3 SkyLight_GetRadiance(__global SkyLight *skyLight, const float3 dir) {
	const float theta = SphericalTheta(-dir);
	const float phi = SphericalPhi(-dir);
	const float3 s = SkyLight_GetSkySpectralRadiance(skyLight, theta, phi);

	return VLOAD3F(&skyLight->gain.r) * s;
}

#endif

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT)

float3 SunLight_Illuminate(__global SunLight *sunLight,
		const float u0, const float u1,
		float3 *dir, float *distance, float *directPdfW) {
	const float cosThetaMax = sunLight->cosThetaMax;
	const float3 sunDir = VLOAD3F(&sunLight->sunDir.x);
	*dir = UniformSampleCone(u0, u1, cosThetaMax, VLOAD3F(&sunLight->x.x), VLOAD3F(&sunLight->y.x), sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = dot(sunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return BLACK;

	*distance = INFINITY;
	*directPdfW = UniformConePdf(cosThetaMax);

	return VLOAD3F(&sunLight->sunColor.r);
}

float3 SunLight_GetRadiance(__global SunLight *sunLight, const float3 dir, float *directPdfA) {
	// Make the sun visible only if relsize has been changed (in order
	// to avoid fireflies).
	if (sunLight->relSize > 5.f) {
		const float cosThetaMax = sunLight->cosThetaMax;
		const float3 sunDir = VLOAD3F(&sunLight->sunDir.x);

		if ((cosThetaMax < 1.f) && (dot(-dir, sunDir) > cosThetaMax)) {
			if (directPdfA)
				*directPdfA = UniformConePdf(cosThetaMax);

			return VLOAD3F(&sunLight->sunColor.r);
		}
	}

	return BLACK;
}

#endif

//------------------------------------------------------------------------------
// TriangleLight
//------------------------------------------------------------------------------

float3 TriangleLight_Illuminate(__global TriangleLight *triLight,
		const float3 p, const float u0, const float u1, const float u2,
		float3 *dir, float *distance, float *directPdfW
		MATERIALS_PARAM_DECL
		IMAGEMAPS_PARAM_DECL) {
	const float3 p0 = VLOAD3F(&triLight->v0.x);
	const float3 p1 = VLOAD3F(&triLight->v1.x);
	const float3 p2 = VLOAD3F(&triLight->v2.x);
	float b0, b1, b2;
	float3 samplePoint = Triangle_Sample(
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

	*directPdfW = triLight->invArea * distanceSquared / cosAtLight;

	const float2 uv0 = VLOAD2F(&triLight->uv0.u);
	const float2 uv1 = VLOAD2F(&triLight->uv1.u);
	const float2 uv2 = VLOAD2F(&triLight->uv2.u);
	const float2 triUV = Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);

	return Material_GetEmittedRadiance(&mats[triLight->materialIndex], triUV
			MATERIALS_PARAM
			IMAGEMAPS_PARAM);
}

float3 TriangleLight_GetRadiance(__global TriangleLight *triLight,
		__global Material *mats, __global Texture *texs, 
		const float3 dir, const float3 hitPointNormal,
		const float2 triUV, float *directPdfA
		IMAGEMAPS_PARAM_DECL) {
	const float cosOutLight = dot(hitPointNormal, dir);
	if (cosOutLight <= 0.f)
		return BLACK;

	if (directPdfA)
		*directPdfA = triLight->invArea;

	return Material_GetEmittedRadiance(&mats[triLight->materialIndex], triUV
			MATERIALS_PARAM
			IMAGEMAPS_PARAM);
}
