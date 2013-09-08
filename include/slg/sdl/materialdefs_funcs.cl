#line 2 "material_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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
	const float d = 1.f + (roughness - 1.f) * cosNH * cosNH;
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
	// Values of roughness < .0001f seems to trigger some kind of exceptions with
	// AMD OpenCL on GPUs. The result is a nearly freeze of the PC.
	const float cos2Theta = (roughness < .0001f) ? 1.f : (u0 / (roughness * (1.f - u0) + u0));
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

float3 FrDiel2(const float cosi, const float3 cost, const float3 eta) {
	float3 Rparl = eta * cosi;
	Rparl = (cost - Rparl) / (cost + Rparl);
	float3 Rperp = eta * cost;
	Rperp = (cosi - Rperp) / (cosi + Rperp);

	return (Rparl * Rparl + Rperp * Rperp) * .5f;
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

float3 FresnelCauchy_Evaluate(const float eta, const float cosi) {
	// Compute indices of refraction for dielectric
	const bool entering = (cosi > 0.f);

	// Compute _sint_ using Snell's law
	const float eta2 = eta * eta;
	const float sint2 = (entering ? 1.f / eta2 : eta2) *
		fmax(0.f, 1.f - cosi * cosi);
	// Handle total internal reflection
	if (sint2 >= 1.f)
		return WHITE;
	else
		return FrDiel2(fabs(cosi), sqrt(fmax(0.f, 1.f - sint2)),
			entering ? eta : 1.f / eta);
}

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTE)

float3 MatteMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matte.kdTexIndex], hitPoint
			TEXTURES_PARAM));
	return M_1_PI_F * kd;
}

float3 MatteMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
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

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matte.kdTexIndex], hitPoint
			TEXTURES_PARAM));
	return M_1_PI_F * kd;
}

#endif

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIRROR)

float3 MirrorMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->mirror.krTexIndex], hitPoint
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
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = Texture_GetFloatValue(&texs[material->glass.ousideIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->glass.iorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;
	const float eta = entering ? (nc / nt) : ntc;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit
	
		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return BLACK;

		const float cost = sqrt(fmax(0.f, 1.f - sint2)) * (entering ? -1.f : 1.f);
		*localSampledDir = (float3)(-eta * localFixedDir.x, -eta * localFixedDir.y, cost);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;

		//if (!hitPoint.fromLight)
			result = (1.f - FresnelCauchy_Evaluate(ntc, cost)) * eta2;
		//else
		//	result = (1.f - FresnelCauchy_Evaluate(ntc, costheta));

		result *= kt;
	} else {
		// Reflect
		*localSampledDir = (float3)(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	// The absCosSampledDir is used to compensate the other one used inside the integrator
	return result / (*absCosSampledDir);
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

	const float phi = 2.f * M_PI_F * u0;
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
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	const float e = 1.f / (Texture_GetFloatValue(&texs[material->metal.expTexIndex], hitPoint
				TEXTURES_PARAM) + 1.f);
	*sampledDir = GlossyReflection(fixedDir, e, u0, u1);

	if ((*sampledDir).z * fixedDir.z > 0.f) {
		*event = SPECULAR | REFLECT;
		*pdfW = 1.f;
		*cosSampledDir = fabs((*sampledDir).z);

		const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->metal.krTexIndex], hitPoint
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
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = Texture_GetFloatValue(&texs[material->glass.ousideIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->glass.iorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;
	const float eta = nc / nt;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit

		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return BLACK;

		*localSampledDir = -localFixedDir;
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;

		//if (!hitPoint.fromLight) {
			if (entering)
				result = BLACK;
			else
				result = FresnelCauchy_Evaluate(ntc, -costheta);
		//} else {
		//	if (entering)
		//		result = FresnelCauchy_Evaluate(ntc, costheta);
		//	else
		//		result = BLACK;
		//}
		result *= 1.f + (1.f - result) * (1.f - result);
		result = 1.f - result;

		result *= kt;
	} else {
		// Reflect
		if (costheta <= 0.f)
			return BLACK;

		*localSampledDir = (float3)(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	// The absCosSampledDir is used to compensate the other one used inside the integrator
	return result / (*absCosSampledDir);
}

float3 ArchGlassMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = Texture_GetFloatValue(&texs[material->glass.ousideIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->glass.iorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;
	const float eta = nc / nt;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (passThroughEvent < threshold) {
		// Transmit

		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return BLACK;

		float3 result;
		//if (!hitPoint.fromLight) {
			if (entering)
				result = BLACK;
			else
				result = FresnelCauchy_Evaluate(ntc, -costheta);
		//} else {
		//	if (entering)
		//		result = FresnelCauchy_Evaluate(ntc, costheta);
		//	else
		//		result = Spectrum();
		//}
		result *= 1.f + (1.f - result) * (1.f - result);
		result = 1.f - result;

		return kt * result;
	} else
		return BLACK;
}

#endif

//------------------------------------------------------------------------------
// NULL material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_NULL)

float3 NullMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
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
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float cosSampledDir = dot(lightDir, eyeDir);

	const float3 r = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.krTexIndex], hitPoint
			TEXTURES_PARAM));
	const float3 t = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.ktTexIndex], hitPoint
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
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
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

	const float3 r = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.krTexIndex], hitPoint
			TEXTURES_PARAM));
	const float3 t = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.ktTexIndex], hitPoint
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
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	//if (!fromLight)
		factor = factor / 4.f * coso +
				(multibounce ? cosi * clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);
	//else
	//	factor = factor / (4.f * cosi) + 
	//			(multibounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);

	// The cosi is used to compensate the other one used inside the integrator
	factor /= cosi;
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

	// The cosi is used to compensate the other one used inside the integrator
	return S / cosi;
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
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float3 baseF = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glossy2.kdTexIndex], hitPoint
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

	float3 ks = Texture_GetSpectrumValue(&texs[material->glossy2.ksTexIndex], hitPoint
			TEXTURES_PARAM);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	const float i = Texture_GetFloatValue(&texs[material->glossy2.indexTexIndex], hitPoint
			TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
#endif
	ks = Spectrum_Clamp(ks);

	const float u = clamp(Texture_GetFloatValue(&texs[material->glossy2.nuTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float v = clamp(Texture_GetFloatValue(&texs[material->glossy2.nvTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	}

	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float3 alpha = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glossy2.kaTexIndex], hitPoint
			TEXTURES_PARAM));
	const float d = Texture_GetFloatValue(&texs[material->glossy2.depthTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 absorption = SchlickBSDF_CoatingAbsorption(cosi, coso, alpha, d);
#else
	const float3 absorption = WHITE;
#endif

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSlick_Evaluate(ks, fabs(dot(sampledDir, H)));

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = material->glossy2.multibounce;
#else
	const int multibounce = 0;
#endif
	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,
			fixedDir, sampledDir);

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	return coatingF + absorption * (WHITE - S) * baseF;
}

float3 Glossy2Material_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	float3 ks = Texture_GetSpectrumValue(&texs[material->glossy2.ksTexIndex], hitPoint
			TEXTURES_PARAM);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	const float i = Texture_GetFloatValue(&texs[material->glossy2.indexTexIndex], hitPoint
			TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
#endif
	ks = Spectrum_Clamp(ks);

	const float u = clamp(Texture_GetFloatValue(&texs[material->glossy2.nuTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float v = clamp(Texture_GetFloatValue(&texs[material->glossy2.nvTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	// Coating is used only on the front face
	const float wCoating = (fixedDir.z <= 0.f) ? 0.f : SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;

	const float3 baseF = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glossy2.kdTexIndex], hitPoint
		TEXTURES_PARAM)) * M_1_PI_F;

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = material->glossy2.multibounce;
#else
	const int multibounce = 0;
#endif

	float basePdf, coatingPdf;
	float3 coatingF;
	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,
				fixedDir, *sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);

		*event = DIFFUSE | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				multibounce, fixedDir, sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF))
			return BLACK;

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = fabs((*sampledDir).z * M_1_PI_F);

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;
	if (fixedDir.z > 0.f) {
		// Front face reflection: coating+base

		// Absorption
		const float cosi = fabs((*sampledDir).z);
		const float coso = fabs(fixedDir.z);

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
		const float3 alpha = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glossy2.kaTexIndex], hitPoint
			TEXTURES_PARAM));
		const float d = Texture_GetFloatValue(&texs[material->glossy2.depthTexIndex], hitPoint
			TEXTURES_PARAM);
		const float3 absorption = SchlickBSDF_CoatingAbsorption(cosi, coso, alpha, d);
#else
		const float3 absorption = WHITE;
#endif

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + *sampledDir);
		const float3 S = FresnelSlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

		// Blend in base layer Schlick style
		// coatingF already takes fresnel factor S into account

		return coatingF + absorption * (WHITE - S) * baseF;
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
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float u = clamp(Texture_GetFloatValue(&texs[material->metal2.nuTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float v = clamp(Texture_GetFloatValue(&texs[material->metal2.nvTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	const float3 wh = normalize(fixedDir + sampledDir);
	const float cosWH = dot(fixedDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * fabs(dot(fixedDir, wh)));

	const float3 nVal = Texture_GetSpectrumValue(&texs[material->metal2.nTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetSpectrumValue(&texs[material->metal2.kTexIndex], hitPoint
			TEXTURES_PARAM);

	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs(sampledDir.z);
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	//if (!hitPoint.fromLight)
		factor /= 4.f * coso;
	//else
	//	factor /= 4.f * cosi;

	*event = GLOSSY | REFLECT;

	// The cosi is used to compensate the other one used inside the integrator
	factor /= cosi;
	return factor * F;
}

float3 Metal2Material_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float u = clamp(Texture_GetFloatValue(&texs[material->metal2.nuTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float v = clamp(Texture_GetFloatValue(&texs[material->metal2.nvTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

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
	
	const float3 nVal = Texture_GetSpectrumValue(&texs[material->metal2.nTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetSpectrumValue(&texs[material->metal2.kTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	float factor = d * G;
	//if (!fromLight)
		factor /= 4.f * coso;
	//else
	//	factor /= cosi;

	*event = GLOSSY | REFLECT;

	// The cosi is used to compensate the other one used inside the integrator
	factor /= cosi;
	return factor * F;
}

#endif

//------------------------------------------------------------------------------
// RoughGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)

float3 RoughGlassMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 localLightDir, const float3 localEyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->roughglass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->roughglass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;
	
	//const float3 localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	//const float3 localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;
	const float3 localFixedDir = localEyeDir;
	const float3 localSampledDir = localLightDir;

	const float nc = Texture_GetFloatValue(&texs[material->roughglass.ousideIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->roughglass.iorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;

	const float u = clamp(Texture_GetFloatValue(&texs[material->roughglass.nuTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
	const float v = clamp(Texture_GetFloatValue(&texs[material->roughglass.nvTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (dot(localFixedDir, localSampledDir) < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localFixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		float3 wh = eta * localFixedDir + localSampledDir;
		if (wh.z < 0.f)
			wh = -wh;

		//const float lengthSquared = wh.LengthSquared();
		const float lengthSquared = dot(wh, wh);
		if (!(lengthSquared > 0.f))
			return BLACK;
		wh /= sqrt(lengthSquared);
		const float cosThetaI = fabs(CosTheta(localSampledDir));
		const float cosThetaIH = fabs(dot(localSampledDir, wh));
		const float cosThetaOH = dot(localFixedDir, wh);

		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaOH);

		if (directPdfW)
			*directPdfW = threshold * specPdf * fabs(cosThetaOH) / lengthSquared;

		//if (reversePdfW)
		//	*reversePdfW = threshold * specPdf * cosThetaIH * eta * eta / lengthSquared;

		float3 result = (fabs(cosThetaOH) * cosThetaIH * D *
			G / (cosThetaI * lengthSquared)) *
			kt * (1.f - F);

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wl). So I have to remove that term.
		result /= fabs(CosTheta(localLightDir));
		return result;
	} else {
		// Reflect
		const float cosThetaO = fabs(CosTheta(localFixedDir));
		const float cosThetaI = fabs(CosTheta(localSampledDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return BLACK;
		float3 wh = localFixedDir + localSampledDir;
		if (all(isequal(wh, BLACK)))
			return BLACK;
		wh = normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		float cosThetaH = dot(localEyeDir, wh);
		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaH);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(localFixedDir, wh)));

		//if (reversePdfW)
		//	*reversePdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(localSampledDir, wh));

		float3 result = (D * G / (4.f * cosThetaI)) * kr * F;

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wl). So I have to remove that term.
		result /= fabs(CosTheta(localLightDir));
		return result;
	}
}

float3 RoughGlassMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event
		TEXTURES_PARAM_DECL) {
	if (fabs(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->roughglass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->roughglass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const float u = clamp(Texture_GetFloatValue(&texs[material->roughglass.nuTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
	const float v = clamp(Texture_GetFloatValue(&texs[material->roughglass.nvTexIndex], hitPoint
		TEXTURES_PARAM), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	if (wh.z < 0.f)
		wh = -wh;
	const float cosThetaOH = dot(localFixedDir, wh);

	const float nc = Texture_GetFloatValue(&texs[material->roughglass.ousideIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->roughglass.iorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;

	const float coso = fabs(localFixedDir.z);

	// Decide to transmit or reflect
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit

		const bool entering = (CosTheta(localFixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;
		const float eta2 = eta * eta;
		const float sinThetaIH2 = eta2 * fmax(0.f, 1.f - cosThetaOH * cosThetaOH);
		if (sinThetaIH2 >= 1.f)
			return BLACK;
		float cosThetaIH = sqrt(1.f - sinThetaIH2);
		if (entering)
			cosThetaIH = -cosThetaIH;
		const float length = eta * cosThetaOH + cosThetaIH;
		*localSampledDir = length * wh - eta * localFixedDir;

		const float lengthSquared = length * length;
		*pdfW = specPdf * fabs(cosThetaIH) / lengthSquared;
		if (*pdfW <= 0.f)
			return BLACK;

		const float cosi = fabs((*localSampledDir).z);
		*absCosSampledDir = cosi;

		const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
		float factor = d * G * fabs(cosThetaOH) / specPdf;

		//if (!hitPoint.fromLight) {
			const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaIH);
			result = (factor / coso) * kt * (1.f - F);
		//} else {
		//	const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//	result = (factor / cosi) * kt * (Spectrum(1.f) - F);
		//}

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wi)/pdf if reverse==true and by Dot(ns. wo)/pdf if reverse==false.
		// So I have to remove that terms.
		//result *= *pdfW / ((!hitPoint.fromLight) ? cosi : coso);
			result *= *pdfW / cosi;
		*pdfW *= threshold;
		*event = GLOSSY | TRANSMIT;
	} else {
		// Reflect
		*pdfW = specPdf / (4.f * fabs(cosThetaOH));
		if (*pdfW <= 0.f)
			return BLACK;

		*localSampledDir = 2.f * cosThetaOH * wh - localFixedDir;

		const float cosi = fabs((*localSampledDir).z);
		*absCosSampledDir = cosi;
		if ((*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) || (localFixedDir.z * (*localSampledDir).z < 0.f))
			return BLACK;

		const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
		float factor = d * G * fabs(cosThetaOH) / specPdf;

		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//factor /= (!hitPoint.fromLight) ? coso : cosi;
		factor /= coso;
		result = factor * F * kr;

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wi)/pdf if reverse==true and by Dot(ns. wo)/pdf if reverse==false.
		// So I have to remove that terms.
		//result *= *pdfW / ((!hitPoint.fromLight) ? cosi : coso);
		result *= *pdfW / cosi;
		*pdfW *= (1.f - threshold);
		*event = GLOSSY | REFLECT;
	}

	return result;
}

#endif
