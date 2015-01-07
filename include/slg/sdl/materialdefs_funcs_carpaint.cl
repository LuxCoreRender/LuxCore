#line 2 "materialdefs_funcs_carpaint.cl"

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
// CarPaint material
//
// LuxRender carpaint material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_CARPAINT)

BSDFEvent CarPaintMaterial_GetEventTypes() {
	return GLOSSY | REFLECT;
}

bool CarPaintMaterial_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 CarPaintMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 CarPaintMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 kaVal, const float d, const float3 kdVal, 
		const float3 ks1Val, const float m1, const float r1,
		const float3 ks2Val, const float m2, const float r2,
		const float3 ks3Val, const float m3, const float r3) {
	float3 H = normalize(lightDir + eyeDir);
	if (all(H == 0.f))
	{
		if (directPdfW)
			*directPdfW = 0.f;
		return BLACK;
	}
	if (H.z < 0.f)
		H = -H;

	float pdf = 0.f;
	int n = 1; // already counts the diffuse layer

	// Absorption
	const float cosi = fabs(lightDir.z);
	const float coso = fabs(eyeDir.z);
	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Diffuse layer
	float3 result = absorption * Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);

	// 1st glossy layer
	const float3 ks1 = Spectrum_Clamp(ks1Val);
	if (Spectrum_Filter(ks1) > 0.f && m1 > 0.f)
	{
		const float rough1 = m1 * m1;
		result += (SchlickDistribution_D(rough1, H, 0.f) * SchlickDistribution_G(rough1, lightDir, eyeDir) / (4.f * coso)) * (ks1 * FresnelSchlick_Evaluate(r1, dot(eyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough1, H, 0.f);
		++n;
	}
	const float3 ks2 = Spectrum_Clamp(ks2Val);
	if (Spectrum_Filter(ks2) > 0.f && m2 > 0.f)
	{
		const float rough2 = m2 * m2;
		result += (SchlickDistribution_D(rough2, H, 0.f) * SchlickDistribution_G(rough2, lightDir, eyeDir) / (4.f * coso)) * (ks2 * FresnelSchlick_Evaluate(r2, dot(eyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough2, H, 0.f);
		++n;
	}
	const float3 ks3 = Spectrum_Clamp(ks3Val);
	if (Spectrum_Filter(ks3) > 0.f && m3 > 0.f)
	{
		const float rough3 = m3 * m3;
		result += (SchlickDistribution_D(rough3, H, 0.f) * SchlickDistribution_G(rough3, lightDir, eyeDir) / (4.f * coso)) * (ks3 * FresnelSchlick_Evaluate(r3, dot(eyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough3, H, 0.f);
		++n;
	}

	// Front face: coating+base
	*event = GLOSSY | REFLECT;

	// Finish pdf computation
	pdf /= 4.f * fabs(dot(lightDir, H));
	if (directPdfW)
		*directPdfW = (pdf + fabs(lightDir.z) * M_1_PI_F) / n;

	return result;
}

float3 CarPaintMaterial_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 kaVal, const float d, const float3 kdVal, 
		const float3 ks1Val, const float m1, const float r1,
		const float3 ks2Val, const float m2, const float r2,
		const float3 ks3Val, const float m3, const float r3) {
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
		(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	// Test presence of components
	int n = 1; // already count the diffuse layer
	int sampled = 0; // sampled layer
	float3 result = BLACK;
	float pdf = 0.f;
	bool l1 = false, l2 = false, l3 = false;
	// 1st glossy layer
	const float3 ks1 = Spectrum_Clamp(ks1Val);
	if (Spectrum_Filter(ks1) > 0.f && m1 > 0.f)
	{
		l1 = true;
		++n;
	}
	// 2nd glossy layer
	const float3 ks2 = Spectrum_Clamp(ks2Val);
	if (Spectrum_Filter(ks2) > 0.f && m2 > 0.f)
	{
		l2 = true;
		++n;
	}
	// 3rd glossy layer
	const float3 ks3 = Spectrum_Clamp(ks3Val);
	if (Spectrum_Filter(ks3) > 0.f && m3 > 0.f)
	{
		l3 = true;
		++n;
	}

	float3 wh;
	float cosWH;
	if (passThroughEvent < 1.f / n) {
		// Sample diffuse layer
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &pdf);

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		// Absorption
		const float cosi = fabs(fixedDir.z);
		const float coso = fabs((*sampledDir).z);
		const float3 alpha = Spectrum_Clamp(kaVal);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Evaluate base BSDF
		result = absorption * Spectrum_Clamp(kdVal) * pdf;

		wh = normalize(*sampledDir + fixedDir);
		if (wh.z < 0.f)
			wh = -wh;
		cosWH = fabs(dot(fixedDir, wh));
	} else if (passThroughEvent < 2.f / n && l1) {
		// Sample 1st glossy layer
		sampled = 1;
		const float rough1 = m1 * m1;
		float d;
		SchlickDistribution_SampleH(rough1, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = dot(fixedDir, wh);
		*sampledDir = 2.f * cosWH * wh - fixedDir;
		*cosSampledDir = fabs((*sampledDir).z);
		cosWH = fabs(cosWH);

		if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) ||
			(fixedDir.z * (*sampledDir).z < 0.f))
			return BLACK;

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f)
			return BLACK;

		result = FresnelSchlick_Evaluate(r1, cosWH);

		const float G = SchlickDistribution_G(rough1, fixedDir, *sampledDir);
		result *= d * G / (4.f * fabs(fixedDir.z));
	} else if ((passThroughEvent < 2.f / n  ||
		(!l1 && passThroughEvent < 3.f / n)) && l2) {
		// Sample 2nd glossy layer
		sampled = 2;
		const float rough2 = m2 * m2;
		float d;
		SchlickDistribution_SampleH(rough2, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = dot(fixedDir, wh);
		*sampledDir = 2.f * cosWH * wh - fixedDir;
		*cosSampledDir = fabs((*sampledDir).z);
		cosWH = fabs(cosWH);

		if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) ||
			(fixedDir.z * (*sampledDir).z < 0.f))
			return BLACK;

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f)
			return BLACK;

		result = FresnelSchlick_Evaluate(r2, cosWH);

		const float G = SchlickDistribution_G(rough2, fixedDir, *sampledDir);
		result *= d * G / (4.f * fabs(fixedDir.z));
	} else if (l3) {
		// Sample 3rd glossy layer
		sampled = 3;
		const float rough3 = m3 * m3;
		float d;
		SchlickDistribution_SampleH(rough3, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = dot(fixedDir, wh);
		*sampledDir = 2.f * cosWH * wh - fixedDir;
		*cosSampledDir = fabs((*sampledDir).z);
		cosWH = fabs(cosWH);

		if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) ||
			(fixedDir.z * (*sampledDir).z < 0.f))
			return BLACK;

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f)
			return BLACK;

		result = FresnelSchlick_Evaluate(r3, cosWH);

		const float G = SchlickDistribution_G(rough3, fixedDir, *sampledDir);
		result *= d * G / (4.f * fabs(fixedDir.z));
	} else {
		// Sampling issue
		return BLACK;
	}
	*event = GLOSSY | REFLECT;
	// Add other components
	// Diffuse
	if (sampled != 0) {
		// Absorption
		const float cosi = fabs(fixedDir.z);
		const float coso = fabs((*sampledDir).z);
		const float3 alpha = Spectrum_Clamp(kaVal);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		const float pdf0 = fabs((*sampledDir).z) * M_1_PI_F;
		pdf += pdf0;
		result = absorption * Spectrum_Clamp(kdVal) * pdf0;
	}
	// 1st glossy
	if (l1 && sampled != 1) {
		const float rough1 = m1 * m1;
		const float d1 = SchlickDistribution_D(rough1, wh, 0.f);
		const float pdf1 = SchlickDistribution_Pdf(rough1, wh, 0.f) / (4.f * cosWH);
		if (pdf1 > 0.f) {
			result += (d1 *
				SchlickDistribution_G(rough1, fixedDir, *sampledDir) /
				(4.f * fabs(fixedDir.z))) *
				FresnelSchlick_Evaluate(r1, cosWH);
			pdf += pdf1;
		}
	}
	// 2nd glossy
	if (l2 && sampled != 2) {
		const float rough2 = m2 * m2;
		const float d2 = SchlickDistribution_D(rough2, wh, 0.f);
		const float pdf2 = SchlickDistribution_Pdf(rough2, wh, 0.f) / (4.f * cosWH);
		if (pdf2 > 0.f) {
			result += (d2 *
				SchlickDistribution_G(rough2, fixedDir, *sampledDir) /
				(4.f * fabs(fixedDir.z))) *
				FresnelSchlick_Evaluate(r2, cosWH);
			pdf += pdf2;
		}
	}
	// 3rd glossy
	if (l3 && sampled != 3) {
		const float rough3 = m3 * m3;
		const float d3 = SchlickDistribution_D(rough3, wh, 0.f);
		const float pdf3 = SchlickDistribution_Pdf(rough3, wh, 0.f) / (4.f * cosWH);
		if (pdf3 > 0.f) {
			result += (d3 *
				SchlickDistribution_G(rough3, fixedDir, *sampledDir) /
				(4.f * fabs(fixedDir.z))) *
				FresnelSchlick_Evaluate(r3, cosWH);
			pdf += pdf3;
		}
	}
	// Adjust pdf and result
	*pdfW = pdf / n;
	return result / *pdfW;
}

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)
float3 CarPaintMaterial_Evaluate(__global Material *material,
	__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
	BSDFEvent *event, float *directPdfW
	TEXTURES_PARAM_DECL) {
	const float3 kaVal = Texture_GetSpectrumValue(material->carpaint.KaTexIndex, hitPoint TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->carpaint.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kdVal = Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM);

	const float3 ks1Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float m1 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float r1 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);

	const float3 ks2Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float m2 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float r2 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);

	const float3 ks3Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float m3 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float r3 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);

	return CarPaintMaterial_ConstEvaluate(
			hitPoint, lightDir, eyeDir, event, directPdfW,
			kaVal, d, kdVal, 
			ks1Val, m1, r1,
			ks2Val, m2, r2,
			ks3Val, m3, r3);
}

float3 CarPaintMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
	TEXTURES_PARAM_DECL) {
	const float3 kaVal = Texture_GetSpectrumValue(material->carpaint.KaTexIndex, hitPoint TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->carpaint.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kdVal = Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM);

	const float3 ks1Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float m1 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float r1 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);

	const float3 ks2Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float m2 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float r2 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);

	const float3 ks3Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float m3 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float r3 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);

	return CarPaintMaterial_ConstSample(
			hitPoint, fixedDir, sampledDir,
			u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, cosSampledDir, event,
			requestedEvent,
			kaVal, d, kdVal, 
			ks1Val, m1, r1,
			ks2Val, m2, r2,
			ks3Val, m3, r3);
}
#endif

#endif
