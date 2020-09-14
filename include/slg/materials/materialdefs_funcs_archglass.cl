#line 2 "materialdefs_funcs_archglass.cl"

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

OPENCL_FORCE_INLINE float ExtractExteriorIors(__global const HitPoint *hitPoint, const uint exteriorIorTexIndex
		TEXTURES_PARAM_DECL) {
	uint extIndex = NULL_INDEX;
	if (exteriorIorTexIndex != NULL_INDEX)
		extIndex = exteriorIorTexIndex;
	else {
		const uint hitPointExteriorIorTexIndex = hitPoint->exteriorIorTexIndex;
		if (hitPointExteriorIorTexIndex != NULL_INDEX)
			extIndex = hitPointExteriorIorTexIndex;
	}
	return (extIndex == NULL_INDEX) ? 1.f : Texture_GetFloatValue(extIndex, hitPoint
			TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE float ExtractInteriorIors(__global const HitPoint *hitPoint, const uint interiorIorTexIndex
		TEXTURES_PARAM_DECL) {
	uint intIndex = NULL_INDEX;
	if (interiorIorTexIndex != NULL_INDEX)
		intIndex = interiorIorTexIndex;
	else {
		const uint hitPointInteriorIorTexIndex = hitPoint->interiorIorTexIndex;
		if (hitPointInteriorIorTexIndex != NULL_INDEX)
			intIndex = hitPointInteriorIorTexIndex;
	}
	return (intIndex == NULL_INDEX) ? 1.f : Texture_GetFloatValue(intIndex, hitPoint
			TEXTURES_PARAM);
}

//------------------------------------------------------------------------------
// ArchGlass material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ArchGlassMaterial_EvalSpecularReflection(__global const HitPoint *hitPoint,
		const float3 localFixedDir, const float3 kr,
		const float nc, const float nt,
		float3 *sampledDir, const float localFilmThickness, const float localFilmIor) {
	if (Spectrum_IsBlack(kr))
		return BLACK;

	const float costheta = CosTheta(localFixedDir);
	if (costheta <= 0.f)
		return BLACK;

	*sampledDir = MAKE_FLOAT3(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);

	const float ntc = nt / nc;
	const float3 result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	
	if (localFilmThickness > 0.f) {
		const float3 filmColor = CalcFilmColor(localFixedDir, localFilmThickness, localFilmIor);
		return result * filmColor;
	}
	return result;
}

OPENCL_FORCE_INLINE float3 ArchGlassMaterial_EvalSpecularTransmission(__global const HitPoint *hitPoint,
		const float3 localFixedDir, const float3 kt,
		const float nc, const float nt, float3 *sampledDir) {
	if (Spectrum_IsBlack(kt))
		return BLACK;

	// Note: there can not be total internal reflection for 
	
	*sampledDir = -localFixedDir;

	const float ntc = nt / nc;
	const float costheta = CosTheta(localFixedDir);
	const bool entering = (costheta > 0.f);
	float ce;
//	if (!hitPoint.fromLight) {
		if (entering)
			ce = 0.f;
		else
			ce = FresnelCauchy_Evaluate(ntc, -costheta);
//	} else {
//		if (entering)
//			ce = FresnelTexture::CauchyEvaluate(ntc, costheta);
//		else
//			ce = 0.f;
//	}
	const float factor = 1.f - ce;
	const float result = (1.f + factor * factor) * ce;

	return (1.f - result) * kt;
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
    const float3 albedo = WHITE;

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint
		TEXTURES_PARAM));

	const float nc = Spectrum_Filter(TO_FLOAT3(ExtractExteriorIors(hitPoint,
			material->archglass.exteriorIorTexIndex
			TEXTURES_PARAM)));
	const float nt = Spectrum_Filter(TO_FLOAT3(ExtractInteriorIors(hitPoint,
			material->archglass.interiorIorTexIndex
			TEXTURES_PARAM)));

	float3 transLocalSampledDir; 
	const float3 trans = ArchGlassMaterial_EvalSpecularTransmission(hitPoint, localFixedDir,
			kt, nc, nt, &transLocalSampledDir);
			
	const float localFilmThickness = (material->archglass.filmThicknessTexIndex != NULL_INDEX) 
									 ? Texture_GetFloatValue(material->archglass.filmThicknessTexIndex, hitPoint TEXTURES_PARAM) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && material->archglass.filmIorTexIndex != NULL_INDEX) 
							   ? Texture_GetFloatValue(material->archglass.filmIorTexIndex, hitPoint TEXTURES_PARAM) : 1.f;
	
	float3 reflLocalSampledDir;
	const float3 refl = ArchGlassMaterial_EvalSpecularReflection(hitPoint, localFixedDir,
			kr, nc, nt, &reflLocalSampledDir, localFilmThickness, localFilmIor);

	// Decide to transmit or reflect
	float3 transp;
	if (!Spectrum_IsBlack(refl)) {
		if (!Spectrum_IsBlack(trans)) {
			// Importance sampling
			const float reflFilter = Spectrum_Filter(refl);
			const float transFilter = Spectrum_Filter(trans);
			const float threshold = transFilter / (reflFilter + transFilter);
			
			if (passThroughEvent < threshold) {
				// Transmit
				transp = trans / threshold;
			} else {
				// Reflect
				transp = BLACK;
			}
		} else
			transp = BLACK;
	} else {
		if (!Spectrum_IsBlack(trans)) {
			// Transmit

			// threshold = 1 so I avoid the / threshold
			transp = trans;
		} else
			transp = BLACK;
	}

	EvalStack_PushFloat3(transp);
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	MATERIAL_EVALUATE_RETURN_BLACK;
}

OPENCL_FORCE_INLINE void ArchGlassMaterial_Sample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	const float3 ktTexVal = Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint TEXTURES_PARAM);
	const float3 krTexVal = Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kt = Spectrum_Clamp(ktTexVal);
	const float3 kr = Spectrum_Clamp(krTexVal);

	const float nc = ExtractExteriorIors(hitPoint, material->archglass.exteriorIorTexIndex TEXTURES_PARAM);
	const float nt = ExtractInteriorIors(hitPoint, material->archglass.interiorIorTexIndex TEXTURES_PARAM);

	float3 transLocalSampledDir; 
	const float3 trans = ArchGlassMaterial_EvalSpecularTransmission(hitPoint, fixedDir,
			kt, nc, nt, &transLocalSampledDir);
	
	const float localFilmThickness = (material->archglass.filmThicknessTexIndex != NULL_INDEX) 
									 ? Texture_GetFloatValue(material->archglass.filmThicknessTexIndex, hitPoint TEXTURES_PARAM) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && material->archglass.filmIorTexIndex != NULL_INDEX) 
							   ? Texture_GetFloatValue(material->archglass.filmIorTexIndex, hitPoint TEXTURES_PARAM) : 1.f;
	float3 reflLocalSampledDir;
	const float3 refl = ArchGlassMaterial_EvalSpecularReflection(hitPoint, fixedDir,
			kr, nc, nt, &reflLocalSampledDir, localFilmThickness, localFilmIor);

	// Decide to transmit or reflect
	float threshold;
	if (!Spectrum_IsBlack(refl)) {
		if (!Spectrum_IsBlack(trans)) {
			// Importance sampling
			const float reflFilter = Spectrum_Filter(refl);
			const float transFilter = Spectrum_Filter(trans);
			threshold = transFilter / (reflFilter + transFilter);
		} else
			threshold = 0.f;
	} else {
		// ArchGlassMaterial::Sample() can be called only if ArchGlassMaterial::GetPassThroughTransparency()
		// has detected a reflection or a mixed reflection/transmission.
		// Here, there was no reflection at all so I return black.
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	float3 sampledDir;
	BSDFEvent event;
	float pdfW;
	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit

		sampledDir = transLocalSampledDir;

		event = SPECULAR | TRANSMIT;
		pdfW = threshold;
		
		result = trans;
	} else {
		// Reflect
		sampledDir = reflLocalSampledDir;

		event = SPECULAR | REFLECT;
		pdfW = 1.f - threshold;

		result = refl;
	}
	result /= pdfW;

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void ArchGlassMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			ArchGlassMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			ArchGlassMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			ArchGlassMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			ArchGlassMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			ArchGlassMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			ArchGlassMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			ArchGlassMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
