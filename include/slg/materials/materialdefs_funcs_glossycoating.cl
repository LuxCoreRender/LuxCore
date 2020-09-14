#line 2 "materialdefs_funcs_glossycoating.cl"

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

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Albedo value is already on the stack so I have nothing to do (Pop/Push the same value would be a waste of time)
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const uint volIndex = material->interiorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	}
	// Else nothing to do because there is already the matBase volume
	// index on the stack
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const uint volIndex = material->exteriorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	}
	// Else nothing to do because there is already the matBase volume
	// index on the stack
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Nothing to do there is already the matBase pass trough
	// transparency on the stack
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	if (material->emitTexIndex != NULL_INDEX)
		DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
	// Else nothing to do because there is already the matBase emitted
	// radiance on the stack	
}

//------------------------------------------------------------------------------
// Material evaluate
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_EvaluateSetUp(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	// Save current shading normal/dpdu/dpdv and setup the hitPoint with base material bump mapping
	const float3 originalShadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 originalDpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 originalDpdv = VLOAD3F(&hitPoint->dpdv.x);
	EvalStack_PushFloat3(originalShadeN);
	EvalStack_PushFloat3(originalDpdu);
	EvalStack_PushFloat3(originalDpdv);

	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	Material_Bump(material->glossycoating.matBaseIndex, hitPointTmp
		MATERIALS_PARAM);
	// Re-read the shadeN modified by Material_Bump()
	const float3 matBaseShadeN = VLOAD3F(&hitPointTmp->shadeN.x);
	const float3 matBaseDpdu = VLOAD3F(&hitPointTmp->dpdu.x);
	const float3 matBaseDpdv = VLOAD3F(&hitPointTmp->dpdv.x);

	// Save the parameters
	EvalStack_PushFloat3(lightDir);
	EvalStack_PushFloat3(eyeDir);

	// Transform lightDir and eyeDir to base material new reference system
	Frame frameBase;
	Frame_Set_Private(&frameBase, matBaseDpdu, matBaseDpdv, matBaseShadeN);
	Frame frameOriginal;
	Frame_Set_Private(&frameOriginal, originalDpdu, originalDpdv, originalShadeN);

	const float3 matBaseLightDir = Frame_ToLocal_Private(&frameBase, Frame_ToWorld_Private(&frameOriginal, lightDir));
	const float3 matBaseEyeDir = Frame_ToLocal_Private(&frameBase, Frame_ToWorld_Private(&frameOriginal, eyeDir));
	
	// To setup the following EVAL_EVALUATE evaluation of base material
	EvalStack_PushFloat3(matBaseLightDir);
	EvalStack_PushFloat3(matBaseEyeDir);
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the base material evaluation
	float directPdfWMatBase;
	BSDFEvent eventMatBase;
	float3 resultMatBase;
	EvalStack_PopFloat(directPdfWMatBase);
	EvalStack_PopBSDFEvent(eventMatBase);
	EvalStack_PopFloat3(resultMatBase);

	// Pop the parameters
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);
	
	// Pop saved original shading normal/dpdu/dpdv
	float3 originalShadeN, originalDpdu, originalDpdv;
	EvalStack_PopFloat3(originalDpdv);
	EvalStack_PopFloat3(originalDpdu);
	EvalStack_PopFloat3(originalShadeN);
	// Restore original hitPoint
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	VSTORE3F(originalShadeN, &hitPointTmp->shadeN.x);
	VSTORE3F(originalDpdu, &hitPointTmp->dpdu.x);
	VSTORE3F(originalDpdv, &hitPointTmp->dpdv.x);

	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	// Note: this is the same side test used by matte translucent material and
	// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
	// are not available here without bump mapping.
	const float sideTest = CosTheta(lightDir) * CosTheta(eyeDir);

	if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection

		// Back face: no coating
		if (eyeDir.z <= 0.f) {
			EvalStack_PushFloat3(resultMatBase);
			EvalStack_PushBSDFEvent(eventMatBase);
			EvalStack_PushFloat(directPdfWMatBase);

			return;
		}

		const float3 baseF = resultMatBase;
		BSDFEvent event = eventMatBase;
		float directPdfW = directPdfWMatBase;

		// I have always to initialized baseF pdf and event because are used below
		if (Spectrum_IsBlack(baseF)) {
			directPdfW = 0.f;
			event = NONE;
		}

		// Front face: coating+base
		event |= GLOSSY | REFLECT;

		float3 ks = Texture_GetSpectrumValue(material->glossycoating.ksTexIndex, hitPoint TEXTURES_PARAM);
		const float i = Texture_GetFloatValue(material->glossycoating.indexTexIndex, hitPoint TEXTURES_PARAM);
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = Spectrum_Clamp(ks);

		const float nuVal = Texture_GetFloatValue(material->glossycoating.nuTexIndex, hitPoint TEXTURES_PARAM);
		const float nvVal = Texture_GetFloatValue(material->glossycoating.nvTexIndex, hitPoint TEXTURES_PARAM);
		const float u = clamp(nuVal, 1e-9f, 1.f);
		const float v = clamp(nvVal, 1e-9f, 1.f);
		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		directPdfW = wBase * directPdfW +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);

		// Absorption
		const float cosi = fabs(sampledDir.z);
		const float coso = fabs(fixedDir.z);

		const float3 alpha = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossycoating.kaTexIndex, hitPoint TEXTURES_PARAM));
		const float d = Texture_GetFloatValue(material->glossycoating.depthTexIndex, hitPoint TEXTURES_PARAM);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

		const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy,
				material->glossycoating.multibounce, fixedDir, sampledDir);

		// Blend in base layer Schlick style
		// assumes coating bxdf takes fresnel factor S into account

		const float3 result = coatingF + absorption * (WHITE - S) * baseF;
		
		EvalStack_PushFloat3(result);
		EvalStack_PushBSDFEvent(event);
		EvalStack_PushFloat(directPdfW);

		return;
	} else if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission

		const float3 baseF = resultMatBase;
		BSDFEvent event = eventMatBase;
		float directPdfW = directPdfWMatBase;

		// I have always to initialized baseF pdf because it is used below
		if (Spectrum_IsBlack(baseF)) {
			directPdfW = 0.f;
			event = NONE;
		}

		event |= GLOSSY | TRANSMIT;

		float3 ks = Texture_GetSpectrumValue(material->glossycoating.ksTexIndex, hitPoint TEXTURES_PARAM);
		const float i = Texture_GetFloatValue(material->glossycoating.indexTexIndex, hitPoint TEXTURES_PARAM);
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = Spectrum_Clamp(ks);

		const float3 fixedDir = eyeDir;
		const float wCoating = (fixedDir.z > DEFAULT_COS_EPSILON_STATIC) ? SchlickBSDF_CoatingWeight(ks, fixedDir) : 0.f;
		const float wBase = 1.f - wCoating;

		directPdfW *= wBase;

		// Absorption
		const float cosi = fabs(sampledDir.z);
		const float coso = fabs(fixedDir.z);

		const float3 alpha = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossycoating.kaTexIndex, hitPoint TEXTURES_PARAM));
		const float d = Texture_GetFloatValue(material->glossycoating.depthTexIndex, hitPoint TEXTURES_PARAM);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const float3 H = normalize(MAKE_FLOAT3 (sampledDir.x + fixedDir.x, sampledDir.y + fixedDir.y,
				sampledDir.z - fixedDir.z));
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(fixedDir, H)));

		// Filter base layer, the square root is just a heuristic
		// so that a sheet coated on both faces gets a filtering factor
		// of 1-S like a reflection
		const float3 result = absorption * Spectrum_Sqrt(WHITE - S) * baseF;
		
		EvalStack_PushFloat3(result);
		EvalStack_PushBSDFEvent(event);
		EvalStack_PushFloat(directPdfW);

		return;
	} else {
		MATERIAL_EVALUATE_RETURN_BLACK;
	}
}

//------------------------------------------------------------------------------
// Material sample
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_SampleSetUp(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	// Save the parameters
	EvalStack_PushFloat3(fixedDir);
	EvalStack_PushFloat(u0);
	EvalStack_PushFloat(u1);
	EvalStack_PushFloat(passThroughEvent);

	float3 ks = Texture_GetSpectrumValue(material->glossycoating.ksTexIndex, hitPoint TEXTURES_PARAM);
	const float i = Texture_GetFloatValue(material->glossycoating.indexTexIndex, hitPoint TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	// Save ks
	EvalStack_PushFloat3(ks);

	const float nuVal = Texture_GetFloatValue(material->glossycoating.nuTexIndex, hitPoint TEXTURES_PARAM);
	const float nvVal = Texture_GetFloatValue(material->glossycoating.nvTexIndex, hitPoint TEXTURES_PARAM);
	const float u = clamp(nuVal, 1e-9f, 1.f);
	const float v = clamp(nvVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	// Save roughness and anisotropy
	EvalStack_PushFloat(roughness);
	EvalStack_PushFloat(anisotropy);

	// Coating is used only on the front face
	const float wCoating = (fixedDir.z > DEFAULT_COS_EPSILON_STATIC) ? SchlickBSDF_CoatingWeight(ks, fixedDir) : 0.f;
	const float wBase = 1.f - wCoating;

	// Save wCoating
	EvalStack_PushFloat(wCoating);

	// Save current shading normal/dpdu/dpdv and setup the hitPoint with base material bump mapping
	const float3 originalShadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 originalDpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 originalDpdv = VLOAD3F(&hitPoint->dpdv.x);
	EvalStack_PushFloat3(originalShadeN);
	EvalStack_PushFloat3(originalDpdu);
	EvalStack_PushFloat3(originalDpdv);

	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	Material_Bump(material->glossycoating.matBaseIndex, hitPointTmp
		MATERIALS_PARAM);
	// Re-read the shadeN modified by Material_Bump()
	const float3 matBaseShadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 matBaseDpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 matBaseDpdv = VLOAD3F(&hitPoint->dpdv.x);

	if (passThroughEvent < wBase) {
		// Sample base BSDF

		// Transform fixedDir to base material new reference system
		Frame frameBase;
		Frame_Set_Private(&frameBase, matBaseDpdu, matBaseDpdv, matBaseShadeN);
		Frame frameOriginal;
		Frame_Set_Private(&frameOriginal, originalDpdu, originalDpdv, originalShadeN);

		const float3 matBaseFixedDir = Frame_ToLocal_Private(&frameBase, Frame_ToWorld_Private(&frameOriginal, fixedDir));

		// To setup the following EVAL_SAMPLE evaluation of base material
		EvalStack_PushFloat3(matBaseFixedDir);
		EvalStack_PushFloat(u0);
		EvalStack_PushFloat(u1);
		const float passThroughEventMatBase = passThroughEvent / wBase;
		EvalStack_PushFloat(passThroughEventMatBase);
	
		// To setup the following EVAL_CONDITIONAL_GOTO evaluation
		EvalStack_PushInt(false);
	} else {
		// Sample coating BSDF (Schlick BSDF)
		float3 sampledDir;
		float coatingPdf;
		const float3 coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				material->glossycoating.multibounce, fixedDir, &sampledDir, u0, u1, &coatingPdf);

		// Save coatingF, coatingPdf and sampledDir
		EvalStack_PushFloat3(coatingF);
		EvalStack_PushFloat(coatingPdf);
		EvalStack_PushFloat3(sampledDir);

		// Transform lightDir and eyeDir to base material new reference system
		Frame frameBase;
		Frame_Set_Private(&frameBase, matBaseDpdu, matBaseDpdv, matBaseShadeN);
		Frame frameOriginal;
		Frame_Set_Private(&frameOriginal, originalDpdu, originalDpdv, originalShadeN);

		const float3 matBaseFixedDir = Frame_ToLocal_Private(&frameBase, Frame_ToWorld_Private(&frameOriginal, fixedDir));
		const float3 matBaseSampleDir = Frame_ToLocal_Private(&frameBase, Frame_ToWorld_Private(&frameOriginal, sampledDir));

		// To setup the following EVAL_EVALUATE evaluation of base material
		EvalStack_PushFloat3(matBaseSampleDir);
		EvalStack_PushFloat3(matBaseFixedDir);

		// To setup the following EVAL_CONDITIONAL_GOTO evaluation
		EvalStack_PushInt(true);		
	}
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_Sample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset,
		const float wBase, const float wCoating,
		const float3 baseF, const float basePdf,
		const float3 coatingF, const float coatingPdf,
		const float3 fixedDir, const float3 sampledDir,
		const BSDFEvent event,	const float3 ks
		MATERIALS_PARAM_DECL) {
	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

	const float3 alpha = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossycoating.kaTexIndex, hitPoint TEXTURES_PARAM));
	const float d = Texture_GetFloatValue(material->glossycoating.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Note: this is the same side test used by matte translucent material and
	// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
	// are not available here without bump mapping.
	const float sideTest = CosTheta(fixedDir) * CosTheta(sampledDir);
	float3 result;
	if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection

		if (!(fixedDir.z > 0.f)) {
			// Back face reflection: no coating
			result = baseF;
		} else {
			// Front face reflection: coating+base

			// Coating fresnel factor
			const float3 H = normalize(fixedDir + sampledDir);
			const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

			// blend in base layer Schlick style
			// coatingF already takes fresnel factor S into account
			result = (coatingF + absorption * (WHITE - S) * baseF);
		}
	} else if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission
		// Coating fresnel factor
		float3 H = MAKE_FLOAT3((sampledDir).x + fixedDir.x, (sampledDir).y + fixedDir.y,
			(sampledDir).z - fixedDir.z);
		const float HLength = dot(H, H);
		
		float3 S;
		// I have to handle the case when HLength is 0.0 (or nearly 0.f) in
		// order to avoid NaN
		if (HLength < DEFAULT_EPSILON_STATIC)
			S = ZERO;
		else {
			// Normalize
			H *= 1.f / HLength;
			S = FresnelSchlick_Evaluate(ks, fabs(dot(fixedDir, H)));
		}

		// Filter base layer, the square root is just a heuristic
		// so that a sheet coated on both faces gets a filtering factor
		// of 1-S like a reflection
		result = absorption * Spectrum_Sqrt(WHITE - S) * baseF;
	} else {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}
	
	const float pdfW = coatingPdf * wCoating + basePdf * wBase;
	result /= pdfW;

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_SampleMatBaseSample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the base material sample
	float3 resultMatBase, sampledDirMatBase;
	float pdfWMatBase;
	BSDFEvent eventMatBase;
	EvalStack_PopBSDFEvent(eventMatBase);
	EvalStack_PopFloat(pdfWMatBase);
	EvalStack_PopFloat3(sampledDirMatBase);
	EvalStack_PopFloat3(resultMatBase);

	// Pop saved original shading normal/dpdu/dpdv
	float3 originalShadeN, originalDpdu, originalDpdv;
	EvalStack_PopFloat3(originalDpdv);
	EvalStack_PopFloat3(originalDpdu);
	EvalStack_PopFloat3(originalShadeN);
	// Restore original hitPoint
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	VSTORE3F(originalShadeN, &hitPointTmp->shadeN.x);
	VSTORE3F(originalDpdu, &hitPointTmp->dpdu.x);
	VSTORE3F(originalDpdv, &hitPointTmp->dpdv.x);

	// Pop wCoating
	float wCoating;
	EvalStack_PopFloat(wCoating);
	const float wBase = 1.f - wCoating;

	// Pop roughness and anisotropy
	float roughness, anisotropy;
	EvalStack_PopFloat(anisotropy);
	EvalStack_PopFloat(roughness);

	// Pop ks
	float3 ks;
	EvalStack_PopFloat3(ks);

	// Pop parameters
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	//--------------------------------------------------------------------------
	
	if (Spectrum_IsBlack(resultMatBase)) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const float3 baseF = resultMatBase * pdfWMatBase;
	const float basePdf = pdfWMatBase;
	const float3 sampledDir = sampledDirMatBase;
	const BSDFEvent event = pdfWMatBase;

	// Don't add the coating scattering if the base sampled
	// component is specular
	float3 coatingF;
	float coatingPdf;
	if (!(event & SPECULAR)) {
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy,
				material->glossycoating.multibounce, fixedDir, sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	} else {
		coatingF = BLACK;
		coatingPdf = 0.f;
	}

	//--------------------------------------------------------------------------

	GlossyCoatingMaterial_Sample(material, hitPoint, evalStack, evalStackOffset,
		wBase, wCoating,
		baseF, basePdf,
		coatingF, coatingPdf,
		fixedDir, sampledDir,
		event, ks
		MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_SampleMatBaseEvaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the base material evaluate
	float3 resultMatBase;
	BSDFEvent eventMatBase;
	float directPdfWMatBase;
	EvalStack_PopFloat(directPdfWMatBase);
	EvalStack_PopBSDFEvent(eventMatBase);
	EvalStack_PopFloat3(resultMatBase);

	// Save coatingF and coatingPdf
	float3 coatingF;
	float coatingPdf;
	float3 sampledDir;
	EvalStack_PopFloat3(sampledDir);
	EvalStack_PopFloat(coatingPdf);
	EvalStack_PopFloat3(coatingF);

	// Pop saved original shading normal/dpdu/dpdv
	float3 originalShadeN, originalDpdu, originalDpdv;
	EvalStack_PopFloat3(originalDpdv);
	EvalStack_PopFloat3(originalDpdu);
	EvalStack_PopFloat3(originalShadeN);
	// Restore original hitPoint
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	VSTORE3F(originalShadeN, &hitPointTmp->shadeN.x);
	VSTORE3F(originalDpdu, &hitPointTmp->dpdu.x);
	VSTORE3F(originalDpdv, &hitPointTmp->dpdv.x);

	// Pop wCoating
	float wCoating;
	EvalStack_PopFloat(wCoating);
	const float wBase = 1.f - wCoating;

	// Pop roughness and anisotropy
	float roughness, anisotropy;
	EvalStack_PopFloat(anisotropy);
	EvalStack_PopFloat(roughness);

	// Pop ks
	float3 ks;
	EvalStack_PopFloat3(ks);

	// Pop parameters
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);
	
	//--------------------------------------------------------------------------

	if (Spectrum_IsBlack(coatingF)) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	if (fabs(CosTheta(sampledDir)) < DEFAULT_COS_EPSILON_STATIC) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	coatingF *= coatingPdf;
	const float3 baseF = resultMatBase;
	float basePdf = directPdfWMatBase;
	// I have always to initialized basePdf because it is used below
	if (Spectrum_IsBlack(baseF))
		basePdf = 0.f;

	const BSDFEvent event = GLOSSY | REFLECT;

	//--------------------------------------------------------------------------
	
	GlossyCoatingMaterial_Sample(material, hitPoint, evalStack, evalStackOffset,
		wBase, wCoating,
		baseF, basePdf,
		coatingF, coatingPdf,
		fixedDir, sampledDir,
		event, ks
		MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void GlossyCoatingMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			GlossyCoatingMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			GlossyCoatingMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			GlossyCoatingMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			GlossyCoatingMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			GlossyCoatingMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE_GLOSSYCOATING_SETUP:
			GlossyCoatingMaterial_EvaluateSetUp(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			GlossyCoatingMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE_GLOSSYCOATING_SETUP:
			GlossyCoatingMaterial_SampleSetUp(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE_GLOSSYCOATING_CLOSE_SAMPLE_BASE:
			GlossyCoatingMaterial_SampleMatBaseSample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE_GLOSSYCOATING_CLOSE_EVALUATE_BASE:
			GlossyCoatingMaterial_SampleMatBaseEvaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
