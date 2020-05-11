#line 2 "materialdefs_funcs_disney.cl"

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

//------------------------------------------------------------------------------
// Disney BRDF material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void DisneyMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
    const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->disney.baseColorTexIndex, hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void DisneyMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void DisneyMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void DisneyMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void DisneyMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DisneyMaterial_CalculateTint(const float3 color) {
	float luminance = color.x * 0.3f + color.y * 0.6f + color.z * 1.0f;
	return (luminance > 0.0f) ? color * (1.0f / luminance) : WHITE;
}

OPENCL_FORCE_INLINE float DisneyMaterial_GTR1(float NdotH, float a) {
	if (a >= 1.0f)
		return M_1_PI_F;

	float a2 = a * a;
	float t = 1.0f + (a2 - 1.0f) * (NdotH * NdotH);

	return (a2 - 1.0f) / (M_PI_F * log(a2) * t);
}

OPENCL_FORCE_INLINE float DisneyMaterial_GTR2_Aniso(float NdotH, float HdotX, float HdotY,
		float ax, float ay) {
	return 1.0f / (M_PI_F * ax * ay * Sqr(Sqr(HdotX / ax) + Sqr(HdotY / ay) + NdotH * NdotH));
}

OPENCL_FORCE_INLINE float DisneyMaterial_SmithG_GGX_Aniso(float NdotV, float VdotX, float VdotY,
		float ax, float ay) {
	return 1.0f / (NdotV + sqrt(Sqr(VdotX * ax) + Sqr(VdotY * ay) + Sqr(NdotV)));
}

OPENCL_FORCE_INLINE float DisneyMaterial_SmithG_GGX(float NdotV, float alphaG) {
	const float a = alphaG * alphaG;
	const float b = NdotV * NdotV;

	return 1.0f / (fabs(NdotV) + max(sqrt(a + b - a * b), 0.0001f));
}

OPENCL_FORCE_INLINE float DisneyMaterial_Schlick_Weight(const float cosi) {
	return pow(1.f - cosi, 5.f);
}

OPENCL_FORCE_INLINE void DisneyMaterial_Anisotropic_Params(const float anisotropic,
		const float roughness, float *ax, float *ay) {
	const float aspect = sqrt(1.0f - 0.9f * anisotropic);
	*ax = fmax(0.001f, Sqr(roughness) / aspect);
	*ay = fmax(0.001f, Sqr(roughness) * aspect);
}

OPENCL_FORCE_INLINE void DisneyMaterial_ComputeRatio(const float metallic, const float clearcoat,
		float *ratioGlossy, float *ratioDiffuse, float *ratioClearcoat) {
	const float metallicBRDF = metallic;
	const float dielectricBRDF = (1.0f - metallic);

	const float specularWeight = metallicBRDF + dielectricBRDF;
	const float diffuseWeight = dielectricBRDF;
	const float clearcoatWeight = clearcoat;

	const float norm = 1.0f / (specularWeight + diffuseWeight + clearcoatWeight);

	*ratioGlossy = specularWeight * norm;
	*ratioDiffuse = diffuseWeight * norm;
	*ratioClearcoat = clearcoatWeight * norm;
}

//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float DisneyMaterial_DiffusePdf(const float3 wi, const float3 wo) {
	return fabs(CosTheta(wi)) * M_1_PI_F;
}

OPENCL_FORCE_INLINE float DisneyMaterial_MetallicPdf(const float anisotropic,
		const float roughness, const float3 wi, const float3 wo) {
	const float3 wh = normalize(wo + wi);

	float ax, ay;
	DisneyMaterial_Anisotropic_Params(anisotropic, roughness, &ax, &ay);

	const float ax2 = ax * ax;
	const float ay2 = ay * ay;

	const float HdotX = wh.x;
	const float HdotY = wh.y;
	const float NdotH = fabs(CosTheta(wh));

	const float denom = HdotX * HdotX / ax2 + HdotY * HdotY / ay2 + NdotH * NdotH;
	if (denom == 0.0f)
		return 0.0f;

	const float pdfDistribution = NdotH / (M_PI_F * ax * ay * denom * denom);

	return pdfDistribution / (4.0f * dot(wo, wh));
}

OPENCL_FORCE_INLINE float DisneyMaterial_ClearcoatPdf(const float clearcoatGloss,
		const float3 wi, const float3 wo) {
	const float3 wh = normalize(wo + wi);

	const float NdotH = fabs(CosTheta(wh));
	const float Dr = DisneyMaterial_GTR1(NdotH, Lerp(clearcoatGloss, 0.1f, 0.001f));

	return Dr * NdotH / (4.0f * dot(wo, wh));
}

OPENCL_FORCE_INLINE float DisneyMaterial_DisneyPdf(const float roughness, const float metallic,
		const float clearcoat, const float clearcoatGloss, const float anisotropic,
		const float3 localLightDir, const float3 localEyeDir) {
	if (CosTheta(localLightDir) * CosTheta(localEyeDir) <= 0.0f)
		return 0.0f;

	const float3 wi = localLightDir;
	const float3 wo = localEyeDir;

	float ratioGlossy, ratioDiffuse, ratioClearcoat;
	DisneyMaterial_ComputeRatio(metallic, clearcoat,
			&ratioGlossy, &ratioDiffuse, &ratioClearcoat);

	const float pdfDiffuse = ratioDiffuse * DisneyMaterial_DiffusePdf(wi, wo);
	const float pdfMicrofacet = ratioGlossy * DisneyMaterial_MetallicPdf(anisotropic, roughness, wi, wo);
	const float pdfClearcoat = ratioClearcoat * DisneyMaterial_ClearcoatPdf(clearcoatGloss, wi, wo);

	return pdfDiffuse + pdfMicrofacet + pdfClearcoat;
}

//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneyDiffuse(const float3 color,
		const float roughness,
		float NdotL, float NdotV, float LdotH) {
	const float FL = DisneyMaterial_Schlick_Weight(NdotL);
	const float FV = DisneyMaterial_Schlick_Weight(NdotV);

	const float Fd90 = 0.5f + 2.0f * (LdotH * LdotH) * (roughness * roughness);
	const float Fd = Lerp(FL, 1.0f, Fd90) * Lerp(FV, 1.0f, Fd90);

	return M_1_PI_F * Fd * color;
}

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneySubsurface(const float3 color,
		const float roughness,
		float NdotL, float NdotV, float LdotH) {

	const float FL = DisneyMaterial_Schlick_Weight(NdotL);
	const float FV = DisneyMaterial_Schlick_Weight(NdotV);

	const float Fss90 = LdotH * LdotH * roughness;
	const float Fss = Lerp(FL, 1.0f, Fss90) * Lerp(FV, 1.0f, Fss90);
	const float ss = 1.25f * (Fss * (1.0f / (NdotL + NdotV) - 0.5f) + 0.5f);

	return M_1_PI_F * ss * color;
}

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneyMetallic(const float3 color,
		const float specular, const float specularTint, const float metallic,
		const float anisotropic, const float roughness,
		const float NdotL, const float NdotV, const float NdotH,
		const float LdotH, const float VdotH,
		const float3 wi, const float3 wo, const float3 H) {
	const float3 Ctint = DisneyMaterial_CalculateTint(color);

	const float3 CSpecTint = specular * 0.08f * Lerp3(specularTint, WHITE, Ctint);
	const float3 Cspec0 = Lerp3(metallic, CSpecTint, color);

	float ax, ay;
	DisneyMaterial_Anisotropic_Params(anisotropic, roughness, &ax, &ay);

	const float Ds = DisneyMaterial_GTR2_Aniso(NdotH, H.x, H.y, ax, ay);

	const float FH = DisneyMaterial_Schlick_Weight(LdotH);

	const float3 Fs = Lerp3(FH, Cspec0, WHITE);

	const float Gl = DisneyMaterial_SmithG_GGX_Aniso(NdotL, wi.x, wi.y, ax, ay);
	const float Gv = DisneyMaterial_SmithG_GGX_Aniso(NdotV, wo.x, wo.y, ax, ay);

	const float Gs = Gl * Gv;

	return Gs * Fs * Ds;
}

OPENCL_FORCE_INLINE float DisneyMaterial_DisneyClearCoat(const float clearcoat,
		const float clearcoatGloss,	float NdotL, float NdotV, float NdotH, float LdotH) {
	const float gloss = Lerp(clearcoatGloss, 0.1f, 0.001f);

	const float Dr = DisneyMaterial_GTR1(fabs(NdotH), gloss);
	const float FH = DisneyMaterial_Schlick_Weight(LdotH);
	const float Fr = Lerp(FH, 0.04f, 1.0f);
	const float Gr = DisneyMaterial_SmithG_GGX(NdotL, 0.25f) * DisneyMaterial_SmithG_GGX(NdotV, 0.25f);

	return clearcoat * Fr * Gr * Dr;
}

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneySheen(const float3 color,
		const float sheen, const float sheenTint,
		float LdotH) {
	const float FH = DisneyMaterial_Schlick_Weight(LdotH);

	const float3 Ctint = DisneyMaterial_CalculateTint(color);
	const float3 Csheen = Lerp3(sheenTint, WHITE, Ctint);

	return clamp(FH * sheen * Csheen, 0.f, 1.f);
}

OPENCL_FORCE_INLINE float3 DisneyMaterial_EvaluateImpl(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 colorVal, const float subsurfaceVal, const float roughnessVal,
		const float metallicVal, const float specularVal, const float specularTintVal,
		const float clearcoatVal, const float clearcoatGlossVal, const float anisotropicGlossVal,
		const float sheenVal, const float sheenTintVal, 
		const float localFilmAmount, const float localFilmThickness, const float localFilmIor) {
	const float3 wo = eyeDir; 
	const float3 wi = lightDir;

	const float NdotL = fabs(CosTheta(wi));
	const float NdotV = fabs(CosTheta(wo));
	
	if (NdotL < DEFAULT_COS_EPSILON_STATIC || NdotV < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 color = Spectrum_Clamp(colorVal);
	const float subsurface = clamp(subsurfaceVal, 0.0f, 1.0f);
	const float roughness = clamp(roughnessVal, 0.0f, 1.0f);
	const float metallic = clamp(metallicVal, 0.0f, 1.0f);
	const float specular = clamp(specularVal, 0.0f, 1.0f);
	const float specularTint = clamp(specularTintVal, 0.0f, 1.0f);
	const float clearcoat = clamp(clearcoatVal, 0.0f, 1.0f);
	const float clearcoatGloss = clamp(clearcoatGlossVal, 0.0f, 1.0f);
	const float anisotropicGloss = clamp(anisotropicGlossVal, 0.0f, 1.0f);
	// I allow sheen values > 1.0 to accentuate the effect. The result is still
	// clamped between 0.0 and 1.0 to not break the energy conservation law.
	const float sheen = sheenVal;
	const float sheenTint = clamp(sheenTintVal, 0.0f, 1.0f);

	const float3 H = normalize(wo + wi);

	const float NdotH = fabs(CosTheta(H));
	const float LdotH = dot(wi, H);
	const float VdotH = dot(wo, H);

	const float3 diffuseEval = DisneyMaterial_DisneyDiffuse(color, roughness, NdotL, NdotV, LdotH);

	const float3 subsurfaceEval = DisneyMaterial_DisneySubsurface(color, roughness, NdotL, NdotV, LdotH);

	float3 metallicEval = DisneyMaterial_DisneyMetallic(color, specular, specularTint, metallic, anisotropicGloss, roughness,
														NdotL, NdotV, NdotH, LdotH, VdotH, wi, wo, H);
	if (localFilmThickness > 0.f) {
		const float3 metallicWithFilmColor = metallicEval * CalcFilmColor(wo, localFilmThickness, localFilmIor);
		metallicEval = Lerp3(localFilmAmount, metallicEval, metallicWithFilmColor);
	}
	
	const float clearCoatEval = DisneyMaterial_DisneyClearCoat(clearcoat, clearcoatGloss,
																NdotL, NdotV, NdotH, LdotH);
	const float3 glossyEval = metallicEval + clearCoatEval;

	const float3 sheenEval = DisneyMaterial_DisneySheen(color, sheen, sheenTint, LdotH);

	if (directPdfW)
		*directPdfW = DisneyMaterial_DisneyPdf(roughness, metallic, clearcoat, clearcoatGloss, anisotropicGloss, lightDir, eyeDir);

	*event = GLOSSY | REFLECT;

	const float3 f = (Lerp3(subsurface, diffuseEval, subsurfaceEval) + sheenEval) * (1.0f - metallic) + glossyEval;
	
	return f * fabs(NdotL);
}

OPENCL_FORCE_INLINE void DisneyMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);
	
	const float localFilmAmount = (material->disney.filmAmountTexIndex != NULL_INDEX) 
								  ? clamp(Texture_GetFloatValue(material->disney.filmAmountTexIndex, hitPoint TEXTURES_PARAM), 0.f, 1.f) : 1.f;
	const float localFilmThickness = (material->disney.filmThicknessTexIndex != NULL_INDEX) 
									 ? Texture_GetFloatValue(material->disney.filmThicknessTexIndex, hitPoint TEXTURES_PARAM) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && material->disney.filmIorTexIndex != NULL_INDEX) 
							   ? Texture_GetFloatValue(material->disney.filmIorTexIndex, hitPoint TEXTURES_PARAM) : 1.f;

	BSDFEvent event;
	float directPdfW;
	const float3 result = DisneyMaterial_EvaluateImpl(hitPoint, lightDir, eyeDir,
		&event, &directPdfW,
		Texture_GetSpectrumValue(material->disney.baseColorTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.subsurfaceTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.roughnessTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.metallicTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.specularTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.specularTintTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.clearcoatTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.clearcoatGlossTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.anisotropicTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.sheenTexIndex, hitPoint TEXTURES_PARAM),
		Texture_GetFloatValue(material->disney.sheenTintTexIndex, hitPoint TEXTURES_PARAM),
		localFilmAmount, localFilmThickness, localFilmIor);

	if (Spectrum_IsBlack(result)) {
		MATERIAL_EVALUATE_RETURN_BLACK;
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneyDiffuseSample(const float3 wo,
		const float u0, const float u1) {
	return Sgn(CosTheta(wo)) * CosineSampleHemisphere(u0, u1);
}

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneyMetallicSample(const float anisotropic,
		const float roughness, const float3 wo,
		const float u0, const float u1) {
	float ax, ay;
	DisneyMaterial_Anisotropic_Params(anisotropic, roughness, &ax, &ay);

	float phi = atan(ay / ax * tan(2.0f * M_PI_F * u1 + 0.5f * M_PI_F));
	if (u1 > 0.5f)
		phi += M_PI_F;

	const float sinPhi = sin(phi), cosPhi = cos(phi);
	const float ax2 = ax * ax, ay2 = ay * ay;
	const float alpha2 = 1.0f / (cosPhi * cosPhi / ax2 + sinPhi * sinPhi / ay2);
	const float tanTheta2 = alpha2 * u0 / (1.0f - u0);
	const float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);

	const float sinTheta = sqrt(fmax(0.0f, 1.0f - cosTheta * cosTheta));
	float3 wh = MAKE_FLOAT3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
		wh *= MAKE_FLOAT3(-1.f, -1.f, -1.f);

	return normalize(2.0f * dot(wh, wo) * wh - wo);
}

OPENCL_FORCE_INLINE float3 DisneyMaterial_DisneyClearcoatSample(const float clearcoatGloss,
		const float3 wo, float u0, float u1) {
	const float gloss = Lerp(clearcoatGloss, 0.1f, 0.001f);
	const float alpha2 = gloss * gloss;
	const float cosTheta = sqrt(fmax(0.0001f, (1.0f - pow(alpha2, 1.0f - u0)) / (1.0f - alpha2)));
	const float sinTheta = sqrt(fmax(0.0001f, 1.0f - cosTheta * cosTheta));
	const float phi = 2.0f * M_PI_F * u1;

	float3 wh = MAKE_FLOAT3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
		wh *= MAKE_FLOAT3(-1.f, -1.f, -1.f);

	return normalize(2.0f * dot(wh, wo) * wh - wo);
}

OPENCL_FORCE_INLINE void DisneyMaterial_Sample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	const float3 colorVal = Texture_GetSpectrumValue(material->disney.baseColorTexIndex, hitPoint TEXTURES_PARAM);
	const float subsurfaceVal = Texture_GetFloatValue(material->disney.subsurfaceTexIndex, hitPoint TEXTURES_PARAM);
	const float roughnessVal = Texture_GetFloatValue(material->disney.roughnessTexIndex, hitPoint TEXTURES_PARAM);
	const float metallicVal = Texture_GetFloatValue(material->disney.metallicTexIndex, hitPoint TEXTURES_PARAM);
	const float specularVal = Texture_GetFloatValue(material->disney.specularTexIndex, hitPoint TEXTURES_PARAM);
	const float specularTintVal = Texture_GetFloatValue(material->disney.specularTintTexIndex, hitPoint TEXTURES_PARAM);
	const float clearcoatVal = Texture_GetFloatValue(material->disney.clearcoatTexIndex, hitPoint TEXTURES_PARAM);
	const float clearcoatGlossVal = Texture_GetFloatValue(material->disney.clearcoatGlossTexIndex, hitPoint TEXTURES_PARAM);
	const float anisotropicGlossVal = Texture_GetFloatValue(material->disney.anisotropicTexIndex, hitPoint TEXTURES_PARAM);
	const float sheenVal = Texture_GetFloatValue(material->disney.sheenTexIndex, hitPoint TEXTURES_PARAM);
	const float sheenTintVal = Texture_GetFloatValue(material->disney.sheenTintTexIndex, hitPoint TEXTURES_PARAM);

	const float roughness = clamp(roughnessVal, 0.0f, 1.0f);
	const float metallic = clamp(metallicVal, 0.0f, 1.0f);
	const float clearcoat = clamp(clearcoatVal, 0.0f, 1.0f);
	const float clearcoatGloss = clamp(clearcoatGlossVal, 0.0f, 1.0f);
	const float anisotropicGloss = clamp(anisotropicGlossVal, 0.0f, 1.0f);

	const float3 wo = fixedDir;

	float ratioGlossy, ratioDiffuse, ratioClearcoat;
	DisneyMaterial_ComputeRatio(metallic, clearcoat,
			&ratioGlossy, &ratioDiffuse, &ratioClearcoat);

	float3 sampledDir;
	if (passThroughEvent <= ratioGlossy)
		sampledDir = DisneyMaterial_DisneyMetallicSample(anisotropicGloss, roughness, wo, u0, u1);
	else if (passThroughEvent > ratioGlossy &&  passThroughEvent <= ratioGlossy + ratioClearcoat)
		sampledDir = DisneyMaterial_DisneyClearcoatSample(clearcoatGloss, wo, u0, u1);
	else if (passThroughEvent > ratioGlossy + ratioClearcoat && passThroughEvent <= ratioGlossy + ratioClearcoat + ratioDiffuse)
		sampledDir = DisneyMaterial_DisneyDiffuseSample(wo, u0, u1);
	else {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}
	
	const float3 localLightDir = sampledDir;
	const float3 localEyeDir = fixedDir;

	const float pdfW = DisneyMaterial_DisneyPdf(roughnessVal, metallicVal, clearcoatVal, clearcoatGlossVal, anisotropicGlossVal,
			localLightDir, localEyeDir);
	if (pdfW < 0.0001f) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}
	
	const float localFilmAmount = (material->disney.filmAmountTexIndex != NULL_INDEX) 
								  ? clamp(Texture_GetFloatValue(material->disney.filmAmountTexIndex, hitPoint TEXTURES_PARAM), 0.f, 1.f) : 1.f;
	const float localFilmThickness = (material->disney.filmThicknessTexIndex != NULL_INDEX) 
									 ? Texture_GetFloatValue(material->disney.filmThicknessTexIndex, hitPoint TEXTURES_PARAM) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && material->disney.filmIorTexIndex != NULL_INDEX) 
							   ? Texture_GetFloatValue(material->disney.filmIorTexIndex, hitPoint TEXTURES_PARAM) : 1.f;

	BSDFEvent event;
	const float3 f = DisneyMaterial_EvaluateImpl(hitPoint, localLightDir, localEyeDir, &event, NULL,
			colorVal, subsurfaceVal, roughnessVal, metallicVal, specularVal, specularTintVal,
			clearcoatVal, clearcoatGlossVal, anisotropicGlossVal, sheenVal, sheenTintVal,
			localFilmAmount, localFilmThickness, localFilmIor);

	const float3 result = f / pdfW;

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void DisneyMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			DisneyMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			DisneyMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			DisneyMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			DisneyMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			DisneyMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			DisneyMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			DisneyMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
