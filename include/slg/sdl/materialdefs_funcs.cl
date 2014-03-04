#line 2 "materialdefs_funcs.cl"

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
// Generic material related functions
//------------------------------------------------------------------------------

float SchlickDistribution_SchlickZ(const float roughness, float cosNH) {
	const float cosNH2 = cosNH * cosNH;
	// expanded for increased numerical stability
	const float d = cosNH2 * roughness + (1.f - cosNH2);
	// use double division to avoid overflow in d*d product
	return (roughness / d) / d;
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
	return kd * fabs(lightDir.z * M_1_PI_F);
}

float3 MatteMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matte.kdTexIndex], hitPoint
			TEXTURES_PARAM));
	return kd;
}

#endif

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIRROR)

float3 MirrorMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (SPECULAR | REFLECT)))
		return BLACK;

	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->mirror.krTexIndex], hitPoint
			TEXTURES_PARAM));
	return kr;
}

#endif

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLASS)

float3 GlassMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & SPECULAR))
		return BLACK;

	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = Texture_GetFloatValue(&texs[material->glass.exteriorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->glass.interiorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;
	const float eta = entering ? (nc / nt) : ntc;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return BLACK;
	}

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

	return result / *pdfW;
}

#endif

//------------------------------------------------------------------------------
// ArchGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ARCHGLASS)

float3 ArchGlassMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & SPECULAR))
		return BLACK;

	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->archglass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->archglass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = Texture_GetFloatValue(&texs[material->archglass.exteriorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->archglass.interiorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;
	const float eta = nc / nt;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return BLACK;
	}

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

	return result / *pdfW;
}

float3 ArchGlassMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->archglass.ktTexIndex], hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->archglass.krTexIndex], hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = Texture_GetFloatValue(&texs[material->archglass.exteriorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->archglass.interiorIorTexIndex], hitPoint
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
		//		result = BLACK;
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
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (SPECULAR | TRANSMIT)))
		return BLACK;

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
	const float3 r = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.krTexIndex], hitPoint
			TEXTURES_PARAM));
	const float3 t = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.ktTexIndex], hitPoint
			TEXTURES_PARAM)) * 
		// Energy conservation
		(1.f - r);

	const bool isKtBlack = Spectrum_IsBlack(t);
	const bool isKrBlack = Spectrum_IsBlack(r);

	// Decide to transmit or reflect
	float threshold;
	if (!isKrBlack) {
		if (!isKtBlack)
			threshold = .5f;
		else
			threshold = 1.f;
	} else {
		if (!isKtBlack)
			threshold = 0.f;
		else {
			if (directPdfW)
				*directPdfW = 0.f;
			return BLACK;
		}
	}

	const bool relfected = (CosTheta(lightDir) * CosTheta(eyeDir) > 0.f);
	const float weight = (lightDir.z * eyeDir.z > 0.f) ? threshold : (1.f - threshold);

	if (directPdfW)
		*directPdfW = weight * fabs(lightDir.z * M_1_PI_F);

	if (lightDir.z * eyeDir.z > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * fabs(lightDir.z * M_1_PI_F);
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * fabs(lightDir.z * M_1_PI_F);
	}
}

float3 MatteTranslucentMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (DIFFUSE | REFLECT | TRANSMIT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, pdfW);
	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.krTexIndex], hitPoint
			TEXTURES_PARAM));
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matteTranslucent.ktTexIndex], hitPoint
			TEXTURES_PARAM)) * 
		// Energy conservation
		(1.f - kr);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 1.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 0.f;
		else
			return BLACK;
	}

	if (passThroughEvent < threshold) {
		*sampledDir *= (signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | REFLECT;
		*pdfW *= threshold;

		return kr / threshold;
	} else {
		*sampledDir *= -(signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | TRANSMIT;
		*pdfW *= (1.f - threshold);

		return kt / (1.f - threshold);
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
	S *= (d / *pdf) * G / (4.f * coso) + 
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
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float3 baseF = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glossy2.kdTexIndex], hitPoint
			TEXTURES_PARAM)) * M_1_PI_F * fabs(lightDir.z);
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

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
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
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if ((!(requestedEvent & (GLOSSY | REFLECT)) && fixedDir.z > 0.f) ||
		(!(requestedEvent & (DIFFUSE | REFLECT)) && fixedDir.z <= 0.f) ||
		(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	if (fixedDir.z <= 0.f) {
		// Back Face
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);
		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;
		*event = DIFFUSE | REFLECT;
		return Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->glossy2.kdTexIndex], hitPoint
			TEXTURES_PARAM));
	}

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
	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
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

		*event = GLOSSY | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				multibounce, fixedDir, sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF))
			return BLACK;

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		coatingF *= coatingPdf;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = *cosSampledDir * M_1_PI_F;

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;

	// Absorption
	const float cosi = fabs((*sampledDir).z);
	const float coso = fabs(fixedDir.z);

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
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

	return (coatingF + absorption * (WHITE - S) * baseF * *cosSampledDir) / *pdfW;
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

	const float3 wh = normalize(lightDir + eyeDir);
	const float cosWH = dot(lightDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * cosWH);

	const float3 nVal = Texture_GetSpectrumValue(&texs[material->metal2.nTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetSpectrumValue(&texs[material->metal2.kTexIndex], hitPoint
			TEXTURES_PARAM);

	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	const float G = SchlickDistribution_G(roughness, lightDir, eyeDir);

	*event = GLOSSY | REFLECT;
	return SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * fabs(eyeDir.z)) * F;
}

float3 Metal2Material_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
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

	*pdfW = specPdf / (4.f * fabs(cosWH));
	if (*pdfW <= 0.f)
		return BLACK;

	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);
	
	const float3 nVal = Texture_GetSpectrumValue(&texs[material->metal2.nTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetSpectrumValue(&texs[material->metal2.kTexIndex], hitPoint
			TEXTURES_PARAM);
	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	float factor = (d / *pdfW) * G * fabs(cosWH);
	//if (!fromLight)
		factor /= 4.f * coso;
	//else
	//	factor /= cosi;

	*event = GLOSSY | REFLECT;

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
	
	const float nc = Texture_GetFloatValue(&texs[material->roughglass.exteriorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->roughglass.interiorIorTexIndex], hitPoint
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
	if (localLightDir.z * localEyeDir.z < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localLightDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		float3 wh = eta * localLightDir + localEyeDir;
		if (wh.z < 0.f)
			wh = -wh;

		const float lengthSquared = dot(wh, wh);
		if (!(lengthSquared > 0.f))
			return BLACK;
		wh /= sqrt(lengthSquared);
		const float cosThetaI = fabs(CosTheta(localEyeDir));
		const float cosThetaIH = fabs(dot(localEyeDir, wh));
		const float cosThetaOH = dot(localLightDir, wh);

		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaOH);

		if (directPdfW)
			*directPdfW = threshold * specPdf * (fabs(cosThetaOH) * eta * eta) / lengthSquared;

		//if (reversePdfW)
		//	*reversePdfW = threshold * specPdf * cosThetaIH / lengthSquared;

		float3 result = (fabs(cosThetaOH) * cosThetaIH * D *
			G / (cosThetaI * lengthSquared)) *
			kt * (1.f - F);

        *event = DIFFUSE | TRANSMIT;

		return result;
	} else {
		// Reflect
		const float cosThetaO = fabs(CosTheta(localLightDir));
		const float cosThetaI = fabs(CosTheta(localEyeDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return BLACK;
		float3 wh = localLightDir + localEyeDir;
		if (all(isequal(wh, BLACK)))
			return BLACK;
		wh = normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		float cosThetaH = dot(localEyeDir, wh);
		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaH);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(localLightDir, wh)));

		//if (reversePdfW)
		//	*reversePdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(localLightDir, wh));

		float3 result = (D * G / (4.f * cosThetaI)) * kr * F;

        *event = DIFFUSE | REFLECT;

		return result;
	}
}

float3 RoughGlassMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (GLOSSY | REFLECT | TRANSMIT)) ||
			(fabs(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
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

	const float nc = Texture_GetFloatValue(&texs[material->roughglass.exteriorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float nt = Texture_GetFloatValue(&texs[material->roughglass.interiorIorTexIndex], hitPoint
			TEXTURES_PARAM);
	const float ntc = nt / nc;

	const float coso = fabs(localFixedDir.z);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return BLACK;
	}

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
		float factor = (d / specPdf) * G * fabs(cosThetaOH) / threshold;

		//if (!hitPoint.fromLight) {
			const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaIH);
			result = (factor / coso) * kt * (1.f - F);
		//} else {
		//	const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//	result = (factor / cosi) * kt * (Spectrum(1.f) - F);
		//}

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
		float factor = (d / specPdf) * G * fabs(cosThetaOH) / (1.f - threshold);

		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//factor /= (!hitPoint.fromLight) ? coso : cosi;
		factor /= coso;
		result = factor * F * kr;

		*pdfW *= (1.f - threshold);
		*event = GLOSSY | REFLECT;
	}

	return result;
}

#endif

//------------------------------------------------------------------------------
// Velvet material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_VELVET)

float3 VelvetMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
		
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->velvet.kdTexIndex], hitPoint
			TEXTURES_PARAM));

	const float A1 = Texture_GetFloatValue(&texs[material->velvet.p1TexIndex], hitPoint
			TEXTURES_PARAM);

	const float A2 = Texture_GetFloatValue(&texs[material->velvet.p2TexIndex], hitPoint
			TEXTURES_PARAM);

	const float A3 = Texture_GetFloatValue(&texs[material->velvet.p3TexIndex], hitPoint
			TEXTURES_PARAM);

	const float delta = Texture_GetFloatValue(&texs[material->velvet.thicknessTexIndex], hitPoint
			TEXTURES_PARAM);

	const float cosv = -dot(lightDir, eyeDir);

	// Compute phase function

	const float B = 3.0f * cosv;

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI_F);
 
	p = (p * delta) / fabs(eyeDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	return p * kd;
}

float3 VelvetMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->velvet.kdTexIndex], hitPoint
			TEXTURES_PARAM));

	const float A1 = Texture_GetFloatValue(&texs[material->velvet.p1TexIndex], hitPoint
			TEXTURES_PARAM);

	const float A2 = Texture_GetFloatValue(&texs[material->velvet.p2TexIndex], hitPoint
			TEXTURES_PARAM);

	const float A3 = Texture_GetFloatValue(&texs[material->velvet.p3TexIndex], hitPoint
			TEXTURES_PARAM);

	const float delta = Texture_GetFloatValue(&texs[material->velvet.thicknessTexIndex], hitPoint
			TEXTURES_PARAM);

	const float cosv = dot(-fixedDir, *sampledDir);;

	// Compute phase function

	const float B = 3.0f * cosv;

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI_F);
 
	p = (p * delta) / fabs(fixedDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	return kd * (p / *pdfW);
}
#endif

//------------------------------------------------------------------------------
// Cloth material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_CLOTH)

__constant WeaveConfig ClothWeaves[] = {
    // DenimWeave
    {
        3, 6,
        0.01f, 4.0f,
        0.0f, 0.5f,
        5.0f, 1.0f, 3.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // SilkShantungWeave
    {
        6, 8,
        0.02f, 1.5f,
        0.5f, 0.5f, 
        8.0f, 16.0f, 0.0f,
        20.0f, 20.0f, 10.0f, 10.0f,
        500.0f
    },
    // SilkCharmeuseWeave
    {
        5, 10,
        0.02f, 7.3f,
        0.5f, 0.5f, 
        9.0f, 1.0f, 3.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // CottonTwillWeave
    {
        4, 8,
        0.01f, 4.0f,
        0.0f, 0.5f, 
        6.0f, 2.0f, 4.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // WoolGarbardineWeave
    {
        6, 9,
        0.01f, 4.0f,
        0.0f, 0.5f, 
        12.0f, 6.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // PolyesterWeave
    {
        2, 2,
        0.015f, 4.0f,
        0.5f, 0.5f,
        1.0f, 1.0f, 0.0f, 
        8.0f, 8.0f, 6.0f, 6.0f,
        50.0f
    }
};

__constant Yarn ClothYarns[][14] = {
    // DenimYarn[8]
    {
        {-30, 12, 0, 1, 5, 0.1667f, 0.75f, WARP},
        {-30, 12, 0, 1, 5, 0.1667f, -0.25f, WARP},
        {-30, 12, 0, 1, 5, 0.5f, 1.0833f, WARP},
        {-30, 12, 0, 1, 5, 0.5f, 0.0833f, WARP},
        {-30, 12, 0, 1, 5, 0.8333f, 0.4167f, WARP},
        {-30, 38, 0, 1, 1, 0.1667f, 0.25f, WEFT},
        {-30, 38, 0, 1, 1, 0.5f, 0.5833f, WEFT},
        {-30, 38, 0, 1, 1, 0.8333f, 0.9167f, WEFT}
    },
    // SilkShantungYarn[5]
    {
        {0, 50, -0.5, 2, 4,  0.3333f, 0.25f, WARP},
        {0, 50, -0.5, 2, 4,  0.8333f, 0.75f, WARP},
        {0, 23, -0.3, 4, 4,  0.3333f, 0.75f, WEFT},
        {0, 23, -0.3, 4, 4, -0.1667f, 0.25f, WEFT},
        {0, 23, -0.3, 4, 4,  0.8333f, 0.25f, WEFT}
    },
    // SilkCharmeuseYarn[14]
    {
        {0, 40, 2, 1, 9, 0.1, 0.45, WARP},
        {0, 40, 2, 1, 9, 0.3, 1.05, WARP},
        {0, 40, 2, 1, 9, 0.3, 0.05, WARP},
        {0, 40, 2, 1, 9, 0.5, 0.65, WARP},
        {0, 40, 2, 1, 9, 0.5, -0.35, WARP},
        {0, 40, 2, 1, 9, 0.7, 1.25, WARP},
        {0, 40, 2, 1, 9, 0.7, 0.25, WARP},
        {0, 40, 2, 1, 9, 0.9, 0.85, WARP},
        {0, 40, 2, 1, 9, 0.9, -0.15, WARP},
        {0, 60, 0, 1, 1, 0.1, 0.95, WEFT},
        {0, 60, 0, 1, 1, 0.3, 0.55, WEFT},
        {0, 60, 0, 1, 1, 0.5, 0.15, WEFT},
        {0, 60, 0, 1, 1, 0.7, 0.75, WEFT},
        {0, 60, 0, 1, 1, 0.9, 0.35, WEFT}
    },
    // CottonTwillYarn[10]
    {
        {-30, 24, 0, 1, 6, 0.125,  0.375, WARP},
        {-30, 24, 0, 1, 6, 0.375,  1.125, WARP},
        {-30, 24, 0, 1, 6, 0.375,  0.125, WARP},
        {-30, 24, 0, 1, 6, 0.625,  0.875, WARP},
        {-30, 24, 0, 1, 6, 0.625, -0.125, WARP},
        {-30, 24, 0, 1, 6, 0.875,  0.625, WARP},
        {-30, 36, 0, 2, 1, 0.125,  0.875, WEFT},
        {-30, 36, 0, 2, 1, 0.375,  0.625, WEFT},
        {-30, 36, 0, 2, 1, 0.625,  0.375, WEFT},
        {-30, 36, 0, 2, 1, 0.875,  0.125, WEFT}
    },
    // WoolGarbardineYarn[7]
    {
        {30, 30, 0, 2, 6, 0.167, 0.667, WARP},
        {30, 30, 0, 2, 6, 0.500, 1.000, WARP},
        {30, 30, 0, 2, 6, 0.500, 0.000, WARP},
        {30, 30, 0, 2, 6, 0.833, 0.333, WARP},
        {30, 30, 0, 3, 2, 0.167, 0.167, WEFT},
        {30, 30, 0, 3, 2, 0.500, 0.500, WEFT},
        {30, 30, 0, 3, 2, 0.833, 0.833, WEFT}
    },
    // PolyesterYarn[4]
    {
        {0, 22, -0.7, 1, 1, 0.25, 0.25, WARP},
        {0, 22, -0.7, 1, 1, 0.75, 0.75, WARP},
        {0, 16, -0.7, 1, 1, 0.25, 0.75, WEFT},
        {0, 16, -0.7, 1, 1, 0.75, 0.25, WEFT}
    }
};

__constant int ClothPatterns[][6 * 9] = {
    // DenimPattern[3 * 6]
    {
        1, 3, 8,  1, 3, 5,  1, 7, 5,  1, 4, 5,  6, 4, 5,  2, 4, 5
    },
    // SilkShantungPattern[6 * 8]
    {
        3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,
        4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5
    },
    // SilkCharmeusePattern[5 * 10]
    {
        10, 2, 4, 6, 8,   1, 2, 4, 6,  8,  1, 2, 4, 13, 8,  1, 2,  4, 7, 8,  1, 11, 4, 7, 8,
        1, 3, 4, 7, 8,   1, 3, 4, 7, 14,  1, 3, 4,  7, 9,  1, 3, 12, 7, 9,  1,  3, 5, 7, 9
    },
    // CottonTwillPattern[4 * 8]
    {
        7, 2, 4, 6,  7, 2, 4, 6,  1, 8, 4,  6,  1, 8, 4,  6,
        1, 3, 9, 6,  1, 3, 9, 6,  1, 3, 5, 10,  1, 3, 5, 10
    },
    // WoolGarbardinePattern[6 * 9]
    {
        1, 1, 2, 2, 7, 7,  1, 1, 2, 2, 7, 7,  1, 1, 2, 2, 7, 7,
        1, 1, 6, 6, 4, 4,  1, 1, 6, 6, 4, 4,  1, 1, 6, 6, 4, 4,
        5, 5, 3, 3, 4, 4,  5, 5, 3, 3, 4, 4,  5, 5, 3, 3, 4, 4
    },
    // PolyesterPattern[2 * 2]
    {
        3, 2, 1, 4
    }
};

ulong sampleTEA(uint v0, uint v1, uint rounds) {
	uint sum = 0;

	for (uint i = 0; i < rounds; ++i) {
		sum += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xA341316C) ^ (v1 + sum) ^ ((v1 >> 5) + 0xC8013EA4);
		v1 += ((v0 << 4) + 0xAD90777D) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7E95761E);
	}

	return ((ulong) v1 << 32) + v0;
}

float sampleTEAfloat(uint v0, uint v1, uint rounds) {
	/* Trick from MTGP: generate an uniformly distributed
	   single precision number in [1,2) and subtract 1. */
	union {
		uint u;
		float f;
	} x;
	x.u = ((sampleTEA(v0, v1, rounds) & 0xFFFFFFFF) >> 9) | 0x3f800000UL;
	return x.f - 1.0f;
}

// von Mises Distribution
float vonMises(float cos_x, float b) {
	// assumes a = 0, b > 0 is a concentration parameter.

	const float factor = exp(b * cos_x) * (.5f * M_1_PI_F);
	const float absB = fabs(b);
	if (absB <= 3.75f) {
		const float t0 = absB / 3.75f;
		const float t = t0 * t0;
		return factor / (1.0f + t * (3.5156229f + t * (3.0899424f +
			t * (1.2067492f + t * (0.2659732f + t * (0.0360768f +
			t * 0.0045813f))))));
	} else {
		const float t = 3.75f / absB;
		return factor * sqrt(absB) / (exp(absB) * (0.39894228f +
			t * (0.01328592f + t * (0.00225319f +
			t * (-0.00157565f + t * (0.00916281f +
			t * (-0.02057706f + t * (0.02635537f +
			t * (-0.01647633f + t * 0.00392377f)))))))));
	}
}

// Attenuation term
float seeliger(float cos_th1, float cos_th2, float sg_a, float sg_s) {
	const float al = sg_s / (sg_a + sg_s); // albedo
	const float c1 = fmax(0.f, cos_th1);
	const float c2 = fmax(0.f, cos_th2);
	if (c1 == 0.0f || c2 == 0.0f)
		return 0.0f;
	return al * (.5f * M_1_PI_F) * .5f * c1 * c2 / (c1 + c2);
}

void GetYarnUV(__constant WeaveConfig *Weave, __constant Yarn *yarn,
        const float Repeat_U, const float Repeat_V,
        const float3 center, const float3 xy, float2 *uv, float *umaxMod) {
	*umaxMod = Radians(yarn->umax);
	if (Weave->period > 0.f) {
		/* Number of TEA iterations (the more, the better the
		   quality of the pseudorandom floats) */
		const int teaIterations = 8;

		// Correlated (Perlin) noise.
		// generate 1 seed per yarn segment
		const float random1 = Noise((center.x *
			(Weave->tileHeight * Repeat_V +
			sampleTEAfloat(center.x, 2.f * center.y,
			teaIterations)) + center.y) / Weave->period, 0.0, 0.0);
		const float random2 = Noise((center.y *
			(Weave->tileWidth * Repeat_U +
			sampleTEAfloat(center.x, 2.f * center.y + 1.f,
			teaIterations)) + center.x) / Weave->period, 0.0, 0.0);
		
		if (yarn->yarn_type == WARP)
	  		*umaxMod += random1 * Radians(Weave->dWarpUmaxOverDWarp) +
				random2 * Radians(Weave->dWarpUmaxOverDWeft);
		else
			*umaxMod += random1 * Radians(Weave->dWeftUmaxOverDWarp) +
				random2 * Radians(Weave->dWeftUmaxOverDWeft);
	}
	

	// Compute u and v.
	// See Chapter 6.
	// Rotate pi/2 radians around z axis
	if (yarn->yarn_type == WARP) {
		(*uv).s0 = xy.y * 2.f * *umaxMod / yarn->length;
		(*uv).s1 = xy.x * M_PI_F / yarn->width;
	} else {
		(*uv).s0 = xy.x * 2.f * *umaxMod / yarn->length;
		(*uv).s1 = -xy.y * M_PI_F / yarn->width;
	}
}

__constant Yarn *GetYarn(const ClothPreset Preset, __constant WeaveConfig *Weave,
        const float Repeat_U, const float Repeat_V,
        const float u_i, const float v_i,
        float2 *uv, float *umax, float *scale) {
	const float u = u_i * Repeat_U;
	const int bu = Floor2Int(u);
	const float ou = u - bu;
	const float v = v_i * Repeat_V;
	const int bv = Floor2Int(v);
	const float ov = v - bv;
	const uint lx = min(Weave->tileWidth - 1, Floor2UInt(ou * Weave->tileWidth));
	const uint ly = Weave->tileHeight - 1 -
		min(Weave->tileHeight - 1, Floor2UInt(ov * Weave->tileHeight));

	const int yarnID = ClothPatterns[Preset][lx + Weave->tileWidth * ly] - 1;
	__constant Yarn *yarn = &ClothYarns[Preset][yarnID];

	const float3 center = (float3)((bu + yarn->centerU) * Weave->tileWidth,
		(bv + yarn->centerV) * Weave->tileHeight, 0.f);
	const float3 xy = (float3)((ou - yarn->centerU) * Weave->tileWidth,
		(ov - yarn->centerV) * Weave->tileHeight, 0.f);

	GetYarnUV(Weave, yarn, Repeat_U, Repeat_V, center, xy, uv, umax);

	/* Number of TEA iterations (the more, the better the
	   quality of the pseudorandom floats) */
	const int teaIterations = 8;

	// Compute random variation and scale specular component.
	if (Weave->fineness > 0.0f) {
		// Initialize random number generator based on texture location.
		// Generate fineness^2 seeds per 1 unit of texture.
		const uint index1 = (uint) ((center.x + xy.x) * Weave->fineness);
		const uint index2 = (uint) ((center.y + xy.y) * Weave->fineness);

		const float xi = sampleTEAfloat(index1, index2, teaIterations);
		
		*scale *= fmin(-log(xi), 10.0f);
	}

	return yarn;
}

float RadiusOfCurvature(__constant Yarn *yarn, float u, float umaxMod) {
	// rhat determines whether the spine is a segment
	// of an ellipse, a parabole, or a hyperbola.
	// See Section 5.3.
	const float rhat = 1.0f + yarn->kappa * (1.0f + 1.0f / tan(umaxMod));
	const float a = 0.5f * yarn->width;
	
	if (rhat == 1.0f) { // circle; see Subsection 5.3.1.
		return 0.5f * yarn->length / sin(umaxMod) - a;
	} else if (rhat > 0.0f) { // ellipsis
		const float tmax = atan(rhat * tan(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sin(umaxMod)) / sin(tmax);
		const float ahat = bhat / rhat;
		const float t = atan(rhat * tan(u));
		return pow(bhat * bhat * cos(t) * cos(t) +
			ahat * ahat * sin(t) * sin(t), 1.5f) / (ahat * bhat);
	} else if (rhat < 0.0f) { // hyperbola; see Subsection 5.3.3.
		const float tmax = -atanh(rhat * tan(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sin(umaxMod)) / sinh(tmax);
		const float ahat = bhat / rhat;
		const float t = -atanh(rhat * tan(u));
		return -pow(bhat * bhat * cosh(t) * cosh(t) +
			ahat * ahat * sinh(t) * sinh(t), 1.5f) / (ahat * bhat);
	} else { // rhat == 0  // parabola; see Subsection 5.3.2.
		const float tmax = tan(umaxMod);
		const float ahat = (0.5f * yarn->length - a * sin(umaxMod)) / (2.f * tmax);
		const float t = tan(u);
		return 2.f * ahat * pow(1.f + t * t, 1.5f);
	}
}

float EvalFilamentIntegrand(__constant WeaveConfig *Weave, __constant Yarn *yarn, const float3 om_i,
        const float3 om_r, float u, float v, float umaxMod) {
	// 0 <= ss < 1.0
	if (Weave->ss < 0.0f || Weave->ss >= 1.0f)
		return 0.0f;

	// w * sin(umax) < l
	if (yarn->width * sin(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const float3 h = normalize(om_r + om_i);

	// u_of_v is location of specular reflection.
	const float u_of_v = atan2(h.y, h.z);

	// Check if u_of_v within the range of valid u values
	if (fabs(u_of_v) >= umaxMod)
		return 0.f;

	// Highlight has constant width delta_u
	const float delta_u = umaxMod * Weave->hWidth;

	// Check if |u(v) - u| < delta_u.
	if (fabs(u_of_v - u) >= delta_u)
		return 0.f;

	
	// n is normal to the yarn surface
	// t is tangent of the fibers.
	const float3 n = normalize((float3)(sin(v), sin(u_of_v) * cos(v),
		cos(u_of_v) * cos(v)));
	const float3 t = normalize((float3)(0.0f, cos(u_of_v), -sin(u_of_v)));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, fmin(fabs(u_of_v),
		(1.f - Weave->ss) * umaxMod), (1.f - Weave->ss) * umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const float3 om_i_plus_om_r = om_i + om_r;
    const float3 t_cross_h = cross(t, h);
	const float Gu = a * (R + a * cos(v)) /
		(length(om_i_plus_om_r) * fabs(t_cross_h.x));


	// fc is phase function
	const float fc = Weave->alpha + vonMises(-dot(om_i, om_r), Weave->beta);

	// attenuation function without smoothing.
	float As = seeliger(dot(n, om_i), dot(n, om_r), 0, 1);
	// As is attenuation function with smoothing.
	if (Weave->ss > 0.0f)
		As *= SmoothStep(0.f, 1.f, (umaxMod - fabs(u_of_v)) /
			(Weave->ss * umaxMod));

	// fs is scattering function.
	const float fs = Gu * fc * As;

	// Domain transform.
	return fs * M_PI_F / Weave->hWidth;
}

float EvalStapleIntegrand(__constant WeaveConfig *Weave, __constant Yarn *yarn,
        const float3 om_i, const float3 om_r, float u, float v, float umaxMod) {
	// w * sin(umax) < l
	if (yarn->width * sin(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const float3 h = normalize(om_i + om_r);

	// v_of_u is location of specular reflection.
	const float D = (h.y * cos(u) - h.z * sin(u)) /
		(sqrt(h.x * h.x + pow(h.y * sin(u) + h.z * cos(u),
		2.0f)) * tan(Radians(yarn->psi)));
	if (!(fabs(D) < 1.f))
		return 0.f;
	const float v_of_u = atan2(-h.y * sin(u) - h.z * cos(u), h.x) +
		acos(D);

	// Highlight has constant width delta_x on screen.
	const float delta_v = .5f * M_PI_F * Weave->hWidth;

	// Check if |x(v(u)) - x(v)| < delta_x/2.
	if (fabs(v_of_u - v) >= delta_v)
		return 0.f;

	// n is normal to the yarn surface.
	const float3 n = normalize((float3)(sin(v_of_u), sin(u) * cos(v_of_u),
		cos(u) * cos(v_of_u)));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, fabs(u), umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const float3 om_i_plus_om_r = om_i + om_r;
	const float Gv = a * (R + a * cos(v_of_u)) /
		(length(om_i_plus_om_r) * dot(n, h) * fabs(sin(Radians(yarn->psi))));

	// fc is phase function.
	const float fc = Weave->alpha + vonMises(-dot(om_i, om_r), Weave->beta);

	// A is attenuation function without smoothing.
	const float A = seeliger(dot(n, om_i), dot(n, om_r), 0, 1);

	// fs is scattering function.
	const float fs = Gv * fc * A;
	
	// Domain transform.
	return fs * 2.0f * umaxMod / Weave->hWidth;
}

float EvalIntegrand(__constant WeaveConfig *Weave, __constant Yarn *yarn,
        const float2 uv, float umaxMod, float3 *om_i, float3 *om_r) {
	if (yarn->yarn_type == WARP) {
		if (Radians(yarn->psi != 0.0f))
			return EvalStapleIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
		else
			return EvalFilamentIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
	} else {
		// Rotate pi/2 radians around z axis
        //swap((*om_i).x, (*om_i).y);
        float tmp = (*om_i).x;
        (*om_i).x = (*om_i).y;
        (*om_i).y = tmp;
		(*om_i).x = -(*om_i).x;

		//swap((*om_r).x, (*om_r).y);
        tmp = (*om_r).x;
        (*om_r).x = (*om_r).y;
        (*om_r).y = tmp;
		(*om_r).x = -(*om_r).x;

		if (Radians(yarn->psi != 0.0f))
			return EvalStapleIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
		else
			return EvalFilamentIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
	}
}


float EvalSpecular(__constant WeaveConfig *Weave, __constant Yarn *yarn, const float2 uv,
        float umax, const float3 wo, const float3 wi) {
	// Get incident and exitant directions.
	float3 om_i = wi;
	if (om_i.z < 0.f)
		om_i = -om_i;
	float3 om_r = wo;
	if (om_r.z < 0.f)
		om_r = -om_r;

	// Compute specular contribution.
	return EvalIntegrand(Weave, yarn, uv, umax, &om_i, &om_r);
}

float3 ClothMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 localLightDir, const float3 localEyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
    *directPdfW = fabs(localLightDir.z * M_1_PI_F);
    
    *event = GLOSSY | REFLECT;

    const ClothPreset Preset = material->cloth.Preset;
    __constant WeaveConfig *Weave = &ClothWeaves[Preset];
    const float Repeat_U = material->cloth.Repeat_U;
    const float Repeat_V = material->cloth.Repeat_V;

	float2 uv;
	float umax, scale = material->cloth.specularNormalization;
	__constant Yarn *yarn = GetYarn(Preset, Weave, Repeat_U, Repeat_V,
            hitPoint->uv.u, hitPoint->uv.v, &uv, &umax, &scale);
    
    scale = scale * EvalSpecular(Weave, yarn, uv, umax, localLightDir, localEyeDir);
	
	const uint ksIndex = yarn->yarn_type == WARP ? material->cloth.Warp_KsIndex : material->cloth.Weft_KsIndex;
	const uint kdIndex = yarn->yarn_type == WARP ? material->cloth.Warp_KdIndex : material->cloth.Weft_KdIndex;

    const float3 ksVal = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[ksIndex], hitPoint
			TEXTURES_PARAM));
    const float3 kdVal = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[kdIndex], hitPoint
			TEXTURES_PARAM));

	return (kdVal + ksVal * scale) * M_1_PI_F * fabs(localLightDir.z);
}

float3 ClothMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			(fabs(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*localSampledDir = (signbit(localFixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*absCosSampledDir = fabs((*localSampledDir).z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = GLOSSY | REFLECT;

    const ClothPreset Preset = material->cloth.Preset;
    __constant WeaveConfig *Weave = &ClothWeaves[Preset];
    const float Repeat_U = material->cloth.Repeat_U;
    const float Repeat_V = material->cloth.Repeat_V;

	float2 uv;
	float umax, scale = material->cloth.specularNormalization;

	__constant Yarn *yarn = GetYarn(Preset, Weave, Repeat_U, Repeat_V,
            hitPoint->uv.u, hitPoint->uv.v, &uv, &umax, &scale);

//	if (!hitPoint.fromLight)
	    scale = scale * EvalSpecular(Weave, yarn, uv, umax, localFixedDir, *localSampledDir);
//	else
//	    scale = scale * EvalSpecular(Weave, yarn, uv, umax, *localSampledDir, localFixedDir);

    const uint ksIndex = yarn->yarn_type == WARP ? material->cloth.Warp_KsIndex : material->cloth.Weft_KsIndex;
	const uint kdIndex = yarn->yarn_type == WARP ? material->cloth.Warp_KdIndex : material->cloth.Weft_KdIndex;

    const float3 ksVal = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[ksIndex], hitPoint
			TEXTURES_PARAM));
    const float3 kdVal = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[kdIndex], hitPoint
			TEXTURES_PARAM));

	return kdVal + ksVal * scale;
}

#endif
