#line 2 "materialdefs_funcs_archglass.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#if defined (PARAM_ENABLE_MAT_ARCHGLASS)

OPENCL_FORCE_INLINE BSDFEvent ArchGlassMaterial_GetEventTypes() {
	return SPECULAR | REFLECT | TRANSMIT;
}

OPENCL_FORCE_INLINE bool ArchGlassMaterial_IsDelta() {
	return true;
}

OPENCL_FORCE_NOT_INLINE float3 ArchGlassMaterial_EvalSpecularReflection(__global const HitPoint *hitPoint,
		const float3 localFixedDir, const float3 kr,
		const float nc, const float nt,
		float3 *localSampledDir) {
	if (Spectrum_IsBlack(kr))
		return BLACK;

	const float costheta = CosTheta(localFixedDir);
	if (costheta <= 0.f)
		return BLACK;

	*localSampledDir = (float3)(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);

	const float ntc = nt / nc;
	return kr * FresnelCauchy_Evaluate(ntc, costheta);
}

OPENCL_FORCE_NOT_INLINE float3 ArchGlassMaterial_EvalSpecularTransmission(__global const HitPoint *hitPoint,
		const float3 localFixedDir, const float3 kt,
		const float nc, const float nt, float3 *localSampledDir) {
	if (Spectrum_IsBlack(kt))
		return BLACK;

	// Note: there can not be total internal reflection for 
	
	*localSampledDir = -localFixedDir;

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

OPENCL_FORCE_NOT_INLINE float3 ArchGlassMaterial_GetPassThroughTransparency(__global const Material *material,
		__global const HitPoint *hitPoint, const float3 localFixedDir,
		const float passThroughEvent, const bool backTracing
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint
		TEXTURES_PARAM));

	const float nc = Spectrum_Filter(ExtractExteriorIors(hitPoint,
			material->archglass.exteriorIorTexIndex
			TEXTURES_PARAM));
	const float nt = Spectrum_Filter(ExtractInteriorIors(hitPoint,
			material->archglass.interiorIorTexIndex
			TEXTURES_PARAM));

	float3 transLocalSampledDir; 
	const float3 trans = ArchGlassMaterial_EvalSpecularTransmission(hitPoint, localFixedDir,
			kt, nc, nt, &transLocalSampledDir);
	
	float3 reflLocalSampledDir;
	const float3 refl = ArchGlassMaterial_EvalSpecularReflection(hitPoint, localFixedDir,
			kr, nc, nt, &reflLocalSampledDir);

	// Decide to transmit or reflect
	float threshold;
	if (!Spectrum_IsBlack(refl)) {
		if (!Spectrum_IsBlack(trans)) {
			// Importance sampling
			const float reflFilter = Spectrum_Filter(refl);
			const float transFilter = Spectrum_Filter(trans);
			threshold = transFilter / (reflFilter + transFilter);
			
			if (passThroughEvent < threshold) {
				// Transmit
				return trans / threshold;
			} else {
				// Reflect
				return BLACK;
			}
		} else
			return BLACK;
	} else {
		if (!Spectrum_IsBlack(trans)) {
			// Transmit

			// threshold = 1 so I avoid the / threshold
			return trans;
		} else
			return BLACK;
	}
}

OPENCL_FORCE_INLINE float3 ArchGlassMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 ktTexVal, const float3 krTexVal,
		const float nc, const float nt) {
	return BLACK;
}

OPENCL_FORCE_NOT_INLINE float3 ArchGlassMaterial_Sample(
		__global const HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float3 ktTexVal, const float3 krTexVal,
		const float nc, const float nt) {
	const float3 kt = Spectrum_Clamp(ktTexVal);
	const float3 kr = Spectrum_Clamp(krTexVal);

	float3 transLocalSampledDir; 
	const float3 trans = ArchGlassMaterial_EvalSpecularTransmission(hitPoint, localFixedDir,
			kt, nc, nt, &transLocalSampledDir);
	
	float3 reflLocalSampledDir;
	const float3 refl = ArchGlassMaterial_EvalSpecularReflection(hitPoint, localFixedDir,
			kr, nc, nt, &reflLocalSampledDir);

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
		return BLACK;
	}

	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit

		*localSampledDir = transLocalSampledDir;

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;
		
		result = trans;
	} else {
		// Reflect
		*localSampledDir = reflLocalSampledDir;

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = refl;
	}

	return result / *pdfW;
}

#endif
