#line 2 "material_funcs.cl"

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
// Generic material related functions
//------------------------------------------------------------------------------

float SchlickDistribution_SchlickZ(const float roughness, float cosNH) {
	const float d = 1.f + (roughness - 1) * cosNH * cosNH;
	return (roughness > 0.f) ? (roughness / (d * d)) : INFINITY;
}

float SchlickDistribution_SchlickA(const float3 H, const float anisotropy) {
	const float h = sqrt(H.x * H.x + H.y * H.y);
	if (h > 0.f) {
		const float w = (anisotropy > 0.f ? H.x : H.y) / h;
		const float p = 1.f - fabs(anisotropy);
		return sqrt(p / (p * p + w * w * (1.f - p * p)));
	}

	return 1.f;
}

float SchlickDistribution_D(const float roughness, const float3 wh, const float anisotropy) {
	const float cosTheta = fabs(wh.z);
	return SchlickDistribution_SchlickZ(roughness, cosTheta) * SchlickDistribution_SchlickA(wh, anisotropy) * M_1_PI_F;
}

float SchlickDistribution_SchlickG(const float roughness, const float costheta) {
	return costheta / (costheta * (1.f - roughness) + roughness);
}

float SchlickDistribution_G(const float roughness, const float3 fixedDir, const float3 sampledDir) {
	return SchlickDistribution_SchlickG(roughness, fabs(fixedDir.z)) *
			SchlickDistribution_SchlickG(roughness, fabs(sampledDir.z));
}

float GetPhi(const float a, const float b) {
	return M_PI_F * .5f * sqrt(a * b / (1.f - a * (1.f - b)));
}

void SchlickDistribution_SampleH(const float roughness, const float anisotropy,
		const float u0, const float u1, float3 *wh, float *d, float *pdf) {
	float u1x4 = u1 * 4.f;
	const float cos2Theta = u0 / (roughness * (1 - u0) + u0);
	const float cosTheta = sqrt(cos2Theta);
	const float sinTheta = sqrt(1.f - cos2Theta);
	const float p = 1.f - fabs(anisotropy);
	float phi;
	if (u1x4 < 1.f) {
		phi = GetPhi(u1x4 * u1x4, p * p);
	} else if (u1x4 < 2.f) {
		u1x4 = 2.f - u1x4;
		phi = M_PI_F - GetPhi(u1x4 * u1x4, p * p);
	} else if (u1x4 < 3.f) {
		u1x4 -= 2.f;
		phi = M_PI_F + GetPhi(u1x4 * u1x4, p * p);
	} else {
		u1x4 = 4.f - u1x4;
		phi = M_PI_F * 2.f - GetPhi(u1x4 * u1x4, p * p);
	}

	if (anisotropy > 0.f)
		phi += M_PI_F * .5f;

	*wh = (float3)(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	*d = SchlickDistribution_SchlickZ(roughness, cosTheta) * SchlickDistribution_SchlickA(*wh, anisotropy) * M_1_PI_F;
	*pdf = *d;
}

float SchlickDistribution_Pdf(const float roughness, const float3 wh,
		const float anisotropy) {
	return SchlickDistribution_D(roughness, wh, anisotropy);
}

float3 FresnelSlick_Evaluate(const float3 normalIncidence, const float cosi) {
	return normalIncidence + (WHITE - normalIncidence) *
		pow(1.f - cosi, 5.f);
}

float3 FrFull(const float cosi, const float3 cost, const float3 eta, const float3 k) {
	const float3 tmp = (eta * eta + k * k) * (cosi * cosi) + (cost * cost);
	const float3 Rparl2 = (tmp - (2.f * cosi * cost) * eta) /
		(tmp + (2.f * cosi * cost) * eta);
	const float3 tmp_f = (eta * eta + k * k) * (cost * cost) + (cosi * cosi);
	const float3 Rperp2 = (tmp_f - (2.f * cosi * cost) * eta) /
		(tmp_f + (2.f * cosi * cost) * eta);
	return (Rparl2 + Rperp2) * 0.5f;
}

float3 FresnelGeneral_Evaluate(const float3 eta, const float3 k, const float cosi) {
	float3 sint2 = fmax(0.f, 1.f - cosi * cosi);
	if (cosi > 0.f)
		sint2 /= eta * eta;
	else
		sint2 *= eta * eta;
	sint2 = Spectrum_Clamp(sint2);

	const float3 cost2 = 1.f - sint2;
	if (cosi > 0.f) {
		const float3 a = 2.f * k * k * sint2;
		return FrFull(cosi, Spectrum_Sqrt((cost2 + Spectrum_Sqrt(cost2 * cost2 + a * a)) / 2.f), eta, k);
	} else {
		const float3 a = 2.f * k * k * sint2;
		const float3 d2 = eta * eta + k * k;
		return FrFull(-cosi, Spectrum_Sqrt((cost2 + Spectrum_Sqrt(cost2 * cost2 + a * a)) / 2.f), eta / d2, -k / d2);
	}
}

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTE)

float3 MatteMaterial_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetColorValue(&texs[material->matte.kdTexIndex], uv
			TEXTURES_PARAM));
	return M_1_PI_F * kd;
}

float3 MatteMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetColorValue(&texs[material->matte.kdTexIndex], uv
			TEXTURES_PARAM));
	return M_1_PI_F * kd;
}

#endif

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIRROR)

float3 MirrorMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	const float3 kr = Spectrum_Clamp(Texture_GetColorValue(&texs[material->mirror.krTexIndex], uv
			TEXTURES_PARAM));
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return kr / (*cosSampledDir);
}

#endif

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLASS)

float3 GlassMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;
	const float3 reflDir = rayDir - (2.f * dot(N, rayDir)) * N;

	const float nc = Texture_GetGreyValue(&texs[material->glass.ousideIorTexIndex], uv
			TEXTURES_PARAM);
	const float nt = Texture_GetGreyValue(&texs[material->glass.iorTexIndex], uv
			TEXTURES_PARAM);
	const float nnt = into ? (nc / nt) : (nt / nc);
	const float nnt2 = nnt * nnt;
	const float ddn = dot(rayDir, shadeN);
	const float cos2t = 1.f - nnt2 * (1.f - ddn * ddn);

	// Total internal reflection
	if (cos2t < 0.f) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		const float3 kr = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
				TEXTURES_PARAM));
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	}

	const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(nnt * rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float a = nt - nc;
	const float b = nt + nc;
	const float R0 = a * a / (b * b);
	const float Re = R0 + (1.f - R0) * c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return BLACK;
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabs((*sampledDir).z);
			*pdfW = 1.f;

			const float3 kr = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
					TEXTURES_PARAM));
			// The cosSampledDir is used to compensate the other one used inside the integrator
			return kr / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		// The cosSampledDir is used to compensate the other one used inside the integrator
//		if (fromLight)
//			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
//		else
//			return Kt->GetColorValue(uv) / (*cosSampledDir);
		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glass.ktTexIndex], uv
				TEXTURES_PARAM));
		return kt / (*cosSampledDir);
	} else if (passThroughEvent < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = P / Re;

		const float3 kr = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
				TEXTURES_PARAM));
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = (1.f - P) / Tr;

		// The cosSampledDir is used to compensate the other one used inside the integrator
//		if (fromLight)
//			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
//		else
//			return Kt->GetColorValue(uv) / (*cosSampledDir);
		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glass.ktTexIndex], uv
				TEXTURES_PARAM));
		return kt / (*cosSampledDir);
	}
}

#endif

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_METAL)

float3 GlossyReflection(const float3 fixedDir, const float exponent,
		const float u0, const float u1) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);

	const float phi = 2.f * M_PI * u0;
	const float cosTheta = pow(1.f - u1, exponent);
	const float sinTheta = sqrt(fmax(0.f, 1.f - cosTheta * cosTheta));
	const float x = cos(phi) * sinTheta;
	const float y = sin(phi) * sinTheta;
	const float z = cosTheta;

	const float3 dir = -fixedDir;
	const float dp = dot(shadeN, dir);
	const float3 w = dir - (2.f * dp) * shadeN;

	const float3 u = normalize(cross(
			(fabs(shadeN.x) > .1f) ? ((float3)(0.f, 1.f, 0.f)) : ((float3)(1.f, 0.f, 0.f)),
			w));
	const float3 v = cross(w, u);

	return x * u + y * v + z * w;
}

float3 MetalMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	const float e = 1.f / (Texture_GetGreyValue(&texs[material->metal.expTexIndex], uv
				TEXTURES_PARAM) + 1.f);
	*sampledDir = GlossyReflection(fixedDir, e, u0, u1);

	if ((*sampledDir).z * fixedDir.z > 0.f) {
		*event = SPECULAR | REFLECT;
		*pdfW = 1.f;
		*cosSampledDir = fabs((*sampledDir).z);

		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->metal.krTexIndex], uv
				TEXTURES_PARAM));
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	} else
		return BLACK;
}

#endif

//------------------------------------------------------------------------------
// ArchGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ARCHGLASS)

float3 ArchGlassMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;
	const float3 reflDir = rayDir - (2.f * dot(N, rayDir)) * N;

	const float ddn = dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return BLACK;
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabs((*sampledDir).z);
			*pdfW = 1.f;

			const float3 kr = Spectrum_Clamp(Texture_GetColorValue(&texs[material->archglass.krTexIndex], uv
					TEXTURES_PARAM));
			// The cosSampledDir is used to compensate the other one used inside the integrator
			return kr / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				TEXTURES_PARAM));
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	} else if (passThroughEvent < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = P / Re;

		const float3 kr = Spectrum_Clamp(Texture_GetColorValue(&texs[material->archglass.krTexIndex], uv
				TEXTURES_PARAM));
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = (1.f - P) / Tr;

		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				TEXTURES_PARAM));
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	}
}

float3 ArchGlassMaterial_GetPassThroughTransparency(__global Material *material,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;

	const float ddn = dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f)
		return BLACK;
	else if (Re == 0.f) {
		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				TEXTURES_PARAM));
		return kt;
	} else if (passThroughEvent < P)
		return BLACK;
	else {
		const float3 kt = Spectrum_Clamp(Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				TEXTURES_PARAM));
		return kt;
	}
}

#endif

//------------------------------------------------------------------------------
// NULL material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_NULL)

float3 NullMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	*sampledDir = -fixedDir;
	*cosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return WHITE;
}

#endif

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)

float3 MatteTranslucentMaterial_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float cosSampledDir = dot(lightDir, eyeDir);

	const float3 r = Spectrum_Clamp(Texture_GetColorValue(&texs[material->matteTranslucent.krTexIndex], uv
			TEXTURES_PARAM));
	const float3 t = Spectrum_Clamp(Texture_GetColorValue(&texs[material->matteTranslucent.ktTexIndex], uv
			TEXTURES_PARAM)) * 
		// Energy conservation
		(1.f - r);

	if (directPdfW)
		*directPdfW = .5f * fabs(lightDir.z * M_1_PI_F);

	if (cosSampledDir > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * M_1_PI_F;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * M_1_PI_F;
	}
}

float3 MatteTranslucentMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, pdfW);
	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*pdfW *= .5f;

	const float3 r = Spectrum_Clamp(Texture_GetColorValue(&texs[material->matteTranslucent.krTexIndex], uv
			TEXTURES_PARAM));
	const float3 t = Spectrum_Clamp(Texture_GetColorValue(&texs[material->matteTranslucent.ktTexIndex], uv
			TEXTURES_PARAM)) * 
		// Energy conservation
		(1.f - r);

	if (passThroughEvent < .5f) {
		*sampledDir *= (signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | REFLECT;
		return r * M_1_PI_F;
	} else {
		*sampledDir *= -(signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | TRANSMIT;
		return t * M_1_PI_F;
	}
}

#endif

//------------------------------------------------------------------------------
// Glossy2 material
//
// LuxRender Glossy2 material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLOSSY2)

float SchlickBSDF_CoatingWeight(const float3 ks, const float3 fixedDir) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return 0.f;

	// Approximate H by using reflection direction for wi
	const float u = fabs(fixedDir.z);
	const float3 S = FresnelSlick_Evaluate(ks, u);

	// Ensures coating is never sampled less than half the time
	// unless we are on the back face
	return .5f * (1.f + Spectrum_Filter(S));
}

float3 SchlickBSDF_CoatingF(const float3 ks, const float roughness,
		const float anisotropy, const int multibounce, const float3 fixedDir,
		const float3 sampledDir) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return BLACK;

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs(sampledDir.z);

	const float3 wh = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSlick_Evaluate(ks, fabs(dot(sampledDir, wh)));

	const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);

	// Multibounce - alternative with interreflection in the coating creases
	const float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * cosi) + 
		(multibounce ? coso * clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);

	return factor * S;
}

float3 SchlickBSDF_CoatingAbsorption(const float cosi, const float coso,
		const float3 alpha, const float depth) {
	if (depth > 0.f) {
		// 1/cosi+1/coso=(cosi+coso)/(cosi*coso)
		const float depthFactor = depth * (cosi + coso) / (cosi * coso);
		return Spectrum_Exp(alpha * -depthFactor);
	} else
		return WHITE;
}

float3 SchlickBSDF_CoatingSampleF(const float3 ks,
		const float roughness, const float anisotropy, const int multibounce,
		const float3 fixedDir, float3 *sampledDir,
		float u0, float u1, float *pdf) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return BLACK;

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = dot(fixedDir, wh);
	*sampledDir = 2.f * cosWH * wh - fixedDir;

	if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * (*sampledDir).z < 0.f))
		return BLACK;

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs((*sampledDir).z);

	*pdf = specPdf / (4.f * cosWH);
	if (*pdf <= 0.f)
		return BLACK;

	float3 S = FresnelSlick_Evaluate(ks, cosWH);

	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);

	//CoatingF(sw, *wi, wo, f_);
	S *= d * G / (4.f * coso) + 
			(multibounce ? cosi * clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);

	return S;
}

float SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
		const float3 fixedDir, const float3 sampledDir) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return 0.f;

	const float3 wh = normalize(fixedDir + sampledDir);
	return SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * fabs(dot(fixedDir, wh)));
}

float3 Glossy2Material_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float3 baseF = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glossy2.kdTexIndex], uv
			TEXTURES_PARAM)) * M_1_PI_F;
	if (eyeDir.z <= 0.f) {
		// Back face: no coating

		if (directPdfW)
			*directPdfW = fabs(sampledDir.z * M_1_PI_F);

		*event = DIFFUSE | REFLECT;
		return baseF;
	}

	// Front face: coating+base
	*event = GLOSSY | REFLECT;

	float3 ks = Texture_GetColorValue(&texs[material->glossy2.ksTexIndex], uv
			TEXTURES_PARAM);
	const float i = Texture_GetGreyValue(&texs[material->glossy2.indexTexIndex], uv
			TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	const float u = clamp(Texture_GetGreyValue(&texs[material->glossy2.nuTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float v = clamp(Texture_GetGreyValue(&texs[material->glossy2.nvTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	}

	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);
	const float3 alpha = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glossy2.kaTexIndex], uv
			TEXTURES_PARAM));
	const float d = Texture_GetGreyValue(&texs[material->glossy2.depthTexIndex], uv
			TEXTURES_PARAM);
	const float3 absorption = SchlickBSDF_CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSlick_Evaluate(ks, fabs(dot(sampledDir, H)));

	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, material->glossy2.multibounce,
			fixedDir, sampledDir);

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	// The cosi is used to compensate the other one used inside the integrator
	return coatingF / cosi + absorption * (WHITE - S) * baseF;
}

float3 Glossy2Material_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	float3 ks = Texture_GetColorValue(&texs[material->glossy2.ksTexIndex], uv
			TEXTURES_PARAM);
	const float i = Texture_GetGreyValue(&texs[material->glossy2.indexTexIndex], uv
			TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	const float u = clamp(Texture_GetGreyValue(&texs[material->glossy2.nuTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float v = clamp(Texture_GetGreyValue(&texs[material->glossy2.nvTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	// Coating is used only on the front face
	const float wCoating = (fixedDir.z <= 0.f) ? 0.f : SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;

	float basePdf, coatingPdf;
	float3 baseF, coatingF;

	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		baseF = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glossy2.kdTexIndex], uv
			TEXTURES_PARAM)) * M_1_PI_F;

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, material->glossy2.multibounce,
				fixedDir, *sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);

		*event = DIFFUSE | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				material->glossy2.multibounce, fixedDir, sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF))
			return BLACK;

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = fabs((*sampledDir).z * M_1_PI_F);
		baseF = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glossy2.kdTexIndex], uv
			TEXTURES_PARAM)) * M_1_PI_F;

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;
	if (fixedDir.z > 0.f) {
		// Front face reflection: coating+base

		// Absorption
		const float cosi = fabs((*sampledDir).z);
		const float coso = fabs(fixedDir.z);
		const float3 alpha = Spectrum_Clamp(Texture_GetColorValue(&texs[material->glossy2.kaTexIndex], uv
			TEXTURES_PARAM));
		const float d = Texture_GetGreyValue(&texs[material->glossy2.depthTexIndex], uv
			TEXTURES_PARAM);
		const float3 absorption = SchlickBSDF_CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + *sampledDir);
		const float3 S = FresnelSlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

		// Blend in base layer Schlick style
		// coatingF already takes fresnel factor S into account

		// The cosi is used to compensate the other one used inside the integrator
		return coatingF / cosi + absorption * (WHITE - S) * baseF;
	} else {
		// Back face reflection: base

		return baseF;
	}
}

#endif

//------------------------------------------------------------------------------
// Metal2 material
//
// LuxRender Metal2 material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_METAL2)

float3 Metal2Material_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float u = clamp(Texture_GetGreyValue(&texs[material->metal2.nuTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float v = clamp(Texture_GetGreyValue(&texs[material->metal2.nvTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const float3 wh = normalize(fixedDir + sampledDir);
	const float cosWH = dot(fixedDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * fabs(dot(fixedDir, wh)));

	const float3 nVal = Texture_GetColorValue(&texs[material->metal2.nTexIndex], uv
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetColorValue(&texs[material->metal2.kTexIndex], uv
			TEXTURES_PARAM);

	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);

	const float cosi = fabs(sampledDir.z);
	const float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * cosi);

	*event = GLOSSY | REFLECT;

	// The cosi is used to compensate the other one used inside the integrator
	return (factor / cosi) * F;
}

float3 Metal2Material_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float u = clamp(Texture_GetGreyValue(&texs[material->metal2.nuTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float v = clamp(Texture_GetGreyValue(&texs[material->metal2.nvTexIndex], uv
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = dot(fixedDir, wh);
	*sampledDir = 2.f * cosWH * wh - fixedDir;

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs((*sampledDir).z);
	*cosSampledDir = cosi;
	if ((*cosSampledDir < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * (*sampledDir).z < 0.f))
		return BLACK;

	*pdfW = specPdf / (4.f * cosWH);
	if (*pdfW <= 0.f)
		return BLACK;

	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);
	
	const float3 nVal = Texture_GetColorValue(&texs[material->metal2.nTexIndex], uv
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetColorValue(&texs[material->metal2.kTexIndex], uv
			TEXTURES_PARAM);
	float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	const float factor = d * G;
	F *= factor;
	//if (!fromLight)
		F /= 4.f * coso;
	//else
	//	F /= cosi;

	*event = GLOSSY | REFLECT;
	// The cosi is used to compensate the other one used inside the integrator
	return F / cosi;
}

#endif

//------------------------------------------------------------------------------
// Generic material functions
//
// They include the support for all material but Mix
// (because OpenCL doesn't support recursion)
//------------------------------------------------------------------------------

bool Material_IsDeltaNoMix(__global Material *material) {
	switch (material->type) {
		// Non Specular materials
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
#endif
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return false;
#endif
		// Specular materials
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
#endif
		default:
			return true;
	}
}

BSDFEvent Material_GetEventTypesNoMix(__global Material *mat) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return DIFFUSE | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return SPECULAR | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return SPECULAR | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
			return SPECULAR | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return SPECULAR | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return SPECULAR | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return DIFFUSE | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return DIFFUSE | GLOSSY | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return GLOSSY | REFLECT;
#endif
		default:
			return NONE;
	}
}

float3 Material_SampleNoMix(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1, pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
			return MetalMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1, pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	passThroughEvent, pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return Metal2Material_Sample(material, uv, fixedDir, sampledDir,
					u0, u1,	pdfW, cosSampledDir, event
					TEXTURES_PARAM);
#endif
		default:
			return BLACK;
	}
}

float3 Material_EvaluateNoMix(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Evaluate(material, uv, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Evaluate(material, uv, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_Evaluate(material, uv, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return Metal2Material_Evaluate(material, uv, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
#endif
		default:
			return BLACK;
	}
}

float3 Material_GetEmittedRadianceNoMix(__global Material *material, const float2 uv
		TEXTURES_PARAM_DECL) {
	const uint emitTexIndex = material->emitTexIndex;
	if (emitTexIndex == NULL_INDEX)
		return BLACK;

	return Texture_GetColorValue(&texs[emitTexIndex], uv
			TEXTURES_PARAM);
}

float3 Material_GetPassThroughTransparencyNoMix(__global Material *material,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_GetPassThroughTransparency(material,
					uv, fixedDir, passThroughEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return WHITE;
#endif
		default:
			return BLACK;
	}
}

//------------------------------------------------------------------------------
// Mix material
//
// This requires a quite complex implementation because OpenCL doesn't support
// recursion.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIX)

#define MIX_STACK_SIZE 16

BSDFEvent MixMaterial_IsDelta(__global Material *material
		MATERIALS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	materialStack[0] = material;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			materialStack[++stackIndex] = &mats[m->mix.matBIndex];
		} else {
			// Normal GetEventTypes() evaluation
			if (!Material_IsDeltaNoMix(m))
				return false;
		}
	}

	return true;
}

BSDFEvent MixMaterial_GetEventTypes(__global Material *material
		MATERIALS_PARAM_DECL) {
	BSDFEvent event = NONE;
	__global Material *materialStack[MIX_STACK_SIZE];
	materialStack[0] = material;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			materialStack[++stackIndex] = &mats[m->mix.matBIndex];
		} else {
			// Normal GetEventTypes() evaluation
			event |= Material_GetEventTypesNoMix(m);
		}
	}

	return event;
}

float3 MixMaterial_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	float totalWeightStack[MIX_STACK_SIZE];

	// Push the root Mix material
	materialStack[0] = material;
	totalWeightStack[0] = 1.f;
	int stackIndex = 0;

	// Setup the results
	float3 result = BLACK;
	*event = NONE;
	if (directPdfW)
		*directPdfW = 0.f;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex];
		float totalWeight = totalWeightStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			const float factor = Texture_GetGreyValue(&texs[m->mix.mixFactorTexIndex], uv
					TEXTURES_PARAM);
			const float weight2 = clamp(factor, 0.f, 1.f);
			const float weight1 = 1.f - weight2;

			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			totalWeightStack[stackIndex] = totalWeight * weight1;

			materialStack[++stackIndex] = &mats[m->mix.matBIndex];			
			totalWeightStack[stackIndex] = totalWeight * weight2;
		} else {
			// Normal Evaluate() evaluation
			if (totalWeight > 0.f) {
				BSDFEvent eventMat;
				float directPdfWMat;
				const float3 resultMat = Material_EvaluateNoMix(m, uv, lightDir, eyeDir, &eventMat, &directPdfWMat
						TEXTURES_PARAM);

				if (!Spectrum_IsBlack(resultMat)) {
					*event |= eventMat;
					result += totalWeight * resultMat;

					if (directPdfW)
						*directPdfW += totalWeight * directPdfWMat;
				}
			}
		}
	}

	return result;
}

float3 MixMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	__global Material *evaluationMatList[MIX_STACK_SIZE];
	float parentWeightList[MIX_STACK_SIZE];
	int evaluationListSize = 0;

	// Setup the results
	float3 result = BLACK;
	*pdfW = 0.f;

	// Look for a no Mix material to sample
	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	float parentWeight = 1.f;
	for (;;) {
		const float factor = Texture_GetGreyValue(&texs[currentMixMat->mix.mixFactorTexIndex], uv
			TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);

		const float weightFirst = sampleMatA ? weight1 : weight2;
		const float weightSecond = sampleMatA ? weight2 : weight1;

		const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

		const uint matIndexFirst = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		const uint matIndexSecond = sampleMatA ? currentMixMat->mix.matBIndex : currentMixMat->mix.matAIndex;

		// Sample the first material, evaluate the second
		__global Material *matFirst = &mats[matIndexFirst];
		__global Material *matSecond = &mats[matIndexSecond];

		//----------------------------------------------------------------------
		// Add the second material to the evaluation list
		//----------------------------------------------------------------------

		if (weightSecond > 0.f) {
			evaluationMatList[evaluationListSize] = matSecond;
			parentWeightList[evaluationListSize++] = parentWeight * weightSecond;
		}

		//----------------------------------------------------------------------
		// Sample the first material
		//----------------------------------------------------------------------

		// Check if it is a Mix material too
		if (matFirst->type == MIX) {
			// Make the first material the current
			currentMixMat = matFirst;
			passThroughEvent = passThroughEventFirst;
			parentWeight *= weightFirst;
		} else {
			// Sample the first material
			float pdfWMatFirst;
			const float3 sampleResult = Material_SampleNoMix(matFirst, uv,
					fixedDir, sampledDir,
					u0, u1, passThroughEventFirst,
					&pdfWMatFirst, cosSampledDir, event
					TEXTURES_PARAM);

			if (all(isequal(sampleResult, BLACK)))
				return BLACK;

			const float weight = parentWeight * weightFirst;
			*pdfW += weight * pdfWMatFirst;
			result += weight * sampleResult;

			// I can stop now
			break;
		}
	}

	while (evaluationListSize > 0) {
		// Extract the material to evaluate
		__global Material *evalMat = evaluationMatList[--evaluationListSize];
		const float evalWeight = parentWeightList[evaluationListSize];

		// Evaluate the material

		// Check if it is a Mix material too
		BSDFEvent eventMat;
		float pdfWMat;
		float3 eval;
		if (evalMat->type == MIX) {
			eval = MixMaterial_Evaluate(evalMat, uv, *sampledDir, fixedDir,
					&eventMat, &pdfWMat
					MATERIALS_PARAM);
		} else {
			eval = Material_EvaluateNoMix(evalMat, uv, *sampledDir, fixedDir,
					&eventMat, &pdfWMat
					TEXTURES_PARAM);
		}
		if (!Spectrum_IsBlack(eval)) {
			result += evalWeight * eval;
			*pdfW += evalWeight * pdfWMat;
		}
	}

	return result;
}

float3 MixMaterial_GetEmittedRadiance(__global Material *material, const float2 uv
		MATERIALS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	float totalWeightStack[MIX_STACK_SIZE];

	// Push the root Mix material
	materialStack[0] = material;
	totalWeightStack[0] = 1.f;
	int stackIndex = 0;

	// Setup the results
	float3 result = BLACK;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex];
		float totalWeight = totalWeightStack[stackIndex--];

		if (m->type == MIX) {
			const float factor = Texture_GetGreyValue(&texs[m->mix.mixFactorTexIndex], uv
					TEXTURES_PARAM);
			const float weight2 = clamp(factor, 0.f, 1.f);
			const float weight1 = 1.f - weight2;

			if (weight1 > 0.f) {
				materialStack[++stackIndex] = &mats[m->mix.matAIndex];
				totalWeightStack[stackIndex] = totalWeight * weight1;
			}

			if (weight2 > 0.f) {
				materialStack[++stackIndex] = &mats[m->mix.matBIndex];
				totalWeightStack[stackIndex] = totalWeight * weight2;
			}
		} else {
			const float3 emitRad = Material_GetEmittedRadianceNoMix(m, uv
				TEXTURES_PARAM);
			if (!Spectrum_IsBlack(emitRad))
				result += totalWeight * emitRad;
		}
	}
	
	return result;
}

float3 MixMaterial_GetPassThroughTransparency(__global Material *material,
		const float2 uv, const float3 fixedDir, const float passEvent
		MATERIALS_PARAM_DECL) {
	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	for (;;) {
		const float factor = Texture_GetGreyValue(&texs[currentMixMat->mix.mixFactorTexIndex], uv
				TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);
		passThroughEvent = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;
		const uint matIndex = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		__global Material *mat = &mats[matIndex];

		if (mat->type == MIX) {
			currentMixMat = mat;
			continue;
		} else
			return Material_GetPassThroughTransparencyNoMix(mat, uv, fixedDir, passThroughEvent
					TEXTURES_PARAM);
	}
}

#endif

//------------------------------------------------------------------------------
// Generic material functions with Mix support
//------------------------------------------------------------------------------

BSDFEvent Material_GetEventTypes(__global Material *material
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetEventTypes(material
				MATERIALS_PARAM);
	else
#endif
		return Material_GetEventTypesNoMix(material);
}

bool Material_IsDelta(__global Material *material
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_IsDelta(material
				MATERIALS_PARAM);
	else
#endif
		return Material_IsDeltaNoMix(material);
}

float3 Material_Evaluate(__global Material *material,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_Evaluate(material, uv, lightDir, eyeDir,
				event, directPdfW
				MATERIALS_PARAM);
	else
#endif
		return Material_EvaluateNoMix(material, uv, lightDir, eyeDir,
				event, directPdfW
				TEXTURES_PARAM);
}

float3 Material_Sample(__global Material *material,	const float2 uv,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_Sample(material, uv,
				fixedDir, sampledDir,
				u0, u1,
				passThroughEvent,
				pdfW, cosSampledDir, event
				MATERIALS_PARAM);
	else
#endif
		return Material_SampleNoMix(material, uv,
				fixedDir, sampledDir,
				u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				passThroughEvent,
#endif
				pdfW, cosSampledDir, event
				TEXTURES_PARAM);
}

float3 Material_GetEmittedRadiance(__global Material *material, const float2 uv
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetEmittedRadiance(material, uv
				MATERIALS_PARAM);
	else
#endif
		return Material_GetEmittedRadianceNoMix(material, uv
				TEXTURES_PARAM);
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Material_GetPassThroughTransparency(__global Material *material,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetPassThroughTransparency(material,
				uv, fixedDir, passThroughEvent
				MATERIALS_PARAM);
	else
#endif
		return Material_GetPassThroughTransparencyNoMix(material,
				uv, fixedDir, passThroughEvent
				TEXTURES_PARAM);
}
#endif
