#line 2 "materialdefs_funcs_null.cl"

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
// NULL material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_NULL)

OPENCL_FORCE_INLINE BSDFEvent NullMaterial_GetEventTypes() {
	return SPECULAR | TRANSMIT;
}

OPENCL_FORCE_INLINE bool NullMaterial_IsDelta() {
	return true;
}

OPENCL_FORCE_NOT_INLINE float3 NullMaterial_GetPassThroughTransparency(__global const Material *material,
		__global const HitPoint *hitPoint, const float3 localFixedDir,
		const float passThroughEvent, const bool backTracing
		TEXTURES_PARAM_DECL) {
	const uint transpTexIndex = (hitPoint->intoObject != backTracing) ?
		material->frontTranspTexIndex : material->backTranspTexIndex;

	if (transpTexIndex != NULL_INDEX) {
		const float3 blendColor = clamp(
			Texture_GetSpectrumValue(transpTexIndex, hitPoint
				TEXTURES_PARAM),
			0.f, 1.f);

		if (Spectrum_IsBlack(blendColor)) {
			// It doesn't make any sense to have a solid NULL material
			return .0001f;
		} else
			return blendColor;
	} else
		return WHITE;
}

OPENCL_FORCE_INLINE float3 NullMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW) {
	return BLACK;
}

OPENCL_FORCE_INLINE float3 NullMaterial_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event) {
	*sampledDir = -fixedDir;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return WHITE;
}

#endif
